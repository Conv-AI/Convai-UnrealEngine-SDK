// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Net/OnlineBlueprintCallProxyBase.h"
#include "Http.h"
#include "ConvaiDefinitions.h"
#include "ConvaiChatBotProxy.generated.h"



//http log
DECLARE_LOG_CATEGORY_EXTERN(ConvaiBotHttpLog, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FBotQueryHttpResponseCallbackSignature, USoundWave*, AudioContent, FString, BotText, FString, NewSessionID, FString, ClassifiedAction);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FBotQueryHttpResponseWithUserTextCallbackSignature, USoundWave*, AudioContent, FString, BotText, FString, UserQuery, FString, NewSessionID, FString, ClassifiedAction);

class USoundWave;
class UTexture2D;

/**
 * 
 */
UCLASS()
class UConvaiChatBotQueryProxy : public UOnlineBlueprintCallProxyBase
{
	GENERATED_BODY()

	// Called when there is a successful http response
	UPROPERTY(BlueprintAssignable)
		FBotQueryHttpResponseCallbackSignature OnSuccess;

	// Called when there is an unsuccessful http response
	UPROPERTY(BlueprintAssignable)
		FBotQueryHttpResponseCallbackSignature OnFailure;


	/**
	 *	  @param UserQuery					The text input to the character
	 *	  @param VoiceResponse				Whether or not we want a voice response
	 *	  @param CharID						The character ID that we want to get response from
	 *	  @param SessionID					The session ID used to track memory of a previous conversation
	 *	  @param Classification				Whether or not use classification
	 *	  @param ClassLabels				Array of classes to output a class from
	 */
	UFUNCTION(BlueprintCallable, meta = (/*DeprecatedFunction, DeprecationMessage = "Use Send Text in 'Convai Player' component for better latency",*/ BlueprintInternalUseOnly = "true", DisplayName = "DEPRECEATED Convai Chatbot Get Response", WorldContext = "WorldContextObject"), Category = "Convai|DEPRECATED")
	static UConvaiChatBotQueryProxy* CreateChatBotQueryProxy(UObject* WorldContextObject, 
		FString UserQuery, 
		bool VoiceResponse, 
		FString CharID, 
		FString SessionID, 
		bool Classification,
		TArray<FString> ClassLabels);


	virtual void Activate() override;

	void onHttpRequestComplete(FHttpRequestPtr RequestPtr, FHttpResponsePtr ResponsePtr, bool bWasSuccessful);

	void failed();
	void success();
	void finish();

	FString URL;
	FString UserQuery;
	bool VoiceResponse;
	FString CharID;
	FString SessionID;
	bool Classification;
	TArray<FString> ClassLabels;

	// Outputs
	USoundWave* AudioContent;
	FString BotText;
	FString NewSessionID;
	FString ClassifiedAction;

	// Pointer to the world
	TWeakObjectPtr<UWorld> WorldPtr;
};


/**
 *
 */
UCLASS()
class UConvaiChatBotQueryFromAudioProxy : public UOnlineBlueprintCallProxyBase
{
	GENERATED_BODY()

		// Called when there is a successful http response
		UPROPERTY(BlueprintAssignable)
		FBotQueryHttpResponseWithUserTextCallbackSignature OnSuccess;

	// Called when there is an unsuccessful http response
	UPROPERTY(BlueprintAssignable)
		FBotQueryHttpResponseWithUserTextCallbackSignature OnFailure;




	/**
	 *    Get a response from a character using a path to an audio file
	 *	  @param Filename					The path to the audio file, it can be relative to the game or an absolute path
	 *	  @param VoiceResponse				Whether or not we want a voice response
	 *	  @param CharID						The character ID that we want to get response from
	 *	  @param SessionID					The session ID used to track memory of a previous conversation
	 *	  @param Classification				Whether or not use classification
	 *	  @param ClassLabels				Array of classes to output a class from
	 */
	UFUNCTION(BlueprintCallable, meta = (/*DeprecatedFunction, DeprecationMessage = "Use Start Talking in 'Convai Player' component for better latency",*/ BlueprintInternalUseOnly = "true", DisplayName = "DEPRECEATED Convai Chatbot Get Response From Audio file", WorldContext = "WorldContextObject"), Category = "Convai|DEPRECATED")
		static UConvaiChatBotQueryFromAudioProxy* CreateChatBotQueryFromAudioProxy(
			UObject* WorldContextObject,
			FString Filename,
			bool VoiceResponse,
			FString CharID,
			FString SessionID,
			bool Classification,
			TArray<FString> ClassLabels);

