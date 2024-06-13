// Copyright 2022 Convai Inc. All Rights Reserved.

#include "ConvaiChatBotProxy.h"
#include "Sound/SoundWave.h"
#include "../Convai.h"
#include "ConvaiDefinitions.h"
#include "ConvaiUtils.h"
#include "Containers/UnrealString.h"
#include "Sound/SoundWave.h"
#include "Misc/Base64.h"
#include "Engine.h"
#include "JsonObjectConverter.h"

#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "TextureResource.h"
#include "Engine/Texture2D.h"


DEFINE_LOG_CATEGORY(ConvaiBotHttpLog);

namespace
{
	void AddURLPair(FString &Data, FString param, FString value)
	{
		Data += ("&" + param + "=" + value);
	}
}

UConvaiChatBotQueryProxy* UConvaiChatBotQueryProxy::CreateChatBotQueryProxy(UObject* WorldContextObject,
	FString UserQuery,
	bool VoiceResponse,
	FString CharID,
	FString SessionID,
	bool Classification,
	TArray<FString> ClassLabels)
{
	UConvaiChatBotQueryProxy* Proxy = NewObject<UConvaiChatBotQueryProxy>();
	Proxy->WorldPtr = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	Proxy->URL = "https://api.convai.com/character/getResponse/";
	Proxy->UserQuery = UserQuery;
	Proxy->VoiceResponse = VoiceResponse;
	Proxy->CharID = CharID;
	Proxy->SessionID = SessionID;
	Proxy->API_key = Convai::Get().GetConvaiSettings()->API_Key;
	Proxy->Classification = Classification;
	Proxy->ClassLabels = ClassLabels;
	return Proxy;
}


void UConvaiChatBotQueryProxy::Activate()
{
	//UE_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request failed with code %d"), sum(1,6));

	UWorld* World = WorldPtr.Get();

	if (!World)
	{
		UE_LOG(ConvaiBotHttpLog, Warning, TEXT("Could not get a pointer to world!"));
		failed();
		return;
	}

	FHttpModule* Http = &FHttpModule::Get();
	if (!Http)
	{
		UE_LOG(ConvaiBotHttpLog, Warning, TEXT("Could not get a pointer to http module!"));
		failed();
		return;
	}

	// Form Validation
	if (
		!UConvaiFormValidation::ValidateAPIKey(API_key)
		|| !UConvaiFormValidation::ValidateInputText(UserQuery)
		|| !UConvaiFormValidation::ValidateCharacterID(CharID)
		|| !UConvaiFormValidation::ValidateSessionID(SessionID)
		)
	{
		failed();
		return;
	}


	// Create the request
	FHttpRequestRef Request = Http->CreateRequest();
	Request->OnProcessRequestComplete().BindUObject(this, &UConvaiChatBotQueryProxy::onHttpRequestComplete);

	// Set request fields
	Request->SetURL(URL);
	Request->SetVerb("POST");
	Request->SetHeader(TEXT("User-Agent"), TEXT("X-UnrealEngine-Agent"));
	Request->SetHeader(TEXT("CONVAI-API-KEY"), API_key);
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/x-www-form-urlencoded"));

	// Prepare x-www-form-urlencoded data
	FString Data;
	AddURLPair(Data, "charID", CharID);
	AddURLPair(Data, "sessionID", SessionID);
	AddURLPair(Data, "responseLevel", FString("5"));
	AddURLPair(Data, "voiceResponse", VoiceResponse? FString("True") : FString("False"));
	AddURLPair(Data, "userText", UserQuery);
	AddURLPair(Data, "classification", Classification? FString("True") : FString("False"));
	AddURLPair(Data, "classLabels", FString::Join(ClassLabels, *FString(",")));
	
	Request->SetContentAsString(Data);

	//// prepare json data
	//FString JsonString;
	//TSharedRef<TJsonWriter<TCHAR>> JsonWriter = TJsonWriterFactory<TCHAR>::Create(&JsonString);

	//JsonWriter->WriteObjectStart();
	//JsonWriter->WriteValue("charID", CharID);
	////JsonWriter->WriteValue("api_key", API_key);
	//JsonWriter->WriteValue("sessionID", SessionID);
	////JsonWriter->WriteValue("responseLevel", FString("5"));
	////if (VoiceResponse)
	//JsonWriter->WriteValue("voiceResponse", VoiceResponse);
	//JsonWriter->WriteValue("userText", UserQuery);

	//JsonWriter->WriteObjectEnd();
	//JsonWriter->Close();

	//Request->SetContentAsString(JsonString);
	

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

	//UE_LOG(ConvaiBotHttpLog, Warning, TEXT("Content: %s"), *JsonString);
	//UE_LOG(ConvaiBotHttpLog, Warning, TEXT("Content: %s"), *Result);

	// Run the request
	if (!Request->ProcessRequest()) failed();
}


