// Copyright 2022 Convai Inc. All Rights Reserved.

#include "ConvaiGetActionProxy.h"
#include "ConvaiActionUtils.h"
#include "ConvaiUtils.h"
#include "../Convai.h"
#include "Sound/SoundWave.h"
#include "Containers/UnrealString.h"
#include "Sound/SoundWave.h"
#include "Misc/Base64.h"
#include "Engine.h"
#include "JsonObjectConverter.h"


DEFINE_LOG_CATEGORY(ConvaiGetActionHttpLog);


UConvaiGetActionProxy* UConvaiGetActionProxy::CreateGetActionQueryProxy(UObject* WorldContextObject, FString TextQuery, UConvaiActionContext* Context)
{
	UConvaiGetActionProxy* Proxy = NewObject<UConvaiGetActionProxy>();
	Proxy->WorldPtr = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	Proxy->URL = "https://api.convai.com/character/getActions";

	Proxy->Context = Context;
	Proxy->TextQuery = TextQuery;
	return Proxy;
}


void UConvaiGetActionProxy::Activate()
{
	UWorld* World = WorldPtr.Get();

	if (!World)
	{
		UE_LOG(ConvaiGetActionHttpLog, Warning, TEXT("Could not get a pointer to world!"));
		failed();
		return;
	}

	FHttpModule* Http = &FHttpModule::Get();
	if (!Http)
	{
		UE_LOG(ConvaiGetActionHttpLog, Warning, TEXT("Could not get a pointer to http module!"));
		failed();
		return;
	}


	if (!IsValid(Context))
	{
		UE_LOG(ConvaiGetActionHttpLog, Warning, TEXT("Context is invalid!"));
		failed();
		return;
	}
	
	if (!Context->MainCharacter.Ref.IsValid())
	{
		UE_LOG(ConvaiGetActionHttpLog, Warning, TEXT("Main Character in Context object is invalid or not set!"));
		failed();
		return;
	}

	TPair<FString, FString> AuthHeaderAndKey = UConvaiUtils::GetAuthHeaderAndKey();
	FString AuthKey = AuthHeaderAndKey.Value;
	FString AuthHeader = AuthHeaderAndKey.Key;

	// Form Validation
	if (!UConvaiFormValidation::ValidateAuthKey(AuthKey) || !UConvaiFormValidation::ValidateInputText(TextQuery))
	{
		failed();
		return;
	}

	// Create the request
	FHttpRequestRef Request = Http->CreateRequest();
	Request->OnProcessRequestComplete().BindUObject(this, &UConvaiGetActionProxy::onHttpRequestComplete);

	// Set request fields
	Request->SetURL(URL);
	Request->SetVerb("POST");
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(AuthHeader, AuthKey);

	// prepare json data
	FString JsonString;
	TSharedRef<TJsonWriter<TCHAR>> JsonWriter = TJsonWriterFactory<TCHAR>::Create(&JsonString);
	JsonWriter->WriteObjectStart();
	JsonWriter->WriteRawJSONValue(TEXT("actions"), FString::Printf(TEXT("[\"%s\"]"), *FString::Join(Context->Actions, *FString("\",\""))));	

	JsonWriter->WriteArrayStart(TEXT("objects"));
	for (auto o : Context->Objects)
	{
		//if (!o.Ref.IsValid())
		//{
		//	UE_LOG(ConvaiGetActionHttpLog, Warning, TEXT("Invalid object found in Context object!"));
		//	failed();
		//	return;
		//}
		JsonWriter->WriteObjectStart();
		JsonWriter->WriteValue("name", o.Name);
		JsonWriter->WriteValue("description", o.Description);
		JsonWriter->WriteObjectEnd();
	}
	JsonWriter->WriteArrayEnd();

	JsonWriter->WriteArrayStart(TEXT("characters"));
	for (auto c : Context->Characters)
	{
		//if (!c.Ref.IsValid())
		//{
		//	UE_LOG(ConvaiGetActionHttpLog, Warning, TEXT("Invalid Character found in Context object!"));
		//	failed();
		//	return;
		//}
		JsonWriter->WriteObjectStart();
		JsonWriter->WriteValue("name", c.Name);
		JsonWriter->WriteValue("bio", c.Description);
		JsonWriter->WriteObjectEnd();
	}
	JsonWriter->WriteArrayEnd();

	JsonWriter->WriteArrayStart(TEXT("conversationLogs"));
	JsonWriter->WriteObjectStart();

	JsonWriter->WriteObjectStart("character");
	JsonWriter->WriteValue("name", Context->MainCharacter.Name);
	JsonWriter->WriteObjectEnd();
	JsonWriter->WriteValue("message", TextQuery);
	JsonWriter->WriteObjectEnd();
	JsonWriter->WriteArrayEnd();

	JsonWriter->WriteObjectEnd();
	JsonWriter->Close();
	// Insert the content into the request
	Request->SetContentAsString(JsonString);

	// Run the request
	if (!Request->ProcessRequest()) failed();


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

	//UE_LOG(ConvaiGetActionHttpLog, Warning, TEXT("Content: %s"), *Result);

}


