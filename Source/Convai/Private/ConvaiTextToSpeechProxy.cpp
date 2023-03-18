// Copyright 2022 Convai Inc. All Rights Reserved.

#include "ConvaiTextToSpeechProxy.h"
#include "ConvaiUtils.h"
#include "Containers/UnrealString.h"
#include "Sound/SoundWave.h"
#include "Engine.h"
#include "JsonObjectConverter.h"

#include "../Convai.h"

DEFINE_LOG_CATEGORY(ConvaiT2SHttpLog);


namespace {
	const char* TTS_Voice_Type_str[] = {
		"MALE",
		"FEMALE",
		"WUKMale 1",
		"WUKFemale 1",
		"SUKMale 1",
		"WAFemale 1",
		"WAMale 1",
		"SIFemale 1",
		"SIMale 1",
		"SUFemale 1",
		"SUMale 1",
		"WUFemale 1",
		"WUMale 1",
		"Trixie",
		"Twilight Sparkle",
		"Celestia",
		"Spike",
		"Applejack"
	};
};


UConvaiTextToSpeechProxy* UConvaiTextToSpeechProxy::CreateTextToSpeechQueryProxy(UObject* WorldContextObject, FString Transcript, FString Voice)
{
	UConvaiTextToSpeechProxy* Proxy = NewObject<UConvaiTextToSpeechProxy>();
	Proxy->WorldPtr = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	Proxy->URL = "https://api.convai.com/tts";
	Proxy->Transcript = Transcript;
	Proxy->VoiceStr = Voice;
	//Proxy->VoiceStr = FString(TTS_Voice_Type_str[uint8(Voice)]);
	
	Proxy->API_key = Convai::Get().GetConvaiSettings()->API_Key;
	return Proxy;
}



void UConvaiTextToSpeechProxy::Activate()
{
	UWorld* World = WorldPtr.Get();

	if (!World)
	{
		UE_LOG(ConvaiT2SHttpLog, Warning, TEXT("Could not get a pointer to world!"));
		failed();
		return;
	}

	FHttpModule* Http = &FHttpModule::Get();
	if (!Http)
	{
		UE_LOG(ConvaiT2SHttpLog, Warning, TEXT("Could not get a pointer to http module!"));
		failed();
		return;
	}

	// Form Validation
	if (!UConvaiFormValidation::ValidateAPIKey(API_key) || !UConvaiFormValidation::ValidateInputText(Transcript) || !UConvaiFormValidation::ValidateVoiceType(VoiceStr))
	{
		failed();
		return;
	}

	// Create the request
	FHttpRequestRef Request = Http->CreateRequest();
	Request->OnProcessRequestComplete().BindUObject(this, &UConvaiTextToSpeechProxy::onHttpRequestComplete);

	// Set request fields
	Request->SetURL(URL);
	Request->SetVerb("POST");
	Request->SetHeader(TEXT("User-Agent"), TEXT("X-UnrealEngine-Agent"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("CONVAI-API-KEY"), *API_key);

	// prepare json data
	FString JsonString;
	TSharedRef<TJsonWriter<TCHAR>> JsonWriter = TJsonWriterFactory<TCHAR>::Create(&JsonString);
	JsonWriter->WriteObjectStart();
	JsonWriter->WriteValue("api_key", API_key);
	JsonWriter->WriteValue("transcript", Transcript);
	JsonWriter->WriteValue("voice", VoiceStr);
	JsonWriter->WriteValue("filename", FString("testAudio"));
	JsonWriter->WriteValue("encoding", (VoiceStr.ToLower() == "female" || VoiceStr.ToLower() == "male")? FString("wav") : FString("mp3"));
	JsonWriter->WriteObjectEnd();
	JsonWriter->Close();
	// Insert the content into the request
	Request->SetContentAsString(JsonString);

	//int Count = Request->GetContent().Num();
	//const uint8* In = Request->GetContent().GetData();
	//FString Result;
	//Result.Empty();

	//while (Count)
	//{
	//	int16 Value = *In;

	//	Result += TCHAR(Value);

	//	++In;
	//	Count--;
	//}

	//UE_LOG(ConvaiT2SHttpLog, Warning, TEXT("%s"), *Result);

	// Initiate the request
	if (!Request->ProcessRequest()) failed();
}


void UConvaiTextToSpeechProxy::onHttpRequestComplete(FHttpRequestPtr RequestPtr, FHttpResponsePtr ResponsePtr, bool bWasSuccessful)
{
	if (!bWasSuccessful || ResponsePtr->GetResponseCode() < 200 || ResponsePtr->GetResponseCode() > 299)
	{
		UE_LOG(ConvaiT2SHttpLog, Warning, TEXT("HTTP request failed with code %d, and with response:%s"),ResponsePtr->GetResponseCode(), *ResponsePtr->GetContentAsString());
		failed();
		return;
	}

	if (VoiceStr.ToLower() == "female" || VoiceStr.ToLower() == "male")
	{
		this->SoundWave = UConvaiUtils::WavDataToSoundWave(ResponsePtr->GetContent()); // use this if in WAV format
	}
	else
	{
		this->SoundWave = UConvaiUtils::DecodeMP3(ResponsePtr->GetContent()); // use this if the received content is in MP3 Format
	}
	//this->SoundWave = UConvaiUtils::PCMDataToSoundWav(ResponsePtr->GetContent(), 1, 44100); // use this if in PCM format
	if (this->SoundWave == nullptr)
	{
		UE_LOG(ConvaiT2SHttpLog, Warning, TEXT("Failed to decode response content to a sound wave"));
		failed();
		return;
	}
	success();
}



void UConvaiTextToSpeechProxy::failed()
{
	OnFailure.Broadcast(SoundWave);
	finish();
}


void UConvaiTextToSpeechProxy::success()
{
	OnSuccess.Broadcast(SoundWave);
	finish();
}


void UConvaiTextToSpeechProxy::finish()
{
}