void UConvaiChatBotQueryProxy::onHttpRequestComplete(FHttpRequestPtr RequestPtr, FHttpResponsePtr ResponsePtr, bool bWasSuccessful)
{
	if (!ResponsePtr)
	{
		if (bWasSuccessful)
		{
			UE_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request succeded - But response pointer is invalid"));
		}
		else
		{
			UE_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request failed - Response pointer is invalid"));
		}

		failed();
		return;
	}
	if (!bWasSuccessful || ResponsePtr->GetResponseCode() < 200 || ResponsePtr->GetResponseCode() > 299)
	{
		UE_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request failed with code %d"), ResponsePtr->GetResponseCode());
		UE_LOG(ConvaiBotHttpLog, Warning, TEXT("Response:%s"), *ResponsePtr->GetContentAsString());

		failed();
		return;
	}

	FString Response = ResponsePtr->GetContentAsString();
	//UE_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP Response: %s"), *Response);

	TSharedPtr<FJsonValue> JsonValue;
	// Create a reader pointer to read the json data
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response);

	FString AudioContentString, SampleRateStr;
	if (FJsonSerializer::Deserialize(Reader, JsonValue)) {
		// Get the value of the json object by field name
		BotText = JsonValue->AsObject()->GetStringField("text");
		NewSessionID = JsonValue->AsObject()->GetStringField("sessionID");
		if (VoiceResponse)
		{
			AudioContentString = JsonValue->AsObject()->GetStringField("audio");
			SampleRateStr = JsonValue->AsObject()->GetStringField("sample_rate");
			TArray<uint8> OutByteArray;
			if (!FBase64::Decode(AudioContentString, OutByteArray))
			{
				UE_LOG(ConvaiBotHttpLog, Warning, TEXT("Could not decode audio content, Response:%s"), *Response);
				failed();
				return;
			}

			// this->AudioContent = UConvaiUtils::PCMDataToSoundWav(OutByteArray, 1, FCString::Atoi(*SampleRateStr));
			this->AudioContent = UConvaiUtils::WavDataToSoundWave(OutByteArray);

			if (this->AudioContent == nullptr)
			{
				UE_LOG(ConvaiBotHttpLog, Warning, TEXT("Failed to decode response content to a sound wave"));
				failed();
				return;
			}

		}

		if (BotText.Len() == 0 || NewSessionID.Len() == 0 || (AudioContentString.Len() == 0 && VoiceResponse))
		{
			UE_LOG(ConvaiBotHttpLog, Warning, TEXT("One or more expected inputs was not received, Response:%s"), *Response);
			//failed();
			//return;
		}
		// Find classified actions in response
		ClassifiedAction = "";
		if (Classification)
		{
			ClassifiedAction = JsonValue->AsObject()->GetStringField("tag");
		}
	}
	else
	{
		UE_LOG(ConvaiBotHttpLog, Warning, TEXT("Could not deserialize response into Json, Response:%s"), *Response);
		failed();
		return;
	}

	success();
}


void UConvaiChatBotQueryProxy::failed()
{
	OnFailure.Broadcast(AudioContent, BotText, NewSessionID, ClassifiedAction);
	finish();
}


void UConvaiChatBotQueryProxy::success()
{
	OnSuccess.Broadcast(AudioContent, BotText, NewSessionID, ClassifiedAction);
	finish();
}


void UConvaiChatBotQueryProxy::finish()
{
}



UConvaiChatBotQueryFromAudioProxy* UConvaiChatBotQueryFromAudioProxy::CreateChatBotQueryFromAudioProxy(
	UObject* WorldContextObject,
	FString Filename,
	bool VoiceResponse,
	FString CharID,
	FString SessionID,
	bool Classification,
	TArray<FString> ClassLabels)
{
	UConvaiChatBotQueryFromAudioProxy* Proxy = NewObject<UConvaiChatBotQueryFromAudioProxy>();
	Proxy->WorldPtr = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	Proxy->URL = "https://api.convai.com/character/getResponse";

	if (!FPaths::FileExists(Filename))
	{
		// check if the file is relative to the save/BouncedWavFiles directory
		FString SaveDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
		Filename = FPaths::Combine(SaveDir, FString("BouncedWavFiles"), Filename);
		if (!FPaths::FileExists(Filename))
		{
			//if (!FPaths::GameAgnosticSavedDir)
			UE_LOG(ConvaiBotHttpLog, Warning, TEXT("File does not exist!, %s"), *Filename);
			Proxy->failed();
			return nullptr;
		}
	}

	// Read the file into a byte array
	FFileHelper::LoadFileToArray(Proxy->Payload, *Filename, 0);

	Proxy->VoiceResponse = VoiceResponse;
	Proxy->CharID = CharID;
	Proxy->SessionID = SessionID;
	Proxy->API_key = Convai::Get().GetConvaiSettings()->API_Key;
	Proxy->Classification = Classification;
	Proxy->ClassLabels = ClassLabels;

	return Proxy;
}


UConvaiChatBotQueryFromAudioProxy* UConvaiChatBotQueryFromAudioProxy::CreateChatBotQueryFromSoundWavProxy(
	UObject* WorldContextObject,
	USoundWave* SoundWave,
	bool VoiceResponse,
	FString CharID,
	FString SessionID,
	bool Classification,
	TArray<FString> ClassLabels)
{
	UConvaiChatBotQueryFromAudioProxy* Proxy = NewObject<UConvaiChatBotQueryFromAudioProxy>();
	Proxy->WorldPtr = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	Proxy->URL = "https://api.convai.com/character/getResponse";

	if (SoundWave == nullptr)
	{
		UE_LOG(ConvaiBotHttpLog, Warning, TEXT("Sound wave is invalid!"));
		Proxy->failed();
		return nullptr;
	}


	//Proxy->Payload.SetNum(SoundWave->TotalSamples * 2);
	uint8* PCMData = nullptr;
	//int32 numBytes = SoundWave->GeneratePCMData(PCMData, SoundWave->TotalSamples);

	if (SoundWave->RawPCMData == nullptr) {
		UE_LOG(ConvaiBotHttpLog, Warning, TEXT("RawPCMData is invalid!"));
	}

	//TArray<uint8> AudioBuffer(SoundWave->RawPCMData, SoundWave->RawPCMDataSize);
	TArray<uint8> RawPCMData;
	RawPCMData = TArray<uint8>(SoundWave->RawPCMData, SoundWave->RawPCMDataSize);

	SerializeWaveFile(Proxy->Payload, RawPCMData.GetData(), RawPCMData.Num(), SoundWave->NumChannels, SoundWave->GetSampleRateForCurrentPlatform());

	Proxy->VoiceResponse = VoiceResponse;
	Proxy->CharID = CharID;
	Proxy->SessionID = SessionID;
	Proxy->API_key = Convai::Get().GetConvaiSettings()->API_Key;
	Proxy->Classification = Classification;
	Proxy->ClassLabels = ClassLabels;

	return Proxy;
}

