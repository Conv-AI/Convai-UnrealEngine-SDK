// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Net/OnlineBlueprintCallProxyBase.h"
#include "ConvaiDefinitions.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "JsonUtilities.h"
#include "ConvaiNarrativeDesignProxy.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(ConvaiNarrativeHTTP, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFetchNarrativeSectionsResponseMulticastCallbackSignature, const TArray<FNarrativeSection>&, NarrativeSections);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFetchNarrativeTriggersResponseMulticastCallbackSignature, const TArray<FNarrativeTrigger>&, NarrativeTriggers);

UCLASS()
class CONVAI_API UFetchNarrativeSectionsProxy : public UOnlineBlueprintCallProxyBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FFetchNarrativeSectionsResponseMulticastCallbackSignature OnSuccess;
	UPROPERTY(BlueprintAssignable)
	FFetchNarrativeSectionsResponseMulticastCallbackSignature OnFailure;

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DisplayName = "Convai Fetch Narrative Sections", WorldContext = "WorldContextObject"), Category = "Convai|REST API")
	static UFetchNarrativeSectionsProxy* FetchNarrativeSections(UObject* WorldContextObject, FString CharacterId);

	virtual void Activate() override;

	TArray<FNarrativeSection> NarrativeSections;

private:
	void OnHttpRequestCompleted(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	FString CharacterId;
	TWeakObjectPtr<UWorld> WorldPtr;


	void failed();
	void success();
	void finish();
};

UCLASS()
class CONVAI_API UFetchNarrativeTriggersProxy : public UOnlineBlueprintCallProxyBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FFetchNarrativeTriggersResponseMulticastCallbackSignature OnSuccess;
	UPROPERTY(BlueprintAssignable)
	FFetchNarrativeTriggersResponseMulticastCallbackSignature OnFailure;

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DisplayName = "Convai Fetch Narrative Triggers", WorldContext = "WorldContextObject"), Category = "Convai|REST API")
	static UFetchNarrativeTriggersProxy* FetchNarrativeTriggers(UObject* WorldContextObject, FString CharacterId);

	virtual void Activate() override;

	TArray<FNarrativeTrigger> NarrativeTriggers;

private:
	void OnHttpRequestCompleted(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	FString CharacterId;
	TWeakObjectPtr<UWorld> WorldPtr;


	void failed();
	void success();
	void finish();
};
