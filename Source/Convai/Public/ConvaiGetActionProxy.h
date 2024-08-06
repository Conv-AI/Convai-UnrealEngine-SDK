// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Net/OnlineBlueprintCallProxyBase.h"
#include "Http.h"
#include "ConvaiDefinitions.h"
#include "ConvaiGetActionProxy.generated.h"



//http log
DECLARE_LOG_CATEGORY_EXTERN(ConvaiGetActionHttpLog, Log, All);


class USoundWave;



DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGetActionQueryHttpResponseCallbackSignature, const TArray<FConvaiResultAction>&, SequenceOfActions);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FGetActionResponseQueryHttpResponseCallbackSignature, USoundWave*, AudioContent, FString, BotText, FString, UserQuery, FString, NewSessionID, const TArray<FConvaiResultAction>&, SequenceOfActions);



/**
 *
 */
UCLASS()
class UConvaiGetActionProxy : public UOnlineBlueprintCallProxyBase
{
	GENERATED_BODY()

	// Called when there is a successful http response
	UPROPERTY(BlueprintAssignable)
	FGetActionQueryHttpResponseCallbackSignature OnSuccess;

	// Called when there is an unsuccessful http response
	UPROPERTY(BlueprintAssignable)
	FGetActionQueryHttpResponseCallbackSignature OnFailure;

	/**
	 *    Initiates a post request to the Get Action API.
	 *    @param Context				Contains information about the possible actions and thesurrounding environment (e.g. objects and characters)
	 */
	UFUNCTION(BlueprintCallable, meta = (/*DeprecatedFunction, DeprecationMessage = "Use Send Text in 'Convai Player' component for better latency", BlueprintInternalUseOnly = "true",*/ DisplayName = "DEPRECATED Convai Get Action", WorldContext = "WorldContextObject"), Category = "Convai|DEPRECATED|Action API")
		static UConvaiGetActionProxy* CreateGetActionQueryProxy(
			UObject* WorldContextObject,
			FString TextQuery,
			UConvaiActionContext* Context);

	///**
	// *    Initiates a post request to the Get Action API.
	// *    @param Context				Contains information about the possible actions and thesurrounding environment (e.g. objects and characters)
	// */
	//UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DisplayName = "Convai Get Action From Audio file", WorldContext = "WorldContextObject"), Category = "Convai|Action API")
	//	static UConvaiGetActionProxy* CreateGetActionQueryFromAudioProxy(
	//		UObject* WorldContextObject, 
	//		FString Filename,
	//		UConvaiActionContext* Context);

	///**
	// *    Initiates a post request to the Get Action API.
	// *    @param Context				Contains information about the possible actions and thesurrounding environment (e.g. objects and characters)
	// */
	//UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DisplayName = "Convai Get Action From SoundWav", WorldContext = "WorldContextObject"), Category = "Convai|Action API")
	//	static UConvaiGetActionProxy* CreateGetActionQueryFromSoundWavProxy(
	//		UObject* WorldContextObject,
	//		USoundWave* SoundWave,
	//		UConvaiActionContext* Context);


	virtual void Activate() override;

	void onHttpRequestComplete(FHttpRequestPtr RequestPtr, FHttpResponsePtr ResponsePtr, bool bWasSuccessful);

	void failed();
	void success();
	void finish();

	FString URL;
	UConvaiActionContext* Context;
	FString TextQuery;

	// Outputs
	TArray<FConvaiResultAction> SequenceOfActions;

	// Pointer to the world
	TWeakObjectPtr<UWorld> WorldPtr;
};




UCLASS()
class UConvaiGetActionResponseProxy : public UOnlineBlueprintCallProxyBase
{
	GENERATED_BODY()

	// Called when there is a successful http response
	UPROPERTY(BlueprintAssignable)
	FGetActionResponseQueryHttpResponseCallbackSignature OnSuccess;

	// Called when there is an unsuccessful http response
	UPROPERTY(BlueprintAssignable)
	FGetActionResponseQueryHttpResponseCallbackSignature OnFailure;




	/**
	 *    Initiates a post request to the Get Action With Responsee API.
	 *    @param Context				Contains information about the possible actions and thesurrounding environment (e.g. objects and characters)
	 */
	UFUNCTION(BlueprintCallable, meta = (/*DeprecatedFunction, DeprecationMessage = "Use Send Text in 'Convai Player' component for better latency", BlueprintInternalUseOnly = "true",*/ DisplayName = "DEPRECATED Convai Get Action With Response", WorldContext = "WorldContextObject"), Category = "Convai|DEPRECATED|Action API")
		static UConvaiGetActionResponseProxy* CreateGetActionResponseQueryProxy(
			UObject* WorldContextObject,
			FString UserQuery,
			bool VoiceResponse,
			FString CharID,
			FString SessionID,
			UConvaiActionContext* Context);

	/**
	 *    Initiates a post request to the Get Action With Responsee API.
	 *    @param Context				Contains information about the possible actions and thesurrounding environment (e.g. objects and characters)
	 */
	UFUNCTION(BlueprintCallable, meta = (/*DeprecatedFunction, DeprecationMessage = "Use Start Talking in 'Convai Player' component for better latency", BlueprintInternalUseOnly = "true",*/ DisplayName = "Convai Get Action With Response From Audio file", WorldContext = "WorldContextObject"), Category = "Convai|DEPRECATED|Action API")
		static UConvaiGetActionResponseProxy* CreateGetActionQueryFromAudioProxy(
			UObject* WorldContextObject, 
			FString Filename,
			bool VoiceResponse,
			FString CharID,
			FString SessionID,
			UConvaiActionContext* Context);

	/**
	 *    Initiates a post request to the Get Action With Responsee API.
	 *    @param Context				Contains information about the possible actions and thesurrounding environment (e.g. objects and characters)
	 */
	UFUNCTION(BlueprintCallable, meta = (/*DeprecatedFunction, DeprecationMessage = "Use Start Talking in 'Convai Player' component for better latency", BlueprintInternalUseOnly = "true", */ DisplayName = "DEPRECATED Convai Get Action With Response From SoundWav", WorldContext = "WorldContextObject"), Category = "Convai|DEPRECATED|Action API")
		static UConvaiGetActionResponseProxy* CreateGetActionQueryFromSoundWavProxy(
			UObject* WorldContextObject,
			USoundWave* SoundWave,
			bool VoiceResponse,
			FString CharID,
			FString SessionID,
			UConvaiActionContext* Context);


	virtual void Activate() override;

	void onHttpRequestComplete(FHttpRequestPtr RequestPtr, FHttpResponsePtr ResponsePtr, bool bWasSuccessful);

	void failed();
	void success();
	void finish();

	// Inputs
	FString URL;
	UConvaiActionContext* Context;
	FString TextQuery;
	TArray<uint8> Payload;
	bool VoiceResponse;
	FString UserQuery;
	FString CharID;
	FString SessionID;
	bool bAudioInput;

	// Outputs
	USoundWave* AudioContent;
	FString BotText;
	FString NewSessionID;
	TArray<FConvaiResultAction> SequenceOfActions;

	// Pointer to the world
	TWeakObjectPtr<UWorld> WorldPtr;
};