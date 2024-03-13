// Copyright 2022 Convai Inc. All Rights Reserved.

#include "ConvaiNarrativeDesignProxy.h"
#include "ConvaiUtils.h"
#include "../Convai.h"

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
		UE_LOG(ConvaiNarrativeHTTP, Warning, TEXT("Could not get a pointer to world!"));
		failed();
		return;
	}

	FHttpModule* Http = &FHttpModule::Get();
	if (!Http)
	{
		UE_LOG(ConvaiNarrativeHTTP, Warning, TEXT("Could not get a pointer to http module!"));
		failed();
		return;
	}

	// Get the list of all functions bound to the OnCompleted delegate

	// Log the number of bindings

	//UE_LOG(LogTemp, Log, TEXT("OnCompleted delegate is bound to %s object."), *InvocationList[0]->GetName());

	FString API_key = Convai::Get().GetConvaiSettings()->API_Key;
	// Form Validation
	if (
		!UConvaiFormValidation::ValidateAPIKey(API_key)
		|| !UConvaiFormValidation::ValidateCharacterID(CharacterId)
		)
	{
		UE_LOG(ConvaiNarrativeHTTP, Warning, TEXT("UFetchNarrativeSectionsProxy::Activate Invalid Character or API key"));
		failed();
		return;
	}
	FHttpModule* HttpModule = &FHttpModule::Get();
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = HttpModule->CreateRequest();
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UFetchNarrativeSectionsProxy::OnHttpRequestCompleted);
	HttpRequest->SetURL(TEXT("https://api.convai.com/character/narrative/list-sections"));
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetHeader(TEXT("CONVAI-API-KEY"), API_key);
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
			UE_LOG(ConvaiNarrativeHTTP, Warning, TEXT("HTTP request succeded - But response pointer is invalid"));
		}
		else
		{
			UE_LOG(ConvaiNarrativeHTTP, Warning, TEXT("HTTP request failed - Response pointer is invalid"));
		}

		failed();
		return;
	}

	if (!bWasSuccessful || Response->GetResponseCode() < 200 || Response->GetResponseCode() > 299)
	{
		UE_LOG(ConvaiNarrativeHTTP, Warning, TEXT("HTTP request failed with code %d"), Response->GetResponseCode());
		UE_LOG(ConvaiNarrativeHTTP, Warning, TEXT("Response:%s"), *Response->GetContentAsString());

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
	//UE_LOG(ConvaiNarrativeHTTP, Log, TEXT("OnFailure delegate is bound to %d functions before invokation."), InvocationList.Num());

	OnFailure.Broadcast(NarrativeSections);
	//UE_LOG(ConvaiNarrativeHTTP, Log, TEXT("OnFailure delegate is bound to %d functions after invokation."), InvocationList.Num());
	finish();
}

void UFetchNarrativeSectionsProxy::success()
{
	//auto InvocationList = OnSuccess.GetAllObjects();
	//UE_LOG(ConvaiNarrativeHTTP, Log, TEXT("OnSuccess delegate is bound to %d functions before invokation."), InvocationList.Num());

	OnSuccess.Broadcast(NarrativeSections);
	//UE_LOG(ConvaiNarrativeHTTP, Log, TEXT("OnSuccess delegate is bound to %d functions after invokation."), InvocationList.Num());
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
		UE_LOG(ConvaiNarrativeHTTP, Warning, TEXT("Could not get a pointer to world!"));
		failed();
		return;
	}

	FHttpModule* Http = &FHttpModule::Get();
	if (!Http)
	{
		UE_LOG(ConvaiNarrativeHTTP, Warning, TEXT("Could not get a pointer to http module!"));
		failed();
		return;
	}

	// Get the list of all functions bound to the OnCompleted delegate

	// Log the number of bindings

	//UE_LOG(LogTemp, Log, TEXT("OnCompleted delegate is bound to %s object."), *InvocationList[0]->GetName());

	FString API_key = Convai::Get().GetConvaiSettings()->API_Key;
	// Form Validation
	if (
		!UConvaiFormValidation::ValidateAPIKey(API_key)
		|| !UConvaiFormValidation::ValidateCharacterID(CharacterId)
		)
	{
		UE_LOG(ConvaiNarrativeHTTP, Warning, TEXT("UFetchNarrativeTriggersProxy::Activate Invalid Character or API key"));
		failed();
		return;
	}
	FHttpModule* HttpModule = &FHttpModule::Get();
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = HttpModule->CreateRequest();
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UFetchNarrativeTriggersProxy::OnHttpRequestCompleted);
	HttpRequest->SetURL(TEXT("https://api.convai.com/character/narrative/list-triggers"));
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetHeader(TEXT("CONVAI-API-KEY"), API_key);
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
			UE_LOG(ConvaiNarrativeHTTP, Warning, TEXT("HTTP request succeded - But response pointer is invalid"));
		}
		else
		{
			UE_LOG(ConvaiNarrativeHTTP, Warning, TEXT("HTTP request failed - Response pointer is invalid"));
		}

		failed();
		return;
	}

	if (!bWasSuccessful || Response->GetResponseCode() < 200 || Response->GetResponseCode() > 299)
	{
		UE_LOG(ConvaiNarrativeHTTP, Warning, TEXT("HTTP request failed with code %d"), Response->GetResponseCode());
		UE_LOG(ConvaiNarrativeHTTP, Warning, TEXT("Response:%s"), *Response->GetContentAsString());

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
	//UE_LOG(ConvaiNarrativeHTTP, Log, TEXT("OnFailure delegate is bound to %d functions before invokation."), InvocationList.Num());

	OnFailure.Broadcast(NarrativeTriggers);
	//UE_LOG(ConvaiNarrativeHTTP, Log, TEXT("OnFailure delegate is bound to %d functions after invokation."), InvocationList.Num());
	finish();
}

void UFetchNarrativeTriggersProxy::success()
{
	//auto InvocationList = OnSuccess.GetAllObjects();
	//UE_LOG(ConvaiNarrativeHTTP, Log, TEXT("OnSuccess delegate is bound to %d functions before invokation."), InvocationList.Num());

	OnSuccess.Broadcast(NarrativeTriggers);
	//UE_LOG(ConvaiNarrativeHTTP, Log, TEXT("OnSuccess delegate is bound to %d functions after invokation."), InvocationList.Num());
	finish();
}

void UFetchNarrativeTriggersProxy::finish()
{
}