void UConvaiGetActionProxy::onHttpRequestComplete(FHttpRequestPtr RequestPtr, FHttpResponsePtr ResponsePtr, bool bWasSuccessful)
{
	if (!IsValid(Context))
	{
		UE_LOG(ConvaiGetActionHttpLog, Warning, TEXT("onHttpRequestComplete: Context is invalid!"));
		failed();
		return;
	}

	if (!Context->MainCharacter.Ref.IsValid())
	{
		UE_LOG(ConvaiGetActionHttpLog, Warning, TEXT("onHttpRequestComplete: Main Character in Context object is invalid or not set!"));
		failed();
		return;
	}


	if (!bWasSuccessful || ResponsePtr->GetResponseCode() < 200 || ResponsePtr->GetResponseCode() > 299)
	{
		UE_LOG(ConvaiGetActionHttpLog, Warning, TEXT("HTTP request failed with code %d, and response:%s"), ResponsePtr->GetResponseCode(), *ResponsePtr->GetContentAsString());
		failed();
		return;
	}

	FString Response = ResponsePtr->GetContentAsString();



	TSharedPtr<FJsonValue> JsonValue;
	// Create a reader pointer to read the json data
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response);

	if (FJsonSerializer::Deserialize(Reader, JsonValue)) {
		// Get the value of the json object by field name
		FString response = JsonValue->AsObject()->GetStringField("response");
		if (response == "")
		{
			UE_LOG(ConvaiGetActionHttpLog, Warning, TEXT("Failed to receive response key, response:%s"), *ResponsePtr->GetContentAsString());
			failed();
			return;
		}

		TArray<FString> Sequence;
		response.ParseIntoArray(Sequence, TEXT("\n"), true);
		for (auto s : Sequence)
		{
			UE_LOG(ConvaiGetActionHttpLog, Log, TEXT("sequence: %s"), *s);

			FConvaiResultAction ConvaiResultAction;
			if (UConvaiActions::ParseAction(Context, s, ConvaiResultAction))
				SequenceOfActions.Add(ConvaiResultAction);
		}
	}
	else
	{
		UE_LOG(ConvaiGetActionHttpLog, Warning, TEXT("Could not deserialize response into Json, Response:%s"), *Response);
		failed();
		return;
	}

	success();
}


void UConvaiGetActionProxy::failed()
{
	OnFailure.Broadcast(SequenceOfActions);
	finish();
}


void UConvaiGetActionProxy::success()
{
	OnSuccess.Broadcast(SequenceOfActions);
	finish();
}


void UConvaiGetActionProxy::finish()
{
}





UConvaiGetActionResponseProxy* UConvaiGetActionResponseProxy::CreateGetActionResponseQueryProxy(
	UObject* WorldContextObject,
	FString UserQuery,
	bool VoiceResponse,
	FString CharID,
	FString SessionID,
	UConvaiActionContext* Context)
{
	UConvaiGetActionResponseProxy* Proxy = NewObject<UConvaiGetActionResponseProxy>();
	Proxy->WorldPtr = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	Proxy->URL = "https://api.convai.com/character/getActionResponse";
	Proxy->Context = Context;
	Proxy->TextQuery = UserQuery;
	Proxy->VoiceResponse = VoiceResponse;
	Proxy->CharID = CharID;
	Proxy->SessionID = SessionID;

	Proxy->bAudioInput = false;
	return Proxy;
}


UConvaiGetActionResponseProxy* UConvaiGetActionResponseProxy::CreateGetActionQueryFromAudioProxy(
	UObject* WorldContextObject,
	FString Filename,
	bool VoiceResponse,
	FString CharID,
	FString SessionID,
	UConvaiActionContext* Context)
{
	UConvaiGetActionResponseProxy* Proxy = NewObject<UConvaiGetActionResponseProxy>();
	Proxy->WorldPtr = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	Proxy->URL = "https://api.convai.com/character/getActionResponse";

	if (!FPaths::FileExists(Filename))
	{
		// check if the file is relative to the save/BouncedWavFiles directory
		FString SaveDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
		Filename = FPaths::Combine(SaveDir, FString("BouncedWavFiles"), Filename);
		if (!FPaths::FileExists(Filename))
		{
			//if (!FPaths::GameAgnosticSavedDir)
			UE_LOG(ConvaiGetActionHttpLog, Warning, TEXT("File does not exist!, %s"), *Filename);
			Proxy->failed();
			return nullptr;
		}
	}

	// Read the file into a byte array
	FFileHelper::LoadFileToArray(Proxy->Payload, *Filename, 0);
	Proxy->Context = Context;
	Proxy->VoiceResponse = VoiceResponse;
	Proxy->CharID = CharID;
	Proxy->SessionID = SessionID;

	Proxy->bAudioInput = true;

	return Proxy;
}


UConvaiGetActionResponseProxy* UConvaiGetActionResponseProxy::CreateGetActionQueryFromSoundWavProxy(
	UObject* WorldContextObject,
	USoundWave* SoundWave,
	bool VoiceResponse,
	FString CharID,
	FString SessionID,
	UConvaiActionContext* Context)
{
	UConvaiGetActionResponseProxy* Proxy = NewObject<UConvaiGetActionResponseProxy>();
	Proxy->WorldPtr = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	Proxy->URL = "https://api.convai.com/character/getActionResponse";

	if (SoundWave == nullptr)
	{
		UE_LOG(ConvaiGetActionHttpLog, Warning, TEXT("Sound wave is invalid!"));
		Proxy->failed();
		return nullptr;
	}


	//Proxy->Payload.SetNum(SoundWave->TotalSamples * 2);
	uint8* PCMData = nullptr;
	//int32 numBytes = SoundWave->GeneratePCMData(PCMData, SoundWave->TotalSamples);

	if (SoundWave->RawPCMData == nullptr) {
		UE_LOG(ConvaiGetActionHttpLog, Warning, TEXT("RawPCMData is invalid!"));
	}

	//TArray<uint8> AudioBuffer(SoundWave->RawPCMData, SoundWave->RawPCMDataSize);
	TArray<uint8> RawPCMData;
	RawPCMData = TArray<uint8>(SoundWave->RawPCMData, SoundWave->RawPCMDataSize);

	SerializeWaveFile(Proxy->Payload, RawPCMData.GetData(), RawPCMData.Num(), SoundWave->NumChannels, SoundWave->GetSampleRateForCurrentPlatform());

	Proxy->VoiceResponse = VoiceResponse;
	Proxy->Context = Context;
	Proxy->CharID = CharID;
	Proxy->SessionID = SessionID;

	Proxy->bAudioInput = true;


	return Proxy;
}



void UConvaiGetActionResponseProxy::Activate()
{
	UWorld* World = WorldPtr.Get();

	if (!World)
	{
		UE_LOG(ConvaiGetActionHttpLog, Warning, TEXT("Could not get a pointer to world!"));
		failed();
		return;
	}

	FHttpModule* Http = &FHttpModule::Get();
	if (!Http)
	{
		UE_LOG(ConvaiGetActionHttpLog, Warning, TEXT("Could not get a pointer to http module!"));
		failed();
		return;
	}


	if (!IsValid(Context))
	{
		UE_LOG(ConvaiGetActionHttpLog, Warning, TEXT("Context is invalid!"));
		failed();
		return;
	}

	if (!Context->MainCharacter.Ref.IsValid())
	{
		UE_LOG(ConvaiGetActionHttpLog, Warning, TEXT("Main Character in Context object is invalid or not set!"));
		failed();
		return;
	}

	//for (auto c : Context->Characters)
	//{
	//	if (!c.Ref.IsValid())
	//	{
	//		UE_LOG(ConvaiGetActionHttpLog, Warning, TEXT("Invalid Character found in Context object!"));
	//		failed();
	//		return;
	//	}
	//}

	//for (auto o : Context->Objects)
	//{
	//	if (!o.Ref.IsValid())
	//	{
	//		UE_LOG(ConvaiGetActionHttpLog, Warning, TEXT("Invalid object found in Context object!"));
	//		failed();
	//		return;
	//	}
	//}

	TPair<FString, FString> AuthHeaderAndKey = UConvaiUtils::GetAuthHeaderAndKey();
	FString AuthKey = AuthHeaderAndKey.Value;
	FString AuthHeader = AuthHeaderAndKey.Key;

	// Form Validation
	if (
		!UConvaiFormValidation::ValidateAuthKey(AuthKey) 
		|| (!bAudioInput && !UConvaiFormValidation::ValidateInputText(TextQuery))
		|| (bAudioInput && !UConvaiFormValidation::ValidateInputVoice(Payload))
		|| !UConvaiFormValidation::ValidateCharacterID(CharID)
		|| !UConvaiFormValidation::ValidateSessionID(SessionID)
		)
	{
		failed();
		return;
	}

	// Create the request
	FHttpRequestRef Request = Http->CreateRequest();
	Request->OnProcessRequestComplete().BindUObject(this, &UConvaiGetActionResponseProxy::onHttpRequestComplete);

	// Set request fields
	Request->SetURL(URL);
	Request->SetVerb("POST");
	Request->SetHeader(AuthHeader, AuthKey);

	if (!bAudioInput)
	{
		Request->SetHeader("Content-Type", "multipart/form-data; boundary=47d4119ab13dacf6fe13c78f233142e9");
		//Request->SetHeader("Accept-Encoding", "gzip, deflate");
		FString sep = "\r\n--47d4119ab13dacf6fe13c78f233142e9\r\n";

		FString FormTextQuery = "Content-Disposition: form-data; name=\"userText\"\r\n\r\n";
		FString FormCharID = "Content-Disposition: form-data; name=\"charID\"\r\n\r\n";
		FString FormSessionID = "Content-Disposition: form-data; name=\"sessionID\"\r\n\r\n";
		FString FormVoiceResponse = "Content-Disposition: form-data; name=\"voiceResponse\"\r\n\r\n";
		FString FormClassification = "Content-Disposition: form-data; name=\"classification\"\r\n\r\n";
		FString FormSpeaker = "Content-Disposition: form-data; name=\"speaker\"\r\n\r\n";
		FString FormActions = "Content-Disposition: form-data; name=\"actions\"\r\n\r\n";
		FString FormCharacters = "Content-Disposition: form-data; name=\"characters\"\r\n\r\n";
		FString FormObjects = "Content-Disposition: form-data; name=\"objects\"\r\n\r\n";
		FString FormFile = "Content-Disposition: form-data; name=\"file\"; filename=\"out.wav\"\r\n\r\n";


		TArray<uint8> data;

		// TextQuery
		data.Append((uint8*)TCHAR_TO_UTF8(*sep), sep.Len());
		data.Append((uint8*)TCHAR_TO_UTF8(*FormTextQuery), FormTextQuery.Len());
		data.Append((uint8*)TCHAR_TO_UTF8(*TextQuery), TextQuery.Len());

		// CharID
		data.Append((uint8*)TCHAR_TO_UTF8(*sep), sep.Len());
		data.Append((uint8*)TCHAR_TO_UTF8(*FormCharID), FormCharID.Len());
		data.Append((uint8*)TCHAR_TO_UTF8(*CharID), CharID.Len());

		// SessionID
		data.Append((uint8*)TCHAR_TO_UTF8(*sep), sep.Len());
		data.Append((uint8*)TCHAR_TO_UTF8(*FormSessionID), FormSessionID.Len());
		data.Append((uint8*)TCHAR_TO_UTF8(*SessionID), SessionID.Len());

		// VoiceResponse
		data.Append((uint8*)TCHAR_TO_UTF8(*sep), sep.Len());
		data.Append((uint8*)TCHAR_TO_UTF8(*FormVoiceResponse), FormVoiceResponse.Len());
		data.Append((uint8*)TCHAR_TO_UTF8(VoiceResponse ? *FString("true") : *FString("false")), VoiceResponse ? 4 : 5);

		// Actions
		FString Actions = FString::Join(Context->Actions, *FString(", "));
		data.Append((uint8*)TCHAR_TO_UTF8(*sep), sep.Len());
		data.Append((uint8*)TCHAR_TO_UTF8(*FormActions), FormActions.Len());
		data.Append((uint8*)TCHAR_TO_UTF8(*Actions), Actions.Len());

		// Classification
		FString Classification = "multistep";
		data.Append((uint8*)TCHAR_TO_UTF8(*sep), sep.Len());
		data.Append((uint8*)TCHAR_TO_UTF8(*FormClassification), FormClassification.Len());
		data.Append((uint8*)TCHAR_TO_UTF8(*Classification), Classification.Len());

		// Objects
		FString Objects = "";
		for (auto o : Context->Objects)
		{
			Objects.Append(FString::Printf(TEXT("{\"name\": \"%s\", \"description\": \"%s\"};"), *o.Name, *o.Description));
		}
		data.Append((uint8*)TCHAR_TO_UTF8(*sep), sep.Len());
		data.Append((uint8*)TCHAR_TO_UTF8(*FormObjects), FormObjects.Len());
		data.Append((uint8*)TCHAR_TO_UTF8(*Objects), Objects.Len());

		// Characters
		FString Characters = "";
		for (auto c : Context->Characters)
		{
			Characters.Append(FString::Printf(TEXT("{\"name\": \"%s\", \"bio\": \"%s\"};"), *c.Name, *c.Description));
		}
		data.Append((uint8*)TCHAR_TO_UTF8(*sep), sep.Len());
		data.Append((uint8*)TCHAR_TO_UTF8(*FormCharacters), FormCharacters.Len());
		data.Append((uint8*)TCHAR_TO_UTF8(*Characters), Characters.Len());

		// Speaker
		FString Speaker = Context->MainCharacter.Name;
		data.Append((uint8*)TCHAR_TO_UTF8(*sep), sep.Len());
		data.Append((uint8*)TCHAR_TO_UTF8(*FormSpeaker), FormSpeaker.Len());
		data.Append((uint8*)TCHAR_TO_UTF8(*Speaker), Speaker.Len());

		sep = "\r\n--47d4119ab13dacf6fe13c78f233142e9--\r\n";
		data.Append((uint8*)TCHAR_TO_UTF8(*sep), sep.Len());


		#if ConvaiDebugMode
			UE_LOG(ConvaiGetActionHttpLog, Log, TEXT("Request body: %s"), *UConvaiUtils::ByteArrayToString(data));
		#endif 


		Request->SetContent(data);
	}
	else
	{
		if (Payload.Num() <= 44)
		{
			UE_LOG(ConvaiGetActionHttpLog, Warning, TEXT("Payload size is too small, %d bytes!"), Payload.Num());
			failed();
			return;
		}

		TArray<uint8> monoWavBytes;
		// Change the wav file from 2 channels to 1 channel
		UConvaiUtils::StereoToMono(Payload, monoWavBytes);


		Request->SetHeader("Content-Type", "multipart/form-data; boundary=47d4119ab13dacf6fe13c78f233142e9");
		Request->SetHeader("Accept-Encoding", "gzip, deflate");
		FString sep = "\r\n--47d4119ab13dacf6fe13c78f233142e9\r\n";

		FString FormCharID = "Content-Disposition: form-data; name=\"charID\"\r\n\r\n";
		FString FormSessionID = "Content-Disposition: form-data; name=\"sessionID\"\r\n\r\n";
		FString FormVoiceResponse = "Content-Disposition: form-data; name=\"voiceResponse\"\r\n\r\n";
		FString FormClassification = "Content-Disposition: form-data; name=\"classification\"\r\n\r\n";
		FString FormSpeaker = "Content-Disposition: form-data; name=\"speaker\"\r\n\r\n";
		FString FormActions = "Content-Disposition: form-data; name=\"actions\"\r\n\r\n";
		FString FormCharacters = "Content-Disposition: form-data; name=\"characters\"\r\n\r\n";
		FString FormObjects = "Content-Disposition: form-data; name=\"objects\"\r\n\r\n";
		FString FormFile = "Content-Disposition: form-data; name=\"file\"; filename=\"out.wav\"\r\n\r\n";


		TArray<uint8> data;

		// CharID
		data.Append((uint8*)TCHAR_TO_UTF8(*sep), sep.Len());
		data.Append((uint8*)TCHAR_TO_UTF8(*FormCharID), FormCharID.Len());
		data.Append((uint8*)TCHAR_TO_UTF8(*CharID), CharID.Len());

		// SessionID
		data.Append((uint8*)TCHAR_TO_UTF8(*sep), sep.Len());
		data.Append((uint8*)TCHAR_TO_UTF8(*FormSessionID), FormSessionID.Len());
		data.Append((uint8*)TCHAR_TO_UTF8(*SessionID), SessionID.Len());

		// VoiceResponse
		data.Append((uint8*)TCHAR_TO_UTF8(*sep), sep.Len());
		data.Append((uint8*)TCHAR_TO_UTF8(*FormVoiceResponse), FormVoiceResponse.Len());
		data.Append((uint8*)TCHAR_TO_UTF8(VoiceResponse ? *FString("true") : *FString("false")), VoiceResponse ? 4 : 5);

		// Actions
		FString Actions = FString::Join(Context->Actions, *FString(", "));
		data.Append((uint8*)TCHAR_TO_UTF8(*sep), sep.Len());
		data.Append((uint8*)TCHAR_TO_UTF8(*FormActions), FormActions.Len());
		data.Append((uint8*)TCHAR_TO_UTF8(*Actions), Actions.Len());

		// Classification
		FString Classification = "multistep";
		data.Append((uint8*)TCHAR_TO_UTF8(*sep), sep.Len());
		data.Append((uint8*)TCHAR_TO_UTF8(*FormClassification), FormClassification.Len());
		data.Append((uint8*)TCHAR_TO_UTF8(*Classification), Classification.Len());

		// Objects
		FString Objects = "";
		for (auto o : Context->Objects)
		{
			Objects.Append(FString::Printf(TEXT("{\"name\": \"%s\", \"description\": \"%s\"};"), *o.Name, *o.Description));
		}
		data.Append((uint8*)TCHAR_TO_UTF8(*sep), sep.Len());
		data.Append((uint8*)TCHAR_TO_UTF8(*FormObjects), FormObjects.Len());
		data.Append((uint8*)TCHAR_TO_UTF8(*Objects), Objects.Len());

		// Characters
		FString Characters = "";
		for (auto c : Context->Characters)
		{
			Characters.Append(FString::Printf(TEXT("{\"name\": \"%s\", \"bio\": \"%s\"};"), *c.Name, *c.Description));
		}
		data.Append((uint8*)TCHAR_TO_UTF8(*sep), sep.Len());
		data.Append((uint8*)TCHAR_TO_UTF8(*FormCharacters), FormCharacters.Len());
		data.Append((uint8*)TCHAR_TO_UTF8(*Characters), Characters.Len());

		// Speaker
		FString Speaker = Context->MainCharacter.Name;
		data.Append((uint8*)TCHAR_TO_UTF8(*sep), sep.Len());
		data.Append((uint8*)TCHAR_TO_UTF8(*FormSpeaker), FormSpeaker.Len());
		data.Append((uint8*)TCHAR_TO_UTF8(*Speaker), Speaker.Len());


#if ConvaiDebugMode
		UE_LOG(ConvaiGetActionHttpLog, Log, TEXT("Request body: %s"), *UConvaiUtils::ByteArrayToString(data));
#endif 

		// Audio File
		data.Append((uint8*)TCHAR_TO_UTF8(*sep), sep.Len());
		data.Append((uint8*)TCHAR_TO_UTF8(*FormFile), FormFile.Len());
		data.Append(monoWavBytes);

		sep = "\r\n--47d4119ab13dacf6fe13c78f233142e9--\r\n";
		data.Append((uint8*)TCHAR_TO_UTF8(*sep), sep.Len());

		Request->SetContent(data);
	}

	// Run the request
	if (!Request->ProcessRequest()) failed();


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

	//UE_LOG(ConvaiGetActionHttpLog, Warning, TEXT("Content: %s"), *Result);

}


void UConvaiGetActionResponseProxy::onHttpRequestComplete(FHttpRequestPtr RequestPtr, FHttpResponsePtr ResponsePtr, bool bWasSuccessful)
{
	if (!IsValid(Context))
	{
		UE_LOG(ConvaiGetActionHttpLog, Warning, TEXT("onHttpRequestComplete: Context is invalid!"));
		failed();
		return;
	}

	if (!Context->MainCharacter.Ref.IsValid())
	{
		UE_LOG(ConvaiGetActionHttpLog, Warning, TEXT("onHttpRequestComplete: Main Character in Context object is invalid or not set!"));
		failed();
		return;
	}


	if (!bWasSuccessful || ResponsePtr->GetResponseCode() < 200 || ResponsePtr->GetResponseCode() > 299)
	{
		UE_LOG(ConvaiGetActionHttpLog, Warning, TEXT("HTTP request failed with code %d, and response:%s"), ResponsePtr->GetResponseCode(), *ResponsePtr->GetContentAsString());
		failed();
		return;
	}

	FString Response = ResponsePtr->GetContentAsString();



	TSharedPtr<FJsonValue> JsonValue;
	// Create a reader pointer to read the json data
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response);


	FString AudioContentString, SampleRateStr;
	if (FJsonSerializer::Deserialize(Reader, JsonValue)) {
		// Get the value of the json object by field name

		// Parse the output session ID and response text
		BotText = JsonValue->AsObject()->GetStringField("response");
		NewSessionID = JsonValue->AsObject()->GetStringField("sessionID");
		UserQuery = JsonValue->AsObject()->GetStringField("UserQuery");

		// Parse output audio
		if (VoiceResponse)
		{
			AudioContentString = JsonValue->AsObject()->GetStringField("audio");
			SampleRateStr = JsonValue->AsObject()->GetStringField("sample_rate");
			TArray<uint8> OutByteArray;
			if (!FBase64::Decode(AudioContentString, OutByteArray))
			{
				UE_LOG(ConvaiGetActionHttpLog, Warning, TEXT("Could not decode audio content, Response:%s"), *Response);
				failed();
				return;
			}

			// this->AudioContent = UConvaiUtils::PCMDataToSoundWav(OutByteArray, 1, FCString::Atoi(*SampleRateStr));
			this->AudioContent = UConvaiUtils::WavDataToSoundWave(OutByteArray);

			if (this->AudioContent == nullptr)
			{
				UE_LOG(ConvaiGetActionHttpLog, Warning, TEXT("Failed to decode response content to a sound wave"));
			}

		}

		// Parse the action sequence
		FString actionSequenceString = JsonValue->AsObject()->GetStringField("actionSequence");
		//UE_LOG(ConvaiGetActionHttpLog, Warning, TEXT("actionSequenceString:%s"), *actionSequenceString);

		TArray<FString> Sequence;
		actionSequenceString.ParseIntoArray(Sequence, TEXT("\n"), true);
		for (auto s : Sequence)
		{
			FConvaiResultAction ConvaiResultAction;
			if (UConvaiActions::ParseAction(Context, s, ConvaiResultAction))
				SequenceOfActions.Add(ConvaiResultAction);
		}

		if (BotText.Len() == 0 || UserQuery.Len() == 0 || NewSessionID.Len() == 0 || (AudioContentString.Len() == 0 && VoiceResponse) || actionSequenceString.Len() == 0)
		{
			UE_LOG(ConvaiGetActionHttpLog, Warning, TEXT("One or more expected inputs was not received, Response:%s"), *Response);
			//failed();
			//return;
		}
	}
	else
	{
		UE_LOG(ConvaiGetActionHttpLog, Warning, TEXT("Could not deserialize response into Json, Response:%s"), *Response);
		failed();
		return;
	}
	success();
}


void UConvaiGetActionResponseProxy::failed()
{
	OnFailure.Broadcast(AudioContent, BotText, UserQuery, NewSessionID, SequenceOfActions);
	finish();
}


void UConvaiGetActionResponseProxy::success()
{
	OnSuccess.Broadcast(AudioContent, BotText, UserQuery, NewSessionID, SequenceOfActions);
	finish();
}


void UConvaiGetActionResponseProxy::finish()
{
}


