// Copyright 2022 Convai Inc. All Rights Reserved.

#include "ConvaiNarrativeDesignProxy.h"
#include "ConvaiUtils.h"
#include "../Convai.h"
#include "Engine.h"
#include "RestAPI/ConvaiURL.h"

DEFINE_LOG_CATEGORY(ConvaiNarrativeHTTP);

UFetchNarrativeSectionsProxy* UFetchNarrativeSectionsProxy::FetchNarrativeSections(UObject* WorldContextObject, FString InCharacterId)
{
	UFetchNarrativeSectionsProxy* Proxy = NewObject<UFetchNarrativeSectionsProxy>();
	Proxy->WorldPtr = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	Proxy->CharacterId = InCharacterId;
	return Proxy;
}

void UFetchNarrativeSectionsProxy::Activate()
{

	UWorld* World = WorldPtr.Get();

	if (!World)
	{
		CONVAI_LOG(ConvaiNarrativeHTTP, Warning, TEXT("Could not get a pointer to world!"));
		failed();
		return;
	}

	FHttpModule* Http = &FHttpModule::Get();
	if (!Http)
	{
		CONVAI_LOG(ConvaiNarrativeHTTP, Warning, TEXT("Could not get a pointer to http module!"));
		failed();
		return;
	}

	TPair<FString, FString> AuthHeaderAndKey = UConvaiUtils::GetAuthHeaderAndKey();
	FString AuthKey = AuthHeaderAndKey.Value;
	FString AuthHeader = AuthHeaderAndKey.Key;

	// Form Validation
	if (
		!UConvaiFormValidation::ValidateAuthKey(AuthKey)
		|| !UConvaiFormValidation::ValidateCharacterID(CharacterId)
		)
	{
		CONVAI_LOG(ConvaiNarrativeHTTP, Warning, TEXT("UFetchNarrativeSectionsProxy::Activate Invalid Character or API key"));
		failed();
		return;
	}

	FHttpModule* HttpModule = &FHttpModule::Get();
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = HttpModule->CreateRequest();
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UFetchNarrativeSectionsProxy::OnHttpRequestCompleted);
	HttpRequest->SetURL(UConvaiURL::GetFullURL(TEXT("character/narrative/list-sections"), false));
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetHeader(AuthHeader, AuthKey);
	HttpRequest->SetContentAsString(FString::Printf(TEXT("{\"character_id\": \"%s\"}"), *CharacterId));
	HttpRequest->ProcessRequest();
}

void UFetchNarrativeSectionsProxy::OnHttpRequestCompleted(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	NarrativeSections.Empty();
	if (!Response)
	{
		if (bWasSuccessful)
		{
			CONVAI_LOG(ConvaiNarrativeHTTP, Warning, TEXT("HTTP request succeded - But response pointer is invalid"));
		}
		else
		{
			CONVAI_LOG(ConvaiNarrativeHTTP, Warning, TEXT("HTTP request failed - Response pointer is invalid"));
		}

		failed();
		return;
	}

	if (!bWasSuccessful || Response->GetResponseCode() < 200 || Response->GetResponseCode() > 299)
	{
		CONVAI_LOG(ConvaiNarrativeHTTP, Warning, TEXT("HTTP request failed with code %d"), Response->GetResponseCode());
		CONVAI_LOG(ConvaiNarrativeHTTP, Warning, TEXT("Response:%s"), *Response->GetContentAsString());

		failed();
		return;
	}

	TArray<TSharedPtr<FJsonValue>> JsonArray;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());

	if (FJsonSerializer::Deserialize(Reader, JsonArray)) // Deserialize into a JSON array
	{
		for (const TSharedPtr<FJsonValue>& Val : JsonArray)
		{
			FNarrativeSection Section;
			if (FJsonObjectConverter::JsonObjectToUStruct(Val->AsObject().ToSharedRef(), FNarrativeSection::StaticStruct(), &Section))
			{
				NarrativeSections.Add(Section);
			}
		}
		success();
		return;
	}
}

void UFetchNarrativeSectionsProxy::failed()
{
	//auto InvocationList = OnFailure.GetAllObjects();
	//CONVAI_LOG(ConvaiNarrativeHTTP, Log, TEXT("OnFailure delegate is bound to %d functions before invokation."), InvocationList.Num());

	OnFailure.Broadcast(NarrativeSections);
	//CONVAI_LOG(ConvaiNarrativeHTTP, Log, TEXT("OnFailure delegate is bound to %d functions after invokation."), InvocationList.Num());
	finish();
}

void UFetchNarrativeSectionsProxy::success()
{
	//auto InvocationList = OnSuccess.GetAllObjects();
	//CONVAI_LOG(ConvaiNarrativeHTTP, Log, TEXT("OnSuccess delegate is bound to %d functions before invokation."), InvocationList.Num());

	OnSuccess.Broadcast(NarrativeSections);
	//CONVAI_LOG(ConvaiNarrativeHTTP, Log, TEXT("OnSuccess delegate is bound to %d functions after invokation."), InvocationList.Num());
	finish();
}

