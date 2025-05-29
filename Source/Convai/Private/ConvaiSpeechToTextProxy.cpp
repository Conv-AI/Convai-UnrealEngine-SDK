// Copyright 2022 Convai Inc. All Rights Reserved.


#include "ConvaiSpeechToTextProxy.h"
#include "ConvaiUtils.h"
#include "Sound/SoundWave.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "AudioDecompress.h"
#include "Engine.h"
#include "JsonObjectConverter.h"
#include "../Convai.h"

namespace
{	
	static FString TextToSpeechURL() { return UConvaiURL::GetFullURL(TEXT("tts"), false); }
	static FString SpeechToTextURL() { return UConvaiURL::GetFullURL(TEXT("stt/"), false); }
}


DEFINE_LOG_CATEGORY(ConvaiS2THttpLog);

UConvaiSpeechToTextProxy* UConvaiSpeechToTextProxy::CreateSpeech2TextFromFileNameQueryProxy(UObject* WorldContextObject, FString filename)
{
	UConvaiSpeechToTextProxy* Proxy = NewObject<UConvaiSpeechToTextProxy>();
	Proxy->WorldPtr = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	Proxy->URL = SpeechToTextURL();

	if (!FPaths::FileExists(filename))
	{
		// check if the file is relative to the save/BouncedWavFiles directory
		FString SaveDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
		filename = FPaths::Combine(SaveDir, FString("BouncedWavFiles"), filename);
		if (!FPaths::FileExists(filename))
		{
			//if (!FPaths::GameAgnosticSavedDir)
			CONVAI_LOG(ConvaiS2THttpLog, Warning, TEXT("File does not exist!, %s"), *filename);
			Proxy->failed();
			return nullptr;
		}
	}
	
	// Read the file into a byte array
	FFileHelper::LoadFileToArray(Proxy->Payload, *filename, 0);

	Proxy->bStereo = true;
	return Proxy;
}

UConvaiSpeechToTextProxy* UConvaiSpeechToTextProxy::CreateSpeech2TextFromSoundWaveQueryProxy(UObject* WorldContextObject, USoundWave* SoundWave)
{
	UConvaiSpeechToTextProxy* Proxy = NewObject<UConvaiSpeechToTextProxy>();
	Proxy->WorldPtr = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	Proxy->URL = SpeechToTextURL();

	if (SoundWave == nullptr)
	{
		CONVAI_LOG(ConvaiS2THttpLog, Warning, TEXT("Sound wave is invalid!"));
		Proxy->failed();
		return nullptr;
	}


	//Proxy->Payload.SetNum(SoundWave->TotalSamples * 2);
	uint8* PCMData = nullptr;
	TArray<uint8> RawPCMData;
	int32 OutSampleRate = -1;
	int32 outNumChannels = -1;
	//int32 numBytes = SoundWave->GeneratePCMData(PCMData, SoundWave->TotalSamples);

	if (SoundWave->RawPCMData == nullptr || SoundWave->RawPCMDataSize <= 0) {
		CONVAI_LOG(LogTemp, Display, TEXT("SoundWave PCM Data is compressed. Starting Decompressing....."));
		RawPCMData = UConvaiUtils::ExtractPCMDataFromSoundWave(SoundWave, OutSampleRate, outNumChannels);

		if (RawPCMData.Num() > 0) {
			CONVAI_LOG(LogTemp, Display, TEXT("SoundWave PCM Data decompression successfully done....."));
		}
		else {
			CONVAI_LOG(LogTemp,Warning,TEXT("SoundWave couldn't be decompressed successuflly !!!"));
		}
		//CONVAI_LOG(ConvaiS2THttpLog, Warning, TEXT("RawPCMData is invalid!"));
	}
	else {
		RawPCMData = TArray<uint8>(SoundWave->RawPCMData, SoundWave->RawPCMDataSize);
	}

	//TArray<uint8> AudioBuffer(SoundWave->RawPCMData, SoundWave->RawPCMDataSize);

	SerializeWaveFile(Proxy->Payload, RawPCMData.GetData(), RawPCMData.Num(), SoundWave->NumChannels, SoundWave->GetSampleRateForCurrentPlatform());

	//CONVAI_LOG(ConvaiS2THttpLog, Warning, TEXT("Sound wave sample rate: %f"), SoundWave->GetSampleRateForCurrentPlatform());
	Proxy->bStereo = SoundWave->NumChannels>1? true : false;
	return Proxy;
}

UConvaiSpeechToTextProxy* UConvaiSpeechToTextProxy::CreateSpeech2TextFromArrayQueryProxy(UObject* WorldContextObject, TArray<uint8> Payload)
{
	UConvaiSpeechToTextProxy* Proxy = NewObject<UConvaiSpeechToTextProxy>();
	Proxy->WorldPtr = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	Proxy->URL = SpeechToTextURL();
	Proxy->Payload = Payload;
	Proxy->bStereo = true;
	return Proxy;
}


