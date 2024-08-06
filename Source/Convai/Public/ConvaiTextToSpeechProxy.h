// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Net/OnlineBlueprintCallProxyBase.h"
#include "Http.h"
#include "ConvaiDefinitions.h"
#include "ConvaiTextToSpeechProxy.generated.h"

class USoundWave;



//http log
DECLARE_LOG_CATEGORY_EXTERN(ConvaiT2SHttpLog, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FT2SHttpResponseCallbackSignature, USoundWave*, Wave);





/**
 * 
 */
UCLASS()
class UConvaiTextToSpeechProxy : public UOnlineBlueprintCallProxyBase
{
	GENERATED_BODY()



	// Called when there is a successful http response
	UPROPERTY(BlueprintAssignable)
	FT2SHttpResponseCallbackSignature OnSuccess;

	// Called when there is an unsuccessful http response
	UPROPERTY(BlueprintAssignable)
	FT2SHttpResponseCallbackSignature OnFailure;




	/**
	 *    Initiates a post request to the Speech To Text API.
	 *    @param API_key			The API key issued from the website
	 *    @param Transcript			The text to be transformed to voice
	 *	  @param Voice				The voice type to be used
	 */
	//UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DisplayName = "Convai Text To Speech", WorldContext = "WorldContextObject"), Category = "Convai|Http")
	//static UConvaiTextToSpeechProxy* CreateTextToSpeechQueryProxy(UObject* WorldContextObject, FString Transcript, ETTS_Voice_Type Voice);


	/**
	 *    Initiates a post request to the Speech To Text API.
	 *    @param API_key			The API key issued from the website
	 *    @param Transcript			The text to be transformed to voice
	 *	  @param Voice				The voice type to be used
	 */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DisplayName = "Convai Text To Speech", WorldContext = "WorldContextObject"), Category = "Convai|Http")
	static UConvaiTextToSpeechProxy* CreateTextToSpeechQueryProxy(UObject* WorldContextObject, FString Transcript, FString Voice);


	virtual void Activate() override;

	void onHttpRequestComplete(FHttpRequestPtr RequestPtr, FHttpResponsePtr ResponsePtr, bool bWasSuccessful);

	void failed();
	void success();
	void finish();


	FString URL;
	FString Transcript;
	ETTS_Voice_Type Voice;
	FString VoiceStr;

	USoundWave* SoundWave;

	// Pointer to the world
	TWeakObjectPtr<UWorld> WorldPtr;
};