	/**
	 *    Get a response from a character using a sound wave
	 *	  @param SoundWave					The audio sound wave
	 *	  @param VoiceResponse				Whether or not we want a voice response
	 *	  @param CharID						The character ID that we want to get response from
	 *	  @param SessionID					The session ID used to track memory of a previous conversation
	 *	  @param Classification				Whether or not use classification
	 *	  @param ClassLabels				Array of classes to output a class from
	 */
	UFUNCTION(BlueprintCallable, meta = (/*DeprecatedFunction, DeprecationMessage = "Use Start Talking in 'Convai Player' component for better latency",*/ BlueprintInternalUseOnly = "true", DisplayName = "DEPRECEATED Convai Chatbot Get Response From SoundWav", WorldContext = "WorldContextObject"), Category = "Convai|DEPRECATED")
		static UConvaiChatBotQueryFromAudioProxy* CreateChatBotQueryFromSoundWavProxy(
			UObject* WorldContextObject,
			USoundWave* SoundWave,
			bool VoiceResponse,
			FString CharID,
			FString SessionID,
			bool Classification,
			TArray<FString> ClassLabels);


	virtual void Activate() override;

	void onHttpRequestComplete(FHttpRequestPtr RequestPtr, FHttpResponsePtr ResponsePtr, bool bWasSuccessful);

	void failed();
	void success();
	void finish();

	FString URL;
	TArray<uint8> Payload;
	bool VoiceResponse;
	FString CharID;
	FString SessionID;
	bool Classification;
	TArray<FString> ClassLabels;

	// Outputs
	USoundWave* AudioContent;
	FString BotText;
	FString UserQuery;
	FString NewSessionID;
	FString ClassifiedAction;

	// Pointer to the world
	TWeakObjectPtr<UWorld> WorldPtr;
};


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBotCreateHttpResponseCallbackSignature, FString, CharID);

UCLASS()
class UConvaiChatBotCreateProxy : public UOnlineBlueprintCallProxyBase
{
	GENERATED_BODY()

		// Called when there is a successful http response
		UPROPERTY(BlueprintAssignable)
		FBotCreateHttpResponseCallbackSignature OnSuccess;

	// Called when there is an unsuccessful http response
	UPROPERTY(BlueprintAssignable)
		FBotCreateHttpResponseCallbackSignature OnFailure;




	/**
	 *    Creates a new character and outputs its character ID
	 *	  @param CharName				The character name
	 *	  @param Voice					The voice type to be used
	 *	  @param Backstory				The backstory for the character
	 */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DisplayName = "Convai Create Character", WorldContext = "WorldContextObject"), Category = "Convai|REST API")
		static UConvaiChatBotCreateProxy* CreateCharacterCreateProxy(UObject* WorldContextObject, FString CharName, FString Voice, FString Backstory);

	UFUNCTION(BlueprintCallable, meta = (DeprecatedFunction, DeprecationMessage = "Use \"Convai Create Character\" instead", BlueprintInternalUseOnly = "true", DisplayName = "Convai Get All Chatbots IDs", WorldContext = "WorldContextObject"), Category = "Convai|REST API")
		static UConvaiChatBotCreateProxy* CreateChatBotCreateProxy(UObject* WorldContextObject, FString CharName, FString Voice, FString Backstory)
	{
		return CreateCharacterCreateProxy(WorldContextObject, CharName, Voice, Backstory);
	}



	virtual void Activate() override;

	void onHttpRequestComplete(FHttpRequestPtr RequestPtr, FHttpResponsePtr ResponsePtr, bool bWasSuccessful);

	void failed();
	void success();
	void finish();

	FString URL;
	FString CharName;
	FString Voice;
	FString Backstory;


	// Outputs
	FString CharID;

	// Pointer to the world
	TWeakObjectPtr<UWorld> WorldPtr;
};



DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBotUpdateHttpResponseCallbackSignature);

UCLASS()
class UConvaiChatBotUpdateProxy : public UOnlineBlueprintCallProxyBase
{
	GENERATED_BODY()

		// Called when there is a successful http response
		UPROPERTY(BlueprintAssignable)
		FBotUpdateHttpResponseCallbackSignature OnSuccess;

	// Called when there is an unsuccessful http response
	UPROPERTY(BlueprintAssignable)
		FBotUpdateHttpResponseCallbackSignature OnFailure;