void UConvaiChatBotQueryFromAudioProxy::Activate()
{
	//UE_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request failed with code %d"), sum(1,6));

	UWorld* World = WorldPtr.Get();

	if (!World)
	{
		UE_LOG(ConvaiBotHttpLog, Warning, TEXT("Could not get a pointer to world!"));
		failed();
		return;
	}

	FHttpModule* Http = &FHttpModule::Get();
	if (!Http)
	{
		UE_LOG(ConvaiBotHttpLog, Warning, TEXT("Could not get a pointer to http module!"));
		failed();
		return;
	}

	if (Payload.Num() <= 44)
	{
		UE_LOG(ConvaiBotHttpLog, Warning, TEXT("Payload size is too small, %d bytes!"), Payload.Num());
		failed();
		return;
	}

	// Form Validation
	if (
		!UConvaiFormValidation::ValidateAPIKey(API_key)
		|| !UConvaiFormValidation::ValidateInputVoice(Payload)
		|| !UConvaiFormValidation::ValidateCharacterID(CharID)
		|| !UConvaiFormValidation::ValidateSessionID(SessionID)
		)
	{
		failed();
		return;
	}

	TArray<uint8> monoWavBytes;
	// Change the wav file from 2 channels to 1 channel
	UConvaiUtils::StereoToMono(Payload, monoWavBytes);

	FString responseLevel = "5";

	// Create the request
	FHttpRequestRef Request = Http->CreateRequest();
	Request->OnProcessRequestComplete().BindUObject(this, &UConvaiChatBotQueryFromAudioProxy::onHttpRequestComplete);

	// Set request fields
	Request->SetURL(URL);
	Request->SetVerb("POST");
	Request->SetHeader(TEXT("User-Agent"), TEXT("X-UnrealEngine-Agent"));
	Request->SetHeader(TEXT("CONVAI-API-KEY"), API_key);
	Request->SetHeader("Content-Type", "multipart/form-data; boundary=blahblahsomeboundary");

	// Set request fields
	Request->SetURL(URL);
	Request->SetVerb("POST");
	Request->SetHeader(TEXT("User-Agent"), TEXT("X-UnrealEngine-Agent"));

	Request->SetHeader("Content-Type", "multipart/form-data; boundary=blahblahsomeboundary");

	// prepare request content data
	FString a = "\r\n--blahblahsomeboundary\r\n";
	FString b = "Content-Disposition: form-data; name=\"charID\"\r\n\r\n";
	// charID

	FString c = "\r\n--blahblahsomeboundary\r\n";
	FString d = "Content-Disposition: form-data; name=\"sessionID\"\r\n\r\n";
	// sessionID

	FString e = "\r\n--blahblahsomeboundary\r\n";
	FString f = "Content-Disposition: form-data; name=\"responseLevel\"\r\n\r\n";
	// responseLevel

	FString g = "\r\n--blahblahsomeboundary\r\n";
	FString h = "Content-Disposition: form-data; name=\"voiceResponse\"\r\n\r\n";
	// voiceResponse

	FString i = "\r\n--blahblahsomeboundary\r\n";
	FString j = "Content-Disposition: form-data; name=\"classification\"\r\n\r\n";
	// classification

	FString k = "\r\n--blahblahsomeboundary\r\n";
	FString l = "Content-Disposition: form-data; name=\"classLabels\"\r\n\r\n";
	// classificationLabels

	FString m = "\r\n--blahblahsomeboundary\r\n";
	FString n = "Content-Disposition: form-data; name=\"file\";  filename=\"out.wav\"\r\n\r\n";

	// UpFileRawData
	FString o = "\r\n--blahblahsomeboundary--\r\n";

	TArray<uint8> data;
	data.Append((uint8*)TCHAR_TO_UTF8(*a), a.Len());
	data.Append((uint8*)TCHAR_TO_UTF8(*b), b.Len());
	data.Append((uint8*)TCHAR_TO_UTF8(*CharID), CharID.Len());

	data.Append((uint8*)TCHAR_TO_UTF8(*c), c.Len());
	data.Append((uint8*)TCHAR_TO_UTF8(*d), d.Len());
	data.Append((uint8*)TCHAR_TO_UTF8(*SessionID), SessionID.Len());

	data.Append((uint8*)TCHAR_TO_UTF8(*e), e.Len());
	data.Append((uint8*)TCHAR_TO_UTF8(*f), f.Len());
	data.Append((uint8*)TCHAR_TO_UTF8(*responseLevel), responseLevel.Len());

	data.Append((uint8*)TCHAR_TO_UTF8(*g), g.Len());
	data.Append((uint8*)TCHAR_TO_UTF8(*h), h.Len());
	data.Append((uint8*)TCHAR_TO_UTF8(VoiceResponse ? *FString("True") : *FString("False")), VoiceResponse ? 4 : 5);

	data.Append((uint8*)TCHAR_TO_UTF8(*i), i.Len());
	data.Append((uint8*)TCHAR_TO_UTF8(*j), j.Len());
	data.Append((uint8*)TCHAR_TO_UTF8(Classification ? *FString("True") : *FString("False")), Classification ? 4 : 5);

	if (Classification && ClassLabels.Num() > 0)
	{
		FString ClassLabelsStr = FString::Join(ClassLabels, *FString(","));
		data.Append((uint8*)TCHAR_TO_UTF8(*k), k.Len());
		data.Append((uint8*)TCHAR_TO_UTF8(*l), l.Len());
		data.Append((uint8*)TCHAR_TO_UTF8(*ClassLabelsStr), ClassLabelsStr.Len());
	}


	data.Append((uint8*)TCHAR_TO_UTF8(*m), m.Len());
	data.Append((uint8*)TCHAR_TO_UTF8(*n), n.Len());
	data.Append(monoWavBytes);
	//UE_LOG(ConvaiBotHttpLog, Warning, TEXT("%d bytes"), monoWavBytes.Num());

	data.Append((uint8*)TCHAR_TO_UTF8(*o), o.Len());

	Request->SetContent(data);

	// Run the request
	if (!Request->ProcessRequest()) failed();
}


void UConvaiChatBotQueryFromAudioProxy::onHttpRequestComplete(FHttpRequestPtr RequestPtr, FHttpResponsePtr ResponsePtr, bool bWasSuccessful)
{
	if (!ResponsePtr)
	{
		if (bWasSuccessful)
		{
			UE_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request succeded - But response pointer is invalid"));
		}
		else
		{
			UE_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request failed - Response pointer is invalid"));
		}

		failed();
		return;
	}
	if (!bWasSuccessful || ResponsePtr->GetResponseCode() < 200 || ResponsePtr->GetResponseCode() > 299)
	{
		UE_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request failed with code %d, Response:%s"), ResponsePtr->GetResponseCode(), *ResponsePtr->GetContentAsString());
		UE_LOG(ConvaiBotHttpLog, Warning, TEXT("Response:%s"), *ResponsePtr->GetContentAsString());

		failed();
		return;
	}

	FString Response = ResponsePtr->GetContentAsString();
	//UE_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request failed with code %s"), *Response);

	TSharedPtr<FJsonValue> JsonValue;
	// Create a reader pointer to read the json data
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response);

	FString AudioContentString, SampleRateStr;
	if (FJsonSerializer::Deserialize(Reader, JsonValue)) {
		// Get the value of the json object by field name
		BotText = JsonValue->AsObject()->GetStringField("text");
		NewSessionID = JsonValue->AsObject()->GetStringField("sessionID");
		UserQuery = JsonValue->AsObject()->GetStringField("UserQuery");

		if (VoiceResponse)
		{
			AudioContentString = JsonValue->AsObject()->GetStringField("audio");
			SampleRateStr = JsonValue->AsObject()->GetStringField("sample_rate");
			TArray<uint8> OutByteArray;
			if (!FBase64::Decode(AudioContentString, OutByteArray))
			{
				UE_LOG(ConvaiBotHttpLog, Warning, TEXT("Could not decode audio content, Response:%s"), *Response);
				failed();
				return;
			}

			// this->AudioContent = UConvaiUtils::PCMDataToSoundWav(OutByteArray, 1, FCString::Atoi(*SampleRateStr));
			this->AudioContent = UConvaiUtils::WavDataToSoundWave(OutByteArray);

			if (this->AudioContent == nullptr)
			{
				UE_LOG(ConvaiBotHttpLog, Warning, TEXT("Failed to decode response content to a sound wave, Response:%s"), *Response);
				failed();
				return;
			}
			if (BotText.Len() == 0 || UserQuery.Len() == 0 || NewSessionID.Len() == 0 || (AudioContentString.Len() == 0 && VoiceResponse))
			{
				UE_LOG(ConvaiBotHttpLog, Warning, TEXT("One or more expected inputs was not received, Response:%s"), *Response);
				//failed();
				//return;
			}

			// Find classified actions in response
			ClassifiedAction = "";
			if (Classification)
			{
				ClassifiedAction = JsonValue->AsObject()->GetStringField("tag");
			}
		}
	}
	else
	{
		UE_LOG(ConvaiBotHttpLog, Warning, TEXT("Could not deserialize response into Json, Response:%s"), *Response);
		failed();
		return;
	}

	success();
}


void UConvaiChatBotQueryFromAudioProxy::failed()
{
	OnFailure.Broadcast(AudioContent, BotText, UserQuery, NewSessionID, ClassifiedAction);
	finish();
}


void UConvaiChatBotQueryFromAudioProxy::success()
{
	OnSuccess.Broadcast(AudioContent, BotText, UserQuery, NewSessionID, ClassifiedAction);
	finish();
}


void UConvaiChatBotQueryFromAudioProxy::finish()
{
}



UConvaiChatBotCreateProxy* UConvaiChatBotCreateProxy::CreateCharacterCreateProxy(UObject* WorldContextObject, FString CharName, FString Voice, FString Backstory)
{
	UConvaiChatBotCreateProxy* Proxy = NewObject<UConvaiChatBotCreateProxy>();
	Proxy->WorldPtr = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	Proxy->URL = "https://api.convai.com/character/create";
	Proxy->API_key = Convai::Get().GetConvaiSettings()->API_Key;
	Proxy->CharName = CharName;
	Proxy->Voice = Voice;
	Proxy->Backstory = Backstory;
	return Proxy;
}


void UConvaiChatBotCreateProxy::Activate()
{
	//UE_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request failed with code %d"), sum(1,6));

	UWorld* World = WorldPtr.Get();

	if (!World)
	{
		UE_LOG(ConvaiBotHttpLog, Warning, TEXT("Could not get a pointer to world!"));
		failed();
		return;
	}

	FHttpModule* Http = &FHttpModule::Get();
	if (!Http)
	{
		UE_LOG(ConvaiBotHttpLog, Warning, TEXT("Could not get a pointer to http module!"));
		failed();
		return;
	}

	// Form Validation
	if (
		!UConvaiFormValidation::ValidateAPIKey(API_key)
		|| !UConvaiFormValidation::ValidateCharacterName(CharName)
		|| !UConvaiFormValidation::ValidateBackstory(Backstory)
		|| !UConvaiFormValidation::ValidateVoiceType(Voice)
		)
	{
		failed();
		return;
	}

	// Create the request
	FHttpRequestRef Request = Http->CreateRequest();
	Request->OnProcessRequestComplete().BindUObject(this, &UConvaiChatBotCreateProxy::onHttpRequestComplete);

	// Set request fields
	Request->SetURL(URL);
	Request->SetVerb("POST");
	Request->SetHeader(TEXT("User-Agent"), TEXT("X-UnrealEngine-Agent"));
	Request->SetHeader(TEXT("CONVAI-API-KEY"), API_key);
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	// prepare json data
	FString JsonString;
	TSharedRef<TJsonWriter<TCHAR>> JsonWriter = TJsonWriterFactory<TCHAR>::Create(&JsonString);

	JsonWriter->WriteObjectStart();
	JsonWriter->WriteValue("charName", CharName);
	//JsonWriter->WriteValue("api_key", API_key);
	JsonWriter->WriteValue("backstory", Backstory);
	JsonWriter->WriteValue("voiceType", Voice);

	JsonWriter->WriteObjectEnd();
	JsonWriter->Close();

	Request->SetContentAsString(JsonString);

	// Run the request
	if (!Request->ProcessRequest()) failed();
}


void UConvaiChatBotCreateProxy::onHttpRequestComplete(FHttpRequestPtr RequestPtr, FHttpResponsePtr ResponsePtr, bool bWasSuccessful)
{
	if (!ResponsePtr)
	{
		if (bWasSuccessful)
		{
			UE_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request succeded - But response pointer is invalid"));
		}
		else
		{
			UE_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request failed - Response pointer is invalid"));
		}

		failed();
		return;
	}
	if (!bWasSuccessful || ResponsePtr->GetResponseCode() < 200 || ResponsePtr->GetResponseCode() > 299)
	{
		UE_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request failed with code %d, and response:%s"), ResponsePtr->GetResponseCode(), *ResponsePtr->GetContentAsString());
		UE_LOG(ConvaiBotHttpLog, Warning, TEXT("Response:%s"), *ResponsePtr->GetContentAsString());

		failed();
		return;
	}

	FString Response = ResponsePtr->GetContentAsString();

	TSharedPtr<FJsonValue> JsonValue;
	// Create a reader pointer to read the json data
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response);

	if (FJsonSerializer::Deserialize(Reader, JsonValue)) {
		// Get the value of the json object by field name
		CharID = JsonValue->AsObject()->GetStringField("charID");
		if (CharID.Len() == 0)
		{
			UE_LOG(ConvaiBotHttpLog, Warning, TEXT("Failed to receive CharID, Response:%s"), *Response);
			failed();
			return;
		}
	}
	else
	{
		UE_LOG(ConvaiBotHttpLog, Warning, TEXT("Could not deserialize response into Json, Response:%s"), *Response);
		failed();
		return;
	}

	success();
}


void UConvaiChatBotCreateProxy::failed()
{
	OnFailure.Broadcast(CharID);
	finish();
}


void UConvaiChatBotCreateProxy::success()
{
	OnSuccess.Broadcast(CharID);
	finish();
}


void UConvaiChatBotCreateProxy::finish()
{
}







UConvaiChatBotUpdateProxy* UConvaiChatBotUpdateProxy::CreateCharacterUpdateProxy(UObject* WorldContextObject, FString CharID, FString NewVoice, FString NewBackstory, FString NewCharName, FString NewLanguage)
{
	UConvaiChatBotUpdateProxy* Proxy = NewObject<UConvaiChatBotUpdateProxy>();
	Proxy->WorldPtr = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	Proxy->URL = "https://api.convai.com/character/update";
	Proxy->API_key = Convai::Get().GetConvaiSettings()->API_Key;
	Proxy->CharID = CharID;
	Proxy->NewVoice = NewVoice;
	Proxy->NewBackstory = NewBackstory;
	Proxy->NewCharName = NewCharName;
	Proxy->NewLanguage = NewLanguage;
	return Proxy;
}


void UConvaiChatBotUpdateProxy::Activate()
{
	//UE_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request failed with code %d"), sum(1,6));

	UWorld* World = WorldPtr.Get();

	if (!World)
	{
		UE_LOG(ConvaiBotHttpLog, Warning, TEXT("Could not get a pointer to world!"));
		failed();
		return;
	}

	FHttpModule* Http = &FHttpModule::Get();
	if (!Http)
	{
		UE_LOG(ConvaiBotHttpLog, Warning, TEXT("Could not get a pointer to http module!"));
		failed();
		return;
	}

	// Form Validation
	if (
		!UConvaiFormValidation::ValidateAPIKey(API_key)
		|| !UConvaiFormValidation::ValidateCharacterID(CharID)
		//|| !UConvaiFormValidation::ValidateCharacterName(NewCharName)
		//|| !UConvaiFormValidation::ValidateBackstory(NewBackstory)
		//|| !UConvaiFormValidation::ValidateVoiceType(NewVoice)
		)
	{
		failed();
		return;
	}

	// Create the request
	FHttpRequestRef Request = Http->CreateRequest();
	Request->OnProcessRequestComplete().BindUObject(this, &UConvaiChatBotUpdateProxy::onHttpRequestComplete);

	// Set request fields
	Request->SetURL(URL);
	Request->SetVerb("POST");
	Request->SetHeader(TEXT("User-Agent"), TEXT("X-UnrealEngine-Agent"));
	Request->SetHeader(TEXT("CONVAI-API-KEY"), API_key);
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));



	//// prepare json data
	FString JsonString;
	TSharedRef<TJsonWriter<TCHAR>> JsonWriter = TJsonWriterFactory<TCHAR>::Create(&JsonString);

	JsonWriter->WriteObjectStart();
	JsonWriter->WriteValue("charID", CharID);
	if (NewVoice.Len())
		JsonWriter->WriteValue("voiceType", NewVoice);
	if (NewBackstory.Len())
		JsonWriter->WriteValue("backstory", NewBackstory);
	if (NewCharName.Len())
		JsonWriter->WriteValue("charName", NewCharName);
	if (NewLanguage.Len())
		JsonWriter->WriteValue("languageCode", NewLanguage);


	JsonWriter->WriteObjectEnd();
	JsonWriter->Close();

	Request->SetContentAsString(JsonString);

	// Run the request
	if (!Request->ProcessRequest()) failed();
}


void UConvaiChatBotUpdateProxy::onHttpRequestComplete(FHttpRequestPtr RequestPtr, FHttpResponsePtr ResponsePtr, bool bWasSuccessful)
{
	if (!ResponsePtr)
	{
		if (bWasSuccessful)
		{
			UE_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request succeded - But response pointer is invalid"));
		}
		else
		{
			UE_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request failed - Response pointer is invalid"));
		}

		failed();
		return;
	}
	if (!bWasSuccessful || ResponsePtr->GetResponseCode() < 200 || ResponsePtr->GetResponseCode() > 299)
	{
		UE_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request failed with code %d, and response:%s this is not happening"), ResponsePtr->GetResponseCode(), *ResponsePtr->GetContentAsString());
		UE_LOG(ConvaiBotHttpLog, Warning, TEXT("Response:%s"), *ResponsePtr->GetContentAsString());

		failed();
		return;
	}

	FString Response = ResponsePtr->GetContentAsString();

	TSharedPtr<FJsonValue> JsonValue;
	// Create a reader pointer to read the json data
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response);

	if (FJsonSerializer::Deserialize(Reader, JsonValue)) {
		// Get the value of the json object by field name
		FString status = JsonValue->AsObject()->GetStringField("STATUS");
		if (status.Len() == 0)
		{
			UE_LOG(ConvaiBotHttpLog, Warning, TEXT("Failed to receive status, Response:%s"), *Response);
			failed();
			return;
		}
		else if (status != "SUCCESS")
		{
			UE_LOG(ConvaiBotHttpLog, Warning, TEXT("%s"), *Response);
			failed();
			return;
		}
	}
	else
	{
		UE_LOG(ConvaiBotHttpLog, Warning, TEXT("Could not deserialize response into Json, Response:%s"), *Response);
		failed();
		return;
	}

	success();
}


void UConvaiChatBotUpdateProxy::failed()
{
	OnFailure.Broadcast();
	finish();
}


void UConvaiChatBotUpdateProxy::success()
{
	OnSuccess.Broadcast();
	finish();
}


void UConvaiChatBotUpdateProxy::finish()
{
}








UConvaiChatBotGetDetailsProxy* UConvaiChatBotGetDetailsProxy::CreateCharacterGetDetailsProxy(UObject* WorldContextObject, FString CharID)
{
	UConvaiChatBotGetDetailsProxy* Proxy = NewObject<UConvaiChatBotGetDetailsProxy>();
	Proxy->WorldPtr = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	Proxy->URL = "https://api.convai.com/character/get";
	Proxy->API_key = Convai::Get().GetConvaiSettings()->API_Key;
	Proxy->CharID = CharID;
	Proxy->HasReadyPlayerMeLink = false;
	return Proxy;
}


void UConvaiChatBotGetDetailsProxy::Activate()
{
	UWorld* World = WorldPtr.Get();

	if (!World)
	{
		UE_LOG(ConvaiBotHttpLog, Warning, TEXT("Could not get a pointer to world!"));
		failed();
		return;
	}

	FHttpModule* Http = &FHttpModule::Get();
	if (!Http)
	{
		UE_LOG(ConvaiBotHttpLog, Warning, TEXT("Could not get a pointer to http module!"));
		failed();
		return;
	}

	// Form Validation
	if (
		!UConvaiFormValidation::ValidateAPIKey(API_key)
		|| !UConvaiFormValidation::ValidateCharacterID(CharID)
		)
	{
		failed();
		return;
	}

	// Create the request
	FHttpRequestRef Request = Http->CreateRequest();
	Request->OnProcessRequestComplete().BindUObject(this, &UConvaiChatBotGetDetailsProxy::onHttpRequestComplete);

	// Set request fields
	Request->SetURL(URL);
	Request->SetVerb("POST");
	Request->SetHeader(TEXT("User-Agent"), TEXT("X-UnrealEngine-Agent"));
	Request->SetHeader(TEXT("CONVAI-API-KEY"), API_key);
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	// prepare json data
	FString JsonString;
	TSharedRef<TJsonWriter<TCHAR>> JsonWriter = TJsonWriterFactory<TCHAR>::Create(&JsonString);

	JsonWriter->WriteObjectStart();
	JsonWriter->WriteValue("charID", CharID);

	JsonWriter->WriteObjectEnd();
	JsonWriter->Close();

	Request->SetContentAsString(JsonString);

	// Run the request
	if (!Request->ProcessRequest()) failed();
}


void UConvaiChatBotGetDetailsProxy::onHttpRequestComplete(FHttpRequestPtr RequestPtr, FHttpResponsePtr ResponsePtr, bool bWasSuccessful)
{
	if (!ResponsePtr)
	{
		if (bWasSuccessful)
		{
			UE_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request succeded - But response pointer is invalid"));
		}
		else
		{
			UE_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request failed - Response pointer is invalid"));
		}

		failed();
		return;
	}
	if (!bWasSuccessful || ResponsePtr->GetResponseCode() < 200 || ResponsePtr->GetResponseCode() > 299)
	{
		UE_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request failed with code %d, and response:%s"), ResponsePtr->GetResponseCode(), *ResponsePtr->GetContentAsString());
		UE_LOG(ConvaiBotHttpLog, Warning, TEXT("Response:%s"), *ResponsePtr->GetContentAsString());

		failed();
		return;
	}

	//FString ResponseContent = ResponsePtr->GetContentAsString();
	//UE_LOG(LogTemp, Log, TEXT("HTTP Response Content: %s"), *ResponseContent);

	FString Response = ResponsePtr->GetContentAsString();

	TSharedPtr<FJsonValue> JsonValue;
	// Create a reader pointer to read the json data
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response);

	if (FJsonSerializer::Deserialize(Reader, JsonValue)) {
		// Get the value of the json object by field name
		character_name	= JsonValue->AsObject()->GetStringField("character_name");
		//user_id			= JsonValue->AsObject()->GetStringField("user_id");
		FString character_id	= JsonValue->AsObject()->GetStringField("character_id");
		voice_type		= JsonValue->AsObject()->GetStringField("voice_type");
		//timestamp		= JsonValue->AsObject()->GetStringField("timestamp");
		backstory		= JsonValue->AsObject()->GetStringField("backstory");
		LanguageCode = JsonValue->AsObject()->GetStringField("language_code");
		AvatarImageLink = FString("https://convai.com/_next/static/images/placeholder-3d-cab6463359f6ccedb4cda311c4056788.jpg");

		const TSharedPtr<FJsonObject>* model_details;
		if (JsonValue->AsObject()->TryGetObjectField(TEXT("model_details"), model_details))
		{
			if ((*model_details)->TryGetStringField(TEXT("modelLink"), ReadyPlayerMeLink))
			{
				// Make sure ReadyPlayerMeLink is not null or empty
				if (ReadyPlayerMeLink != "NULL" && ReadyPlayerMeLink.Len() > 0)
				{
					HasReadyPlayerMeLink = true;
					(*model_details)->TryGetStringField(TEXT("modelPlaceholder"), AvatarImageLink);
				}
			}
		}

		if (character_name.Len() == 0 || voice_type.Len() == 0)
		{
			UE_LOG(ConvaiBotHttpLog, Warning, TEXT("One or more expected inputs was not received, Response:%s"), *Response);
			failed();
			return;
		}
	}
	else
	{
		UE_LOG(ConvaiBotHttpLog, Warning, TEXT("Could not deserialize response into Json, Response:%s"), *Response);
		failed();
		return;
	}
	success();
}


void UConvaiChatBotGetDetailsProxy::failed()
{
	OnFailure.Broadcast(character_name,
		voice_type,
		backstory,
		LanguageCode,
		HasReadyPlayerMeLink,
		ReadyPlayerMeLink,
		AvatarImageLink);
	finish();
}


void UConvaiChatBotGetDetailsProxy::success()
{
	OnSuccess.Broadcast(character_name,
		voice_type,
		backstory,
		LanguageCode,
		HasReadyPlayerMeLink,
		ReadyPlayerMeLink,
		AvatarImageLink);
	finish();
}


void UConvaiChatBotGetDetailsProxy::finish()
{
}













UConvaiChatBotGetCharsProxy* UConvaiChatBotGetCharsProxy::CreateCharacterGetCharsProxy(UObject* WorldContextObject)
{
	UConvaiChatBotGetCharsProxy* Proxy = NewObject<UConvaiChatBotGetCharsProxy>();
	Proxy->WorldPtr = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	Proxy->URL = "https://api.convai.com/character/list";
	Proxy->API_key = Convai::Get().GetConvaiSettings()->API_Key;
	return Proxy;
}


void UConvaiChatBotGetCharsProxy::Activate()
{
	//UE_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request failed with code %d"), sum(1,6));

	UWorld* World = WorldPtr.Get();

	if (!World)
	{
		UE_LOG(ConvaiBotHttpLog, Warning, TEXT("Could not get a pointer to world!"));
		failed();
		return;
	}

	FHttpModule* Http = &FHttpModule::Get();
	if (!Http)
	{
		UE_LOG(ConvaiBotHttpLog, Warning, TEXT("Could not get a pointer to http module!"));
		failed();
		return;
	}

	// Create the request
	FHttpRequestRef Request = Http->CreateRequest();
	Request->OnProcessRequestComplete().BindUObject(this, &UConvaiChatBotGetCharsProxy::onHttpRequestComplete);

	// Set request fields
	Request->SetURL(URL);
	Request->SetVerb("POST");
	Request->SetHeader(TEXT("User-Agent"), TEXT("X-UnrealEngine-Agent"));
	Request->SetHeader(TEXT("CONVAI-API-KEY"), API_key);

	// Run the request
	if (!Request->ProcessRequest()) failed();
}


void UConvaiChatBotGetCharsProxy::onHttpRequestComplete(FHttpRequestPtr RequestPtr, FHttpResponsePtr ResponsePtr, bool bWasSuccessful)
{
	if (!ResponsePtr)
	{
		if (bWasSuccessful)
		{
			UE_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request succeded - But response pointer is invalid"));
		}
		else
		{
			UE_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request failed - Response pointer is invalid"));
		}

		failed();
		return;
	}
	if (!bWasSuccessful || ResponsePtr->GetResponseCode() < 200 || ResponsePtr->GetResponseCode() > 299)
	{
		UE_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request failed with code %d, and response:%s"), ResponsePtr->GetResponseCode(), *ResponsePtr->GetContentAsString());
		UE_LOG(ConvaiBotHttpLog, Warning, TEXT("Response:%s"), *ResponsePtr->GetContentAsString());

		failed();
		return;
	}

	FString Response = ResponsePtr->GetContentAsString();

	TSharedPtr<FJsonValue> JsonValue;
	// Create a reader pointer to read the json data
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response);

	if (FJsonSerializer::Deserialize(Reader, JsonValue)) {
		// Get the value of the json object by field name
		const TArray< TSharedPtr<FJsonValue> > *OutArray;
		if (JsonValue->AsObject()->TryGetArrayField("characters", OutArray))
		{
			for (int i = 0; i < OutArray->Num(); i++)
			{
				FString CharID;
				if ((*OutArray)[i]->AsObject()->TryGetStringField("character_id", CharID))
				{
					CharIDs.Add(CharID);
				}
				else
				{
					UE_LOG(ConvaiBotHttpLog, Warning, TEXT("CharIDs is received in an invalid format, Response:%s"), *Response);
					failed();
					return;
				}
			}
		}
		else
		{
			UE_LOG(ConvaiBotHttpLog, Warning, TEXT("Failed to receive CharIDs, Response:%s"), *Response);
			failed();
			return;
		}
	}
	else
	{
		UE_LOG(ConvaiBotHttpLog, Warning, TEXT("Could not deserialize response into Json, Response:%s"), *Response);
		failed();
		return;
	}

	success();
}


void UConvaiChatBotGetCharsProxy::failed()
{
	OnFailure.Broadcast(CharIDs);
	finish();
}


void UConvaiChatBotGetCharsProxy::success()
{
	OnSuccess.Broadcast(CharIDs);
	finish();
}


void UConvaiChatBotGetCharsProxy::finish()
{
}





UConvaiDownloadImageProxy* UConvaiDownloadImageProxy::CreateDownloadImageProxy(UObject* WorldContextObject, FString URL)
{
	UConvaiDownloadImageProxy* Proxy = NewObject<UConvaiDownloadImageProxy>();
	Proxy->WorldPtr = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	Proxy->URL = URL;
	return Proxy;
}

void UConvaiDownloadImageProxy::Activate()
{
	//UE_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request failed with code %d"), sum(1,6));

	UWorld* World = WorldPtr.Get();

	if (!World)
	{
		UE_LOG(ConvaiBotHttpLog, Warning, TEXT("Could not get a pointer to world!"));
		failed();
		return;
	}

	FHttpModule* Http = &FHttpModule::Get();
	if (!Http)
	{
		UE_LOG(ConvaiBotHttpLog, Warning, TEXT("Could not get a pointer to http module!"));
		failed();
		return;
	}

	// Remove trailing slash if present
	if (URL.EndsWith("/")) {
		URL.RemoveAt(URL.Len() - 1, 1, false);
	}

	// Create the request
	FHttpRequestRef Request = Http->CreateRequest();
	Request->OnProcessRequestComplete().BindUObject(this, &UConvaiDownloadImageProxy::onHttpRequestComplete);
	Request->SetVerb("GET");
	Request->SetURL(URL);

	// Run the request
	if (!Request->ProcessRequest()) failed();
}

void UConvaiDownloadImageProxy::onHttpRequestComplete(FHttpRequestPtr RequestPtr, FHttpResponsePtr ResponsePtr, bool bWasSuccessful)
{
	if (!ResponsePtr)
	{
		if (bWasSuccessful)
		{
			UE_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request succeded - But response pointer is invalid"));
		}
		else
		{
			UE_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request failed - Response pointer is invalid"));
		}

		failed();
		return;
	}
	if (!bWasSuccessful || ResponsePtr->GetResponseCode() < 200 || ResponsePtr->GetResponseCode() > 299)
	{
		UE_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request failed with code %d, and response:%s this should not be happening"), ResponsePtr->GetResponseCode(), *ResponsePtr->GetContentAsString());
		UE_LOG(ConvaiBotHttpLog, Warning, TEXT("Response:%s"), *ResponsePtr->GetContentAsString());

		failed();
		return;
	}

	if (ResponsePtr->GetContentLength() <= 0)
	{
		UE_LOG(ConvaiBotHttpLog, Warning, TEXT("Failed to download image - Received 0 bytes"));
	}

	// Load image data into memory buffer
	TArray<uint8> ImageData = ResponsePtr->GetContent();

	// Detect image format using file extension in URL
	FString FileExtension = FPaths::GetExtension(URL);
	EImageFormat ImageFormat = EImageFormat::Invalid;
	if (FileExtension.Equals("jpg", ESearchCase::IgnoreCase) || FileExtension.Equals("jpeg", ESearchCase::IgnoreCase)) {
		ImageFormat = EImageFormat::JPEG;
	}
	else if (FileExtension.Equals("png", ESearchCase::IgnoreCase)) {
		ImageFormat = EImageFormat::PNG;
	}
	else if (FileExtension.Equals("bmp", ESearchCase::IgnoreCase)) {
		ImageFormat = EImageFormat::BMP;
	}

	if (ImageFormat == EImageFormat::Invalid)
	{
		UE_LOG(ConvaiBotHttpLog, Warning, TEXT("Could not detect Image format from URL - Assuming PNG"));
		ImageFormat = EImageFormat::PNG;
	}

	// Decode image using ImageWrapper
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(ImageFormat);
	if (ImageWrapper.IsValid() && ImageWrapper->SetCompressed(ImageData.GetData(), ImageData.Num())) 
	{
		TArray<uint8> UncompressedImageData;
		if (ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, UncompressedImageData)) 
		{
			// Create texture from image data
			Image = UTexture2D::CreateTransient(ImageWrapper->GetWidth(), ImageWrapper->GetHeight(), PF_B8G8R8A8);
			if (Image) {
				FTexture2DMipMap& Mip = Image->GetPlatformData()->Mips[0];
				void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
				FMemory::Memcpy(Data, UncompressedImageData.GetData(), UncompressedImageData.Num());
				Mip.BulkData.Unlock();
				Image->UpdateResource();

				success();
				return;
			}
			else
			{
				UE_LOG(ConvaiBotHttpLog, Warning, TEXT("Could not create Image texture"));
				failed();
			}
		}
		else
		{
			UE_LOG(ConvaiBotHttpLog, Warning, TEXT("ImageWrapper->GetRaw was not successful"));
			failed();
		}
	}
	else
	{
		if (!ImageWrapper.IsValid())
		{

			UE_LOG(ConvaiBotHttpLog, Warning, TEXT("ImageWrapper is not valid"));
		}
		else
		{
			//FString ImageFormatString = UEnum::GetValueAsString(ImageFormat); // Convert enum to string
			UE_LOG(ConvaiBotHttpLog, Warning, TEXT("Image data was not in the expected form - expected: %d"), ImageFormat);
		}
		failed();
	}



	
}

void UConvaiDownloadImageProxy::failed()
{
	OnFailure.Broadcast(Image);
	finish();
}

void UConvaiDownloadImageProxy::success()
{
	OnSuccess.Broadcast(Image);
	finish();
}

void UConvaiDownloadImageProxy::finish()
{
}
