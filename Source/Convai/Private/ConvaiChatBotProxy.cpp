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
#include "RestAPI/ConvaiURL.h"

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
	
	// Define URL functions for all endpoints
	static FString GetResponseURL() { return UConvaiURL::GetFullURL(TEXT("character/getResponse/"), false); }
	static FString GetResponseFromAudioURL() { return UConvaiURL::GetFullURL(TEXT("character/getResponse"), false); }
	static FString CreateCharacterURL() { return UConvaiURL::GetFullURL(TEXT("character/create"), false); }
	static FString UpdateCharacterURL() { return UConvaiURL::GetFullURL(TEXT("character/update"), false); }
	static FString GetCharacterDetailsURL() { return UConvaiURL::GetFullURL(TEXT("character/get"), false); }
	static FString ListCharactersURL() { return UConvaiURL::GetFullURL(TEXT("character/list"), false); }
	static FString GetActionResponseURL() { return UConvaiURL::GetFullURL(TEXT("character/getActionResponse"), false); }
	static FString GetAvailableVoicesURL() { return UConvaiURL::GetFullURL(TEXT("tts/voices"), false); }
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
	Proxy->URL = GetResponseURL();
	Proxy->UserQuery = UserQuery;
	Proxy->VoiceResponse = VoiceResponse;
	Proxy->CharID = CharID;
	Proxy->SessionID = SessionID;

	Proxy->Classification = Classification;
	Proxy->ClassLabels = ClassLabels;
	return Proxy;
}


void UConvaiChatBotQueryProxy::Activate()
{
	//CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request failed with code %d"), sum(1,6));

	UWorld* World = WorldPtr.Get();

	if (!World)
	{
		CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("Could not get a pointer to world!"));
		failed();
		return;
	}

	FHttpModule* Http = &FHttpModule::Get();
	if (!Http)
	{
		CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("Could not get a pointer to http module!"));
		failed();
		return;
	}

	TPair<FString, FString> AuthHeaderAndKey = UConvaiUtils::GetAuthHeaderAndKey();
	FString AuthKey = AuthHeaderAndKey.Value;
	FString AuthHeader = AuthHeaderAndKey.Key;

	// Form Validation
	if (
		!UConvaiFormValidation::ValidateAuthKey(AuthKey)
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
	Request->SetHeader(AuthHeader, AuthKey);
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

	//CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("Content: %s"), *JsonString);
	//CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("Content: %s"), *Result);

	// Run the request
	if (!Request->ProcessRequest()) failed();
}