	/**
	 *    Updates an already created character, IMPORTANT: if you do not want to update some property (e.g. Name) then leave it as empty string and it will not update
	 *	  @param CharID				The character's ID that is going to be updated
	 *	  @param NewVoice				The new voice type to be used, leave empty if you do not want to update
	 *	  @param NewBackstory			The new backstory, leave empty if you do not want to update
	 *	  @param NewCharName			The new character name, leave empty if you do not want to update
	 */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DisplayName = "Convai Update Character", WorldContext = "WorldContextObject"), Category = "Convai|REST API")
		static UConvaiChatBotUpdateProxy* CreateCharacterUpdateProxy(UObject* WorldContextObject, FString CharID, FString NewVoice, FString NewBackstory, FString NewCharName, FString NewLanguage);

	UFUNCTION(BlueprintCallable, meta = (DeprecatedFunction, DeprecationMessage = "Use \"Convai Update Character\" instead", BlueprintInternalUseOnly = "true", DisplayName = "Convai Update Chatbot", WorldContext = "WorldContextObject"), Category = "Convai|REST API")
		static UConvaiChatBotUpdateProxy* CreateChatBotUpdateProxy(UObject* WorldContextObject, FString CharID, FString NewVoice, FString NewBackstory, FString NewCharName, FString NewLanguage)
	{
		return CreateCharacterUpdateProxy(WorldContextObject, CharID, NewVoice, NewBackstory, NewCharName, NewLanguage);
	}

	virtual void Activate() override;

	void onHttpRequestComplete(FHttpRequestPtr RequestPtr, FHttpResponsePtr ResponsePtr, bool bWasSuccessful);

	void failed();
	void success();
	void finish();

	FString URL;
	FString CharID;
	FString NewCharName;
	FString NewVoice;
	FString NewBackstory;
	FString NewLanguage;

	// Pointer to the world
	TWeakObjectPtr<UWorld> WorldPtr;
};



DECLARE_DYNAMIC_MULTICAST_DELEGATE_SevenParams(FBotGetDetailsHttpResponseCallbackSignature
	, FString, character_name
	, FString, voice_type
	, FString, backstory
	, FString, LanguageCode
	, bool, HasReadyPlayerMeLink
	, FString, ReadyPlayerMeLink
	, FString, AvatarImageLink);

UCLASS()
class UConvaiChatBotGetDetailsProxy : public UOnlineBlueprintCallProxyBase
{
	GENERATED_BODY()

public:
	// Called when there is a successful http response
	UPROPERTY(BlueprintAssignable)
		FBotGetDetailsHttpResponseCallbackSignature OnSuccess;

	// Called when there is an unsuccessful http response
	UPROPERTY(BlueprintAssignable)
		FBotGetDetailsHttpResponseCallbackSignature OnFailure;




	/**
	 *    Initiates a post request to the Character API.
	 *    @param CharID				The character's ID, you can find it from the Dashboard on the Convai.com website
	 */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DisplayName = "Convai Get Character Details", WorldContext = "WorldContextObject"), Category = "Convai|REST API")
		static UConvaiChatBotGetDetailsProxy* CreateCharacterGetDetailsProxy(UObject* WorldContextObject, FString CharID);

	UFUNCTION(BlueprintCallable, meta = (DeprecatedFunction, DeprecationMessage = "Use \"Convai Get Character Details\" instead", BlueprintInternalUseOnly = "true", DisplayName = "Convai Get All Chatbots IDs", WorldContext = "WorldContextObject"), Category = "Convai|REST API")
		static UConvaiChatBotGetDetailsProxy* CreateChatBotGetDetailsProxy(UObject* WorldContextObject, FString CharID)
	{
		return CreateCharacterGetDetailsProxy(WorldContextObject, CharID);
	}


	virtual void Activate() override;

	void onHttpRequestComplete(FHttpRequestPtr RequestPtr, FHttpResponsePtr ResponsePtr, bool bWasSuccessful);

	void failed();
	void success();
	void finish();

	// inputs
	FString URL;
	FString CharID;

	// outputs
	FString character_name;
	//FString user_id;
	FString voice_type;
	//FString timestamp;
	FString backstory;
	FString LanguageCode;
	bool HasReadyPlayerMeLink;
	FString ReadyPlayerMeLink;
	FString AvatarImageLink;


	// Pointer to the world
	TWeakObjectPtr<UWorld> WorldPtr;
};






DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBotGetCharsHttpResponseCallbackSignature, const TArray<FString>&, CharIDs);

UCLASS()
class UConvaiChatBotGetCharsProxy : public UOnlineBlueprintCallProxyBase
{
	GENERATED_BODY()

	// Called when there is a successful http response
	UPROPERTY(BlueprintAssignable)
	FBotGetCharsHttpResponseCallbackSignature OnSuccess;

	// Called when there is an unsuccessful http response
	UPROPERTY(BlueprintAssignable)
	FBotGetCharsHttpResponseCallbackSignature OnFailure;