void UConvaiSpeechToTextProxy::Activate()
{
	UWorld* World = WorldPtr.Get();

	if (!World)
	{
		CONVAI_LOG(ConvaiS2THttpLog, Warning, TEXT("Could not get a pointer to world!"));
		failed();
		return;
	}

	FHttpModule* Http = &FHttpModule::Get();
	if (!Http)
	{
		CONVAI_LOG(ConvaiS2THttpLog, Warning, TEXT("Could not get a pointer to http module!"));
		failed();
		return;
	}

	if (Payload.Num() <= 44)
	{
		CONVAI_LOG(ConvaiS2THttpLog, Warning, TEXT("Payload size is too small, %d bytes!"), Payload.Num());
		failed();
		return;
	}


	TPair<FString, FString> AuthHeaderAndKey = UConvaiUtils::GetAuthHeaderAndKey();
	FString AuthKey = AuthHeaderAndKey.Value;
	FString AuthHeader = AuthHeaderAndKey.Key;

	// Form Validation
	if (!UConvaiFormValidation::ValidateAuthKey(AuthKey) || !UConvaiFormValidation::ValidateInputVoice(Payload))
	{
		failed();
		return;
	}

	TArray<uint8> monoWavBytes;
	if (bStereo)
	{
		// Change the wav file from 2 channels to 1 channel
		UConvaiUtils::StereoToMono(Payload, monoWavBytes);
	}
	else
	{
		monoWavBytes = Payload;
	}

	// Create the request
	FHttpRequestRef Request = Http->CreateRequest();
	Request->OnProcessRequestComplete().BindUObject(this, &UConvaiSpeechToTextProxy::onHttpRequestComplete);

	// Set request fields
	Request->SetURL(URL);
	Request->SetVerb("POST");
	Request->SetHeader(TEXT("User-Agent"), TEXT("X-UnrealEngine-Agent"));

	Request->SetHeader("Content-Type", "multipart/form-data; boundary=blahblahsomeboundary");
	Request->SetHeader(AuthHeader, AuthKey);

	// prepare request content data
	//FString a = "\r\n--blahblahsomeboundary\r\n";
	//FString b = "Content-Disposition: form-data; name=\"Request->SetHeader(AuthHeader, AuthKey);\"\r\n\r\n";
	// Request->SetHeader(AuthHeader, AuthKey);
	FString c = "\r\n--blahblahsomeboundary\r\n";
	FString d = "Content-Disposition: form-data; name=\"file\";  filename=\"out.wav\"\r\n\r\n";

	// UpFileRawData
	FString e = "\r\n--blahblahsomeboundary--\r\n";

	TArray<uint8> data;
	//data.Append((uint8*)TCHAR_TO_UTF8(*a), a.Len());
	//data.Append((uint8*)TCHAR_TO_UTF8(*b), b.Len());
	//data.Append((uint8*)TCHAR_TO_UTF8(*Request->SetHeader(AuthHeader, AuthKey);), Request->SetHeader(AuthHeader, AuthKey);.Len());
	data.Append((uint8*)TCHAR_TO_UTF8(*c), c.Len());
	data.Append((uint8*)TCHAR_TO_UTF8(*d), d.Len());
	data.Append(monoWavBytes);
	data.Append((uint8*)TCHAR_TO_UTF8(*e), e.Len());


	Request->SetContent(data);

	// Run the request
	if (!Request->ProcessRequest()) failed();
}


void UConvaiSpeechToTextProxy::onHttpRequestComplete(FHttpRequestPtr RequestPtr, FHttpResponsePtr ResponsePtr, bool bWasSuccessful)
{
	if (!bWasSuccessful || ResponsePtr->GetResponseCode() < 200 || ResponsePtr->GetResponseCode() > 299)
	{
		CONVAI_LOG(ConvaiS2THttpLog, Warning, TEXT("HTTP request failed with code %d"), ResponsePtr->GetResponseCode());
		CONVAI_LOG(ConvaiS2THttpLog, Warning, TEXT("Response:%s"), *ResponsePtr->GetContentAsString());

		this->Response = ResponsePtr->GetContentAsString();
		failed();
		return;
	}

	this->Response = ResponsePtr->GetContentAsString();
	// Clean the string
	this->Response = this->Response.LeftChop(3);
	this->Response = this->Response.RightChop(12);
	success();
}


void UConvaiSpeechToTextProxy::failed()
{
	OnFailure.Broadcast(Response);
	finish();
}


void UConvaiSpeechToTextProxy::success()
{
	OnSuccess.Broadcast(Response);
	finish();
}


void UConvaiSpeechToTextProxy::finish()
{
}