void UConvaiChatBotQueryProxy::onHttpRequestComplete(FHttpRequestPtr RequestPtr, FHttpResponsePtr ResponsePtr, bool bWasSuccessful)
{
	if (!ResponsePtr)
	{
		if (bWasSuccessful)
		{
			CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request succeded - But response pointer is invalid"));
		}
		else
		{
			CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request failed - Response pointer is invalid"));
		}

		failed();
		return;
	}
	if (!bWasSuccessful || ResponsePtr->GetResponseCode() < 200 || ResponsePtr->GetResponseCode() > 299)
	{
		CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request failed with code %d"), ResponsePtr->GetResponseCode());
		CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("Response:%s"), *ResponsePtr->GetContentAsString());

		failed();
		return;
	}

	FString Response = ResponsePtr->GetContentAsString();
	//CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP Response: %s"), *Response);

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
				CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("Could not decode audio content, Response:%s"), *Response);
				failed();
				return;
			}

			// this->AudioContent = UConvaiUtils::PCMDataToSoundWav(OutByteArray, 1, FCString::Atoi(*SampleRateStr));
			this->AudioContent = UConvaiUtils::WavDataToSoundWave(OutByteArray);

			if (this->AudioContent == nullptr)
			{
				CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("Failed to decode response content to a sound wave"));
				failed();
				return;
			}

		}

		if (BotText.Len() == 0 || NewSessionID.Len() == 0 || (AudioContentString.Len() == 0 && VoiceResponse))
		{
			CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("One or more expected inputs was not received, Response:%s"), *Response);
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
		CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("Could not deserialize response into Json, Response:%s"), *Response);
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
	Proxy->URL = GetResponseFromAudioURL();

	if (!FPaths::FileExists(Filename))
	{
		// check if the file is relative to the save/BouncedWavFiles directory
		FString SaveDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
		Filename = FPaths::Combine(SaveDir, FString("BouncedWavFiles"), Filename);
		if (!FPaths::FileExists(Filename))
		{
			//if (!FPaths::GameAgnosticSavedDir)
			CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("File does not exist!, %s"), *Filename);
			Proxy->failed();
			return nullptr;
		}
	}

	// Read the file into a byte array
	FFileHelper::LoadFileToArray(Proxy->Payload, *Filename, 0);

	Proxy->VoiceResponse = VoiceResponse;
	Proxy->CharID = CharID;
	Proxy->SessionID = SessionID;

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
	Proxy->URL = GetResponseFromAudioURL();

	if (SoundWave == nullptr)
	{
		CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("Sound wave is invalid!"));
		Proxy->failed();
		return nullptr;
	}


	//Proxy->Payload.SetNum(SoundWave->TotalSamples * 2);
	uint8* PCMData = nullptr;
	//int32 numBytes = SoundWave->GeneratePCMData(PCMData, SoundWave->TotalSamples);

	if (SoundWave->RawPCMData == nullptr) {
		CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("RawPCMData is invalid!"));
	}

	//TArray<uint8> AudioBuffer(SoundWave->RawPCMData, SoundWave->RawPCMDataSize);
	TArray<uint8> RawPCMData;
	RawPCMData = TArray<uint8>(SoundWave->RawPCMData, SoundWave->RawPCMDataSize);

	SerializeWaveFile(Proxy->Payload, RawPCMData.GetData(), RawPCMData.Num(), SoundWave->NumChannels, SoundWave->GetSampleRateForCurrentPlatform());

	Proxy->VoiceResponse = VoiceResponse;
	Proxy->CharID = CharID;
	Proxy->SessionID = SessionID;

	Proxy->Classification = Classification;
	Proxy->ClassLabels = ClassLabels;

	return Proxy;
}

void UConvaiChatBotQueryFromAudioProxy::Activate()
{
	//CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request failed with code %d"), sum(1,6));

	UWorld* World = WorldPtr.Get();

	if (!World)
	{
		CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("Could not get a pointer to world!"));
		failed();
		return;
	}

	FHttpModule* Http = &FHttpModule::Get();
	if (!Http)
	{
		CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("Could not get a pointer to http module!"));
		failed();
		return;
	}

	if (Payload.Num() <= 44)
	{
		CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("Payload size is too small, %d bytes!"), Payload.Num());
		failed();
		return;
	}

	TPair<FString, FString> AuthHeaderAndKey = UConvaiUtils::GetAuthHeaderAndKey();
	FString AuthKey = AuthHeaderAndKey.Value;
	FString AuthHeader = AuthHeaderAndKey.Key;

	// Form Validation
	if (
		!UConvaiFormValidation::ValidateAuthKey(AuthKey)
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
	Request->SetHeader(AuthHeader, AuthKey);
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
	//CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("%d bytes"), monoWavBytes.Num());

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
			CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request succeded - But response pointer is invalid"));
		}
		else
		{
			CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request failed - Response pointer is invalid"));
		}

		failed();
		return;
	}
	if (!bWasSuccessful || ResponsePtr->GetResponseCode() < 200 || ResponsePtr->GetResponseCode() > 299)
	{
		CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request failed with code %d, Response:%s"), ResponsePtr->GetResponseCode(), *ResponsePtr->GetContentAsString());
		CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("Response:%s"), *ResponsePtr->GetContentAsString());

		failed();
		return;
	}

	FString Response = ResponsePtr->GetContentAsString();
	//CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request failed with code %s"), *Response);

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
				CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("Could not decode audio content, Response:%s"), *Response);
				failed();
				return;
			}

			// this->AudioContent = UConvaiUtils::PCMDataToSoundWav(OutByteArray, 1, FCString::Atoi(*SampleRateStr));
			this->AudioContent = UConvaiUtils::WavDataToSoundWave(OutByteArray);

			if (this->AudioContent == nullptr)
			{
				CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("Failed to decode response content to a sound wave, Response:%s"), *Response);
				failed();
				return;
			}
			if (BotText.Len() == 0 || UserQuery.Len() == 0 || NewSessionID.Len() == 0 || (AudioContentString.Len() == 0 && VoiceResponse))
			{
				CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("One or more expected inputs was not received, Response:%s"), *Response);
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
		CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("Could not deserialize response into Json, Response:%s"), *Response);
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
	Proxy->URL = CreateCharacterURL();

	Proxy->CharName = CharName;
	Proxy->Voice = Voice;
	Proxy->Backstory = Backstory;
	return Proxy;
}


void UConvaiChatBotCreateProxy::Activate()
{
	//CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request failed with code %d"), sum(1,6));

	UWorld* World = WorldPtr.Get();

	if (!World)
	{
		CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("Could not get a pointer to world!"));
		failed();
		return;
	}

	FHttpModule* Http = &FHttpModule::Get();
	if (!Http)
	{
		CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("Could not get a pointer to http module!"));
		failed();
		return;
	}

	TPair<FString, FString> AuthHeaderAndKey = UConvaiUtils::GetAuthHeaderAndKey();
	FString AuthKey = AuthHeaderAndKey.Value;
	FString AuthHeader = AuthHeaderAndKey.Key;

	// Form Validation
	if (
		!UConvaiFormValidation::ValidateAuthKey(AuthKey)
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
	Request->SetHeader(AuthHeader, AuthKey);
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
			CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request succeded - But response pointer is invalid"));
		}
		else
		{
			CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request failed - Response pointer is invalid"));
		}

		failed();
		return;
	}
	if (!bWasSuccessful || ResponsePtr->GetResponseCode() < 200 || ResponsePtr->GetResponseCode() > 299)
	{
		CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request failed with code %d, and response:%s"), ResponsePtr->GetResponseCode(), *ResponsePtr->GetContentAsString());
		CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("Response:%s"), *ResponsePtr->GetContentAsString());

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
			CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("Failed to receive CharID, Response:%s"), *Response);
			failed();
			return;
		}
	}
	else
	{
		CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("Could not deserialize response into Json, Response:%s"), *Response);
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
	Proxy->URL = UpdateCharacterURL();

	Proxy->CharID = CharID;
	Proxy->NewVoice = NewVoice;
	Proxy->NewBackstory = NewBackstory;
	Proxy->NewCharName = NewCharName;
	Proxy->NewLanguage = NewLanguage;
	return Proxy;
}


void UConvaiChatBotUpdateProxy::Activate()
{
	//CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request failed with code %d"), sum(1,6));

	UWorld* World = WorldPtr.Get();

	if (!World)
	{
		CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("Could not get a pointer to world!"));
		failed();
		return;
	}

	FHttpModule* Http = &FHttpModule::Get();
	if (!Http)
	{
		CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("Could not get a pointer to http module!"));
		failed();
		return;
	}

	TPair<FString, FString> AuthHeaderAndKey = UConvaiUtils::GetAuthHeaderAndKey();
	FString AuthKey = AuthHeaderAndKey.Value;
	FString AuthHeader = AuthHeaderAndKey.Key;

	// Form Validation
	if (
		!UConvaiFormValidation::ValidateAuthKey(AuthKey)
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
	Request->SetHeader(AuthHeader, AuthKey);
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
			CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request succeded - But response pointer is invalid"));
		}
		else
		{
			CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request failed - Response pointer is invalid"));
		}

		failed();
		return;
	}
	if (!bWasSuccessful || ResponsePtr->GetResponseCode() < 200 || ResponsePtr->GetResponseCode() > 299)
	{
		CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request failed with code %d, and response:%s"), ResponsePtr->GetResponseCode(), *ResponsePtr->GetContentAsString());
		CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("Response:%s"), *ResponsePtr->GetContentAsString());

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
			CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("Failed to receive status, Response:%s"), *Response);
			failed();
			return;
		}
		else if (status != "SUCCESS")
		{
			CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("%s"), *Response);
			failed();
			return;
		}
	}
	else
	{
		CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("Could not deserialize response into Json, Response:%s"), *Response);
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
	Proxy->URL = GetCharacterDetailsURL();

	Proxy->CharID = CharID;
	Proxy->HasReadyPlayerMeLink = false;
	return Proxy;
}


void UConvaiChatBotGetDetailsProxy::Activate()
{
	UWorld* World = WorldPtr.Get();

	if (!World)
	{
		CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("Could not get a pointer to world!"));
		failed();
		return;
	}

	FHttpModule* Http = &FHttpModule::Get();
	if (!Http)
	{
		CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("Could not get a pointer to http module!"));
		failed();
		return;
	}

	TPair<FString, FString> AuthHeaderAndKey = UConvaiUtils::GetAuthHeaderAndKey();
	FString AuthKey = AuthHeaderAndKey.Value;
	FString AuthHeader = AuthHeaderAndKey.Key;

	// Form Validation
	if (
		!UConvaiFormValidation::ValidateAuthKey(AuthKey)
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
	Request->SetHeader(AuthHeader, AuthKey);
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
			CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request succeded - But response pointer is invalid"));
		}
		else
		{
			CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request failed - Response pointer is invalid"));
		}

		failed();
		return;
	}
	if (!bWasSuccessful || ResponsePtr->GetResponseCode() < 200 || ResponsePtr->GetResponseCode() > 299)
	{
		CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request failed with code %d, and response:%s"), ResponsePtr->GetResponseCode(), *ResponsePtr->GetContentAsString());
		CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("Response:%s"), *ResponsePtr->GetContentAsString());

		failed();
		return;
	}

	//FString ResponseContent = ResponsePtr->GetContentAsString();
	//CONVAI_LOG(LogTemp, Log, TEXT("HTTP Response Content: %s"), *ResponseContent);

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
			CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("One or more expected inputs was not received, Response:%s"), *Response);
			failed();
			return;
		}
	}
	else
	{
		CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("Could not deserialize response into Json, Response:%s"), *Response);
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
	Proxy->URL = ListCharactersURL();

	return Proxy;
}


void UConvaiChatBotGetCharsProxy::Activate()
{
	//CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request failed with code %d"), sum(1,6));

	UWorld* World = WorldPtr.Get();

	if (!World)
	{
		CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("Could not get a pointer to world!"));
		failed();
		return;
	}

	FHttpModule* Http = &FHttpModule::Get();
	if (!Http)
	{
		CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("Could not get a pointer to http module!"));
		failed();
		return;
	}

	TPair<FString, FString> AuthHeaderAndKey = UConvaiUtils::GetAuthHeaderAndKey();
	FString AuthKey = AuthHeaderAndKey.Value;
	FString AuthHeader = AuthHeaderAndKey.Key;

	// Form Validation
	if (!UConvaiFormValidation::ValidateAuthKey(AuthKey))
	{
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
	Request->SetHeader(AuthHeader, AuthKey);
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	// Run the request
	if (!Request->ProcessRequest()) failed();
}


void UConvaiChatBotGetCharsProxy::onHttpRequestComplete(FHttpRequestPtr RequestPtr, FHttpResponsePtr ResponsePtr, bool bWasSuccessful)
{
	if (!ResponsePtr)
	{
		if (bWasSuccessful)
		{
			CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request succeded - But response pointer is invalid"));
		}
		else
		{
			CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request failed - Response pointer is invalid"));
		}

		failed();
		return;
	}
	if (!bWasSuccessful || ResponsePtr->GetResponseCode() < 200 || ResponsePtr->GetResponseCode() > 299)
	{
		CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request failed with code %d, and response:%s"), ResponsePtr->GetResponseCode(), *ResponsePtr->GetContentAsString());
		CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("Response:%s"), *ResponsePtr->GetContentAsString());

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
					CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("CharIDs is received in an invalid format, Response:%s"), *Response);
					failed();
					return;
				}
			}
		}
		else
		{
			CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("Failed to receive CharIDs, Response:%s"), *Response);
			failed();
			return;
		}
	}
	else
	{
		CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("Could not deserialize response into Json, Response:%s"), *Response);
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

UConvaiDownloadImageProxy* UConvaiDownloadImageProxy::CreateDownloadImageForRPMProxy(UObject* WorldContextObject, FString URL)
{
	UConvaiDownloadImageProxy* Proxy = NewObject<UConvaiDownloadImageProxy>();
	Proxy->WorldPtr = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);

	// Define the base URL for the model
	FString BaseUrl = TEXT("https://models.readyplayer.me/");

	// Check if the URL starts with the base URL and ends with .glb
	if (URL.StartsWith(BaseUrl) && URL.EndsWith(TEXT(".glb")))
	{
		// Extract the model ID
		FString ModelId;
		int32 StartIndex = BaseUrl.Len();
		int32 EndIndex = URL.Len() - 4; // Exclude the length of ".glb"
		ModelId = URL.Mid(StartIndex, EndIndex - StartIndex);

		// Generate the new URL
		FString NewURL = BaseUrl + ModelId + TEXT(".png?pose=relaxed&background=0,0,60&size=512");

		// Assign the new URL to the Proxy
		Proxy->URL = NewURL;
	}
	else
	{
		// If URL doesn't match, just assign the original URL
		Proxy->URL = URL;
	}

	return Proxy;
}

void UConvaiDownloadImageProxy::Activate()
{
	//CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request failed with code %d"), sum(1,6));

	UWorld* World = WorldPtr.Get();

	if (!World)
	{
		CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("Could not get a pointer to world!"));
		failed();
		return;
	}

	FHttpModule* Http = &FHttpModule::Get();
	if (!Http)
	{
		CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("Could not get a pointer to http module!"));
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
			CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request succeded - But response pointer is invalid"));
		}
		else
		{
			CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request failed - Response pointer is invalid"));
		}

		failed();
		return;
	}
	if (!bWasSuccessful || ResponsePtr->GetResponseCode() < 200 || ResponsePtr->GetResponseCode() > 299)
	{
		CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request failed with code %d, and response:%s"), ResponsePtr->GetResponseCode(), *ResponsePtr->GetContentAsString());
		CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("Response:%s"), *ResponsePtr->GetContentAsString());

		failed();
		return;
	}

	if (ResponsePtr->GetContentLength() <= 0)
	{
		CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("Failed to download image - Received 0 bytes"));
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
		CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("Could not detect Image format from URL - Assuming PNG"));
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
				
#if ENGINE_MAJOR_VERSION < 5
				FTexture2DMipMap& Mip = Image->PlatformData->Mips[0];
#else
				FTexture2DMipMap& Mip = Image->GetPlatformData()->Mips[0];
#endif
				void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
				FMemory::Memcpy(Data, UncompressedImageData.GetData(), UncompressedImageData.Num());
				Mip.BulkData.Unlock();
				Image->UpdateResource();

				success();
				return;
			}
			else
			{
				CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("Could not create Image texture"));
				failed();
			}
		}
		else
		{
			CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("ImageWrapper->GetRaw was not successful"));
			failed();
		}
	}
	else
	{
		if (!ImageWrapper.IsValid())
		{

			CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("ImageWrapper is not valid"));
		}
		else
		{
			//FString ImageFormatString = UEnum::GetValueAsString(ImageFormat); // Convert enum to string
			CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("Image data was not in the expected form - expected: %d"), ImageFormat);
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














UConvaiGetAvailableVoicesProxy* UConvaiGetAvailableVoicesProxy::CreateGetAvailableVoicesProxy(EVoiceType VoiceType, ELanguageType LanguageType, EGenderType Gender)
{
	UConvaiGetAvailableVoicesProxy* Proxy = NewObject<UConvaiGetAvailableVoicesProxy>();
	Proxy->URL = GetAvailableVoicesURL();
	Proxy->FilterVoiceType = VoiceType;
	Proxy->FilterLanguageType = LanguageType;
	Proxy->FilterGender = Gender;
	return Proxy;
}

void UConvaiGetAvailableVoicesProxy::Activate()
{
	FHttpModule* Http = &FHttpModule::Get();
	if (!Http)
	{
		CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("Could not get a pointer to http module!"));
		failed();
		return;
	}

	TPair<FString, FString> AuthHeaderAndKey = UConvaiUtils::GetAuthHeaderAndKey();
	FString AuthKey = AuthHeaderAndKey.Value;
	FString AuthHeader = AuthHeaderAndKey.Key;

	// Form Validation
	if (!UConvaiFormValidation::ValidateAuthKey(AuthKey))
	{
		failed();
		return;
	}

	// Create the request
	FHttpRequestRef Request = Http->CreateRequest();
	Request->OnProcessRequestComplete().BindUObject(this, &UConvaiGetAvailableVoicesProxy::onHttpRequestComplete);

	// Set request fields
	Request->SetURL(URL);
	Request->SetVerb("GET");
	Request->SetHeader(TEXT("User-Agent"), TEXT("X-UnrealEngine-Agent"));
	Request->SetHeader(AuthHeader, AuthKey);
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	// Run the request
	if (!Request->ProcessRequest()) failed();
}

void UConvaiGetAvailableVoicesProxy::onHttpRequestComplete(FHttpRequestPtr RequestPtr, FHttpResponsePtr ResponsePtr, bool bWasSuccessful)
{
	if (!ResponsePtr)
	{
		if (bWasSuccessful)
		{
			CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request succeded - But response pointer is invalid"));
		}
		else
		{
			CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request failed - Response pointer is invalid"));
		}

		failed();
		return;
	}
	if (!bWasSuccessful || ResponsePtr->GetResponseCode() < 200 || ResponsePtr->GetResponseCode() > 299)
	{
		CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("HTTP request failed with code %d, and response:%s"), ResponsePtr->GetResponseCode(), *ResponsePtr->GetContentAsString());
		CONVAI_LOG(ConvaiBotHttpLog, Warning, TEXT("Response:%s"), *ResponsePtr->GetContentAsString());

		failed();
		return;
	}
	
	FString Response = ResponsePtr->GetContentAsString();
	
	if (!ParseAllVoiceData(Response, AvailableVoices.AvailableVoices))
	{
		failed();
		return;
	}

	if (AvailableVoices.AvailableVoices.IsEmpty())
	{
		failed();
		return;
	}

	success();
}

void UConvaiGetAvailableVoicesProxy::failed()
{
	OnFailure.Broadcast(FAvailableVoices());
}

void UConvaiGetAvailableVoicesProxy::success()
{
	OnSuccess.Broadcast(AvailableVoices);
}

bool UConvaiGetAvailableVoicesProxy::ParseAllVoiceData(const FString& JsonString, TMap<FString, FVoiceLanguageStruct>& FilteredVoices)
{
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	TSharedPtr<FJsonObject> JsonObject;

	if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
	{
		FString LanguagePrefix = GetLanguageCodeFromEnum(FilterLanguageType);
		FString VoiceTypeString = GetVoiceTypeFromEnum(FilterVoiceType);
		FString GenderTypeString = GetGenderFromEnum(FilterGender);

		for (const auto& VoiceTypeEntry : JsonObject->Values)
		{
			FString VoiceType = VoiceTypeEntry.Key;
			const TArray<TSharedPtr<FJsonValue>>* VoiceArray;

			if (VoiceTypeEntry.Value->TryGetArray(VoiceArray))
			{
				for (const TSharedPtr<FJsonValue>& Value : *VoiceArray)
				{
					TSharedPtr<FJsonObject> VoiceObject = Value->AsObject();
					if (VoiceObject.IsValid())
					{
						FVoiceLanguageStruct VoiceData;

						if (ParseVoiceData(VoiceObject, VoiceData))
						{
							VoiceData.VoiceType = VoiceType;

							// Apply the filters here directly
							if (VoiceType == VoiceTypeString && VoiceData.Gender == GenderTypeString)
							{
								// Filter based on language code
								FVoiceLanguageStruct FilteredVoiceData;
								bool bShouldAdd = false;

								for (const FString& LangCode : VoiceData.LangCodes)
								{
									if (LangCode.StartsWith(LanguagePrefix))
									{
										bShouldAdd = true;
										FilteredVoiceData.LangCodes.Add(LangCode);
									}
								}

								if (bShouldAdd)
								{
									FilteredVoiceData.VoiceValue = VoiceData.VoiceValue;
									FilteredVoices.Add(VoiceData.VoiceName, FilteredVoiceData);
								}
							}
						}
					}
				}
			}
		}
		return true;
	}

	return false;
}

bool UConvaiGetAvailableVoicesProxy::ParseVoiceData(TSharedPtr<FJsonObject> VoiceObject, FVoiceLanguageStruct& OutVoice)
{
	if (VoiceObject.IsValid())
	{
		for (const auto& VoiceEntry : VoiceObject->Values)
		{
			OutVoice.VoiceName = VoiceEntry.Key;

			TSharedPtr<FJsonObject> VoiceDetails = VoiceEntry.Value->AsObject();
			if (VoiceDetails.IsValid())
			{
				// Check for deployment env
				const TArray<TSharedPtr<FJsonValue>>* Deployment_Env;
				if (VoiceDetails->TryGetArrayField(TEXT("deployment_env"), Deployment_Env))
				{
					bool bInProd = false;
					for (const TSharedPtr<FJsonValue>& It : *Deployment_Env)
					{
						if (It->AsString().MatchesWildcard(TEXT("PRODUCTION")))
						{
							bInProd = true;
							break;
						}
					} 

					if (!bInProd)
						return false;
				}


				OutVoice.LangCodes.Empty();

				const TArray<TSharedPtr<FJsonValue>>* LangCodesArray;
				if (VoiceDetails->TryGetArrayField(TEXT("lang_codes"), LangCodesArray))
				{
					for (const TSharedPtr<FJsonValue>& LangValue : *LangCodesArray)
					{
						OutVoice.LangCodes.Add(LangValue->AsString());
					}
				}
				VoiceDetails->TryGetStringField(TEXT("voice_value"), OutVoice.VoiceValue);
				VoiceDetails->TryGetStringField(TEXT("gender"), OutVoice.Gender);
			}
		}
		return true;
	}

	return false;
}

FString UConvaiGetAvailableVoicesProxy::GetLanguageCodeFromEnum(ELanguageType LanguageType)
{
	switch (LanguageType)
	{
	case ELanguageType::Arabic:                return "ar";
	case ELanguageType::ChineseCantonese:      return "zh";
	case ELanguageType::ChineseMandarin:       return "zh";
	case ELanguageType::Dutch:                 return "nl";
	case ELanguageType::DutchBelgium:          return "nl";
	case ELanguageType::English:               return "en";
	case ELanguageType::Finnish:               return "fi";
	case ELanguageType::French:                return "fr";
	case ELanguageType::German:                return "de";
	case ELanguageType::Hindi:                 return "hi";
	case ELanguageType::Italian:               return "it";
	case ELanguageType::Japanese:              return "ja";
	case ELanguageType::Korean:                return "ko";
	case ELanguageType::Polish:                return "pl";
	case ELanguageType::PortugueseBrazil:      return "pt";
	case ELanguageType::PortuguesePortugal:    return "pt";
	case ELanguageType::Russian:               return "ru";
	case ELanguageType::Spanish:               return "es";
	case ELanguageType::SpanishMexico:         return "es";
	case ELanguageType::SpanishUS:             return "es";
	case ELanguageType::Swedish:               return "sv";
	case ELanguageType::Turkish:               return "tr";
	case ELanguageType::Vietnamese:            return "vi";
	default:                                   return "";
	}
}

FString UConvaiGetAvailableVoicesProxy::GetVoiceTypeFromEnum(EVoiceType VoiceType)
{
	switch (VoiceType)
	{
	case EVoiceType::AzureVoices:
		return TEXT("Azure Voices");
	case EVoiceType::ElevenLabsVoices:
		return TEXT("ElevenLabs Voices");
	case EVoiceType::GCPVoices:
		return TEXT("GCP Voices");
	case EVoiceType::ConvaiVoices:
		return TEXT("Convai Voices");
	case EVoiceType::OpenAIVoices:
		return TEXT("OpenAI Voices");
	case EVoiceType::ConvaiVoicesNew:
		return TEXT("Convai Voices (New)");
	case EVoiceType::ConvaiVoicesExperimental:
		return TEXT("Convai Voices (Experimental)");
	default:
		return TEXT("Unknown Voice Type");
	}
}

FString UConvaiGetAvailableVoicesProxy::GetGenderFromEnum(EGenderType GenderType)
{
	switch (GenderType)
	{
	case EGenderType::Male: return "MALE";
	case EGenderType::Female: return "FEMALE";
	default: return "";
	}
}