void UFetchNarrativeSectionsProxy::finish()
{
}


UFetchNarrativeTriggersProxy* UFetchNarrativeTriggersProxy::FetchNarrativeTriggers(UObject* WorldContextObject, FString InCharacterId)
{
	UFetchNarrativeTriggersProxy* Proxy = NewObject<UFetchNarrativeTriggersProxy>();
	Proxy->WorldPtr = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	Proxy->CharacterId = InCharacterId;
	return Proxy;
}

void UFetchNarrativeTriggersProxy::Activate()
{

	UWorld* World = WorldPtr.Get();

	if (!World)
	{
		CONVAI_LOG(ConvaiNarrativeHTTP, Warning, TEXT("Could not get a pointer to world!"));
		failed();
		return;
	}

	FHttpModule* Http = &FHttpModule::Get();
	if (!Http)
	{
		CONVAI_LOG(ConvaiNarrativeHTTP, Warning, TEXT("Could not get a pointer to http module!"));
		failed();
		return;
	}

	TPair<FString, FString> AuthHeaderAndKey = UConvaiUtils::GetAuthHeaderAndKey();
	FString AuthKey = AuthHeaderAndKey.Value;
	FString AuthHeader = AuthHeaderAndKey.Key;

	// Form Validation
	if (
		!UConvaiFormValidation::ValidateAuthKey(AuthKey)
		|| !UConvaiFormValidation::ValidateCharacterID(CharacterId)
		)
	{
		CONVAI_LOG(ConvaiNarrativeHTTP, Warning, TEXT("UFetchNarrativeTriggersProxy::Activate Invalid Character or API key"));
		failed();
		return;
	}
	FHttpModule* HttpModule = &FHttpModule::Get();
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = HttpModule->CreateRequest();
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UFetchNarrativeTriggersProxy::OnHttpRequestCompleted);
	HttpRequest->SetURL(UConvaiURL::GetFullURL(TEXT("character/narrative/list-triggers"), false));
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetHeader(AuthHeader, AuthKey);
	HttpRequest->SetContentAsString(FString::Printf(TEXT("{\"character_id\": \"%s\"}"), *CharacterId));
	HttpRequest->ProcessRequest();
}

void UFetchNarrativeTriggersProxy::OnHttpRequestCompleted(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	NarrativeTriggers.Empty();
	if (!Response)
	{
		if (bWasSuccessful)
		{
			CONVAI_LOG(ConvaiNarrativeHTTP, Warning, TEXT("HTTP request succeded - But response pointer is invalid"));
		}
		else
		{
			CONVAI_LOG(ConvaiNarrativeHTTP, Warning, TEXT("HTTP request failed - Response pointer is invalid"));
		}

		failed();
		return;
	}

	if (!bWasSuccessful || Response->GetResponseCode() < 200 || Response->GetResponseCode() > 299)
	{
		CONVAI_LOG(ConvaiNarrativeHTTP, Warning, TEXT("HTTP request failed with code %d"), Response->GetResponseCode());
		CONVAI_LOG(ConvaiNarrativeHTTP, Warning, TEXT("Response:%s"), *Response->GetContentAsString());

		failed();
		return;
	}

	TArray<TSharedPtr<FJsonValue>> JsonArray;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());

	if (FJsonSerializer::Deserialize(Reader, JsonArray)) // Deserialize into a JSON array
	{
		for (const TSharedPtr<FJsonValue>& Val : JsonArray)
		{
			FNarrativeTrigger Trigger;
			if (FJsonObjectConverter::JsonObjectToUStruct(Val->AsObject().ToSharedRef(), FNarrativeTrigger::StaticStruct(), &Trigger))
			{
				NarrativeTriggers.Add(Trigger);
			}
		}
		success();
		return;
	}
}

void UFetchNarrativeTriggersProxy::failed()
{
	//auto InvocationList = OnFailure.GetAllObjects();
	//CONVAI_LOG(ConvaiNarrativeHTTP, Log, TEXT("OnFailure delegate is bound to %d functions before invokation."), InvocationList.Num());

	OnFailure.Broadcast(NarrativeTriggers);
	//CONVAI_LOG(ConvaiNarrativeHTTP, Log, TEXT("OnFailure delegate is bound to %d functions after invokation."), InvocationList.Num());
	finish();
}

void UFetchNarrativeTriggersProxy::success()
{
	//auto InvocationList = OnSuccess.GetAllObjects();
	//CONVAI_LOG(ConvaiNarrativeHTTP, Log, TEXT("OnSuccess delegate is bound to %d functions before invokation."), InvocationList.Num());

	OnSuccess.Broadcast(NarrativeTriggers);
	//CONVAI_LOG(ConvaiNarrativeHTTP, Log, TEXT("OnSuccess delegate is bound to %d functions after invokation."), InvocationList.Num());
	finish();
}

void UFetchNarrativeTriggersProxy::finish()
{
}

