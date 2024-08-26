// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Net/OnlineBlueprintCallProxyBase.h"
#include "Http.h"
#include "ConvaiSpeechToTextProxy.generated.h"

//http log
DECLARE_LOG_CATEGORY_EXTERN(ConvaiS2THttpLog, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FS2THttpResponseCallbackSignature, FString, Response);


class USoundWave;

/**
 * 
 */
UCLASS()
class UConvaiSpeechToTextProxy : public UOnlineBlueprintCallProxyBase
{
	GENERATED_BODY()



	// Called when there is a successful http response
	UPROPERTY(BlueprintAssignable)
	FS2THttpResponseCallbackSignature OnSuccess;

	// Called when there is an unsuccessful http response
	UPROPERTY(BlueprintAssignable)
	FS2THttpResponseCallbackSignature OnFailure;


	/**
	 *    Initiates a post request to the Speech To Text API.
	 *    @param URL				The url of the API endpoint.
	 *    @param API_key			The API key issued from the website
	 *    @param SoundWave			SoundWave that is to be transfered to text
	 */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DisplayName = "Convai Speech To Text From SoundWave", WorldContext = "WorldContextObject"), Category = "Convai|Http")
	static UConvaiSpeechToTextProxy* CreateSpeech2TextFromSoundWaveQueryProxy(UObject* WorldContextObject, USoundWave* SoundWave);




	/**
	 *    Initiates a post request to the Speech To Text API.
	 *    @param URL				The url of the API endpoint.
	 *    @param API_key			The API key issued from the website
	 *    @param Payload			Byte array containing the wav file
	 *	  @param bStereo			Set to true if the wav file has 2 channels
	 */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DisplayName = "Convai Speech To Text From byte array", WorldContext = "WorldContextObject"), Category = "Convai|Http")
	static UConvaiSpeechToTextProxy* CreateSpeech2TextFromArrayQueryProxy(UObject* WorldContextObject, TArray<uint8> Payload);

	/**
	 *    Initiates a post request to the Speech To Text API.
	 *    @param URL				The url of the API endpoint.
	 *    @param API_key			The API key issued from the website
	 *    @param filename			The name of the wav file to be sent
	 *	  @param bStereo			Set to true if the wav file has 2 channels
	 */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DisplayName = "Convai Speech To Text From file", WorldContext = "WorldContextObject"), Category = "Convai|Http")
	static UConvaiSpeechToTextProxy* CreateSpeech2TextFromFileNameQueryProxy(UObject* WorldContextObject, FString filename);



	virtual void Activate() override;

	void onHttpRequestComplete(FHttpRequestPtr RequestPtr, FHttpResponsePtr ResponsePtr, bool bWasSuccessful);

	void failed();
	void success();
	void finish();


	FString URL;
	FString filename;
	TArray<uint8> Payload;
	bool bStereo;
	FString Response;

	// Pointer to the world
	TWeakObjectPtr<UWorld> WorldPtr;
};