	/**
	 *    Gets all character IDs created by the user.
	 */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DisplayName = "Convai Get All Characters IDs", WorldContext = "WorldContextObject"), Category = "Convai|REST API")
	static UConvaiChatBotGetCharsProxy* CreateCharacterGetCharsProxy(UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, meta = (DeprecatedFunction, DeprecationMessage = "Use \"Convai Get All Characters IDs\" instead", BlueprintInternalUseOnly = "true", DisplayName = "Convai Get All Chatbots IDs", WorldContext = "WorldContextObject"), Category = "Convai|REST API")
		static UConvaiChatBotGetCharsProxy* CreateChatBotGetCharsProxy(UObject* WorldContextObject)
	{
		return CreateCharacterGetCharsProxy(WorldContextObject);
	}

	virtual void Activate() override;

	void onHttpRequestComplete(FHttpRequestPtr RequestPtr, FHttpResponsePtr ResponsePtr, bool bWasSuccessful);

	void failed();
	void success();
	void finish();

	FString URL;

	// Outputs
	TArray<FString> CharIDs;

	// Pointer to the world
	TWeakObjectPtr<UWorld> WorldPtr;
};













DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDownloadImageHttpResponseCallbackSignature, UTexture2D*, Image);

UCLASS()
class UConvaiDownloadImageProxy : public UOnlineBlueprintCallProxyBase
{
	GENERATED_BODY()

	// Called when there is a successful http response
	UPROPERTY(BlueprintAssignable)
	FDownloadImageHttpResponseCallbackSignature OnSuccess;

	// Called when there is an unsuccessful http response
	UPROPERTY(BlueprintAssignable)
	FDownloadImageHttpResponseCallbackSignature OnFailure;


	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DisplayName = "Convai Download Image", WorldContext = "WorldContextObject"), Category = "Convai|REST API")
	static UConvaiDownloadImageProxy* CreateDownloadImageProxy(UObject* WorldContextObject, FString URL);

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DisplayName = "Convai Download Image using RPM Link", WorldContext = "WorldContextObject"), Category = "Convai|REST API")
	static UConvaiDownloadImageProxy* CreateDownloadImageForRPMProxy(UObject* WorldContextObject, FString URL);

	virtual void Activate() override;

	void onHttpRequestComplete(FHttpRequestPtr RequestPtr, FHttpResponsePtr ResponsePtr, bool bWasSuccessful);

	void failed();
	void success();
	void finish();

	FString URL;

	// Outputs
	UTexture2D* Image;

	// Pointer to the world
	TWeakObjectPtr<UWorld> WorldPtr;
};






USTRUCT(BlueprintType)
struct FAvailableVoices
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, category = "Convai|Language")
	TMap<FString, FVoiceLanguageStruct> AvailableVoices;

	FAvailableVoices()
	{}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGetAvailableVoicesCallbackSignature, const FAvailableVoices&, AvailableVoices);

UCLASS()
class UConvaiGetAvailableVoicesProxy : public UOnlineBlueprintCallProxyBase
{
	GENERATED_BODY()

public:
	// Called when there is a successful http response
	UPROPERTY(BlueprintAssignable)
	FGetAvailableVoicesCallbackSignature OnSuccess;

	// Called when there is an unsuccessful http response
	UPROPERTY(BlueprintAssignable)
	FGetAvailableVoicesCallbackSignature OnFailure;

	/**
	 *    Gets all available voices.
	 */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DisplayName = "Convai Get Available Voices"), Category = "Convai|REST API")
	static UConvaiGetAvailableVoicesProxy* CreateGetAvailableVoicesProxy(EVoiceType VoiceType, ELanguageType LanguageType, EGenderType Gender);

	virtual void Activate() override;

	void onHttpRequestComplete(FHttpRequestPtr RequestPtr, FHttpResponsePtr ResponsePtr, bool bWasSuccessful);

	void failed();
	void success();

	FString URL;
	EVoiceType FilterVoiceType;
	ELanguageType FilterLanguageType;
	EGenderType FilterGender;

	// Output
	FAvailableVoices AvailableVoices;

private:
	// Helper functions
	bool ParseAllVoiceData(const FString& JsonString, TMap<FString, FVoiceLanguageStruct>& FilteredVoices);
	bool ParseVoiceData(TSharedPtr<FJsonObject> JsonObject, FVoiceLanguageStruct& OutVoice);	
	FString GetLanguageCodeFromEnum(ELanguageType LanguageType);
	FString GetGenderFromEnum(EGenderType GenderType);
	FString GetVoiceTypeFromEnum(EVoiceType VoiceType);
};