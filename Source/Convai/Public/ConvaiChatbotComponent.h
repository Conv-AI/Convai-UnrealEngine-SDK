// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once
//#include "CoreMinimal.h"
#include "Components/AudioComponent.h"
#include "ConvaiAudioStreamer.h"
#include "ConviDefinitions.h"

#include "ConvaiChatbotComponent.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(ConvaiChatbotComponentLog, Log, All);

DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_ThreeParams(FOnTranscriptionReceivedSignature, UConvaiChatbotComponent, OnTranscriptionReceivedEvent, FString, Transcription, bool, IsTranscriptionReady, bool, IsFinal);
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_FourParams(FOnTextReceivedSignature, UConvaiChatbotComponent, OnTextReceivedEvent, FString, CharacterName, FString, BotText, float, AudioDuration, bool, IsFinal);
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_OneParam(FOnActionReceivedSignature, UConvaiChatbotComponent, OnActionReceivedEvent, const TArray<FConvaiResultAction>&, SequenceOfActions);
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_OneParam(FOnCharacterDataLoadSignature, UConvaiChatbotComponent, OnCharacterDataLoadEvent, bool, Success);
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE(FOnFailureSignature, UConvaiChatbotComponent, OnFailureEvent);
// TODO (Mohamed): Manage onDestroy/onEndPlay - should end any on-going streams

class UConvaiPlayerComponent;
class USoundWaveProcedural;
class UConvaiGRPCGetResponseProxy;
class UConvaiChatBotGetDetailsProxy;

UCLASS(meta = (BlueprintSpawnableComponent), DisplayName = "Convai Chatbot")
class UConvaiChatbotComponent : public UConvaiAudioStreamer
{
	GENERATED_BODY()
public:

	UConvaiChatbotComponent();

	void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	* Returns true, if the character is being talked to, is talking, or is processing the response.
	*/
	UFUNCTION(BlueprintPure, BlueprintCallable, Category = "Convai")
		bool IsInConversation();

	/**
	* Returns true, if the character is still processing and has not received the full response yet.
	*/
	UFUNCTION(BlueprintPure, BlueprintCallable, Category = "Convai")
		bool IsProcessing();

	/**
	* Returns true, if the character is currently talking.
	*/
	UFUNCTION(BlueprintPure, BlueprintCallable, Category = "Convai", meta = (DisplayName = "Is Talking"))
		bool GetIsTalking();

	UPROPERTY(EditAnywhere, Category = "Convai", Replicated, BlueprintSetter = LoadCharacter)
	FString CharacterID;

	UPROPERTY(BlueprintReadWrite, Category = "Convai", Replicated)
		FString CharacterName;

	UPROPERTY(BlueprintReadWrite, Category = "Convai", Replicated)
		FString VoiceType;

	UPROPERTY(BlueprintReadWrite, Category = "Convai", Replicated)
		FString Backstory;

	UPROPERTY(BlueprintReadWrite, Category = "Convai", Replicated)
		FString ReadyPlayerMeLink;

	/**
	 *    Used to track memory of a previous conversation, set to -1 means no previous conversation,
	 *	  this property will change as you talk to the character, you can save the session ID for a
	 *    conversation and then set it back later on to resume a conversation
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Convai", Replicated)
		FString SessionID = "-1";

	/**
	 *    Contains all relevant objects and characters in the scene including the (Player), and also all the actions doable by the character
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Convai")
		UConvaiEnvironment* Environment;

	/**
	 *    Time in seconds, for the character's voice audio to gradually degrade until it is completely turned off when interrupted.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Convai")
	float InterruptVoiceFadeOutDuration;

	/**
	 *    Reset the conversation with the character and remove previous memory, this is the same as setting the session ID property to -1.
	 */
	UFUNCTION(BlueprintCallable, Category = "Convai")
		void ResetConversation();	
	
	/**
	 *    Loads a new character using its ID
	 */
	UFUNCTION(BlueprintCallable, BlueprintInternalUseOnly, Category = "Convai")
	void LoadCharacter(FString NewCharacterID);

public:
	/** Called when a new action is received from the API */
	UPROPERTY(BlueprintAssignable, Category = "Convai", meta = (DisplayName = "On Actions Received"))
	FOnActionReceivedSignature OnActionReceivedEvent;

	/** Called when new text is received from the API, AudioDuration = 0 if no audio was received */
	UPROPERTY(BlueprintAssignable, Category = "Convai", meta = (DisplayName = "On Text Received"))
	FOnTextReceivedSignature OnTextReceivedEvent;

	/** Called when new transcription is available */
	UPROPERTY(BlueprintAssignable, Category = "Convai", meta = (DisplayName = "On Transcription Received"))
	FOnTranscriptionReceivedSignature OnTranscriptionReceivedEvent;

	UPROPERTY(BlueprintAssignable, Category = "Convai", meta = (DisplayName = "On Character Data Loaded"))
	FOnCharacterDataLoadSignature OnCharacterDataLoadEvent;

	/** Called there is an error */
	UPROPERTY(BlueprintAssignable, Category = "Convai", meta = (DisplayName = "On Failure"))
	FOnFailureSignature OnFailureEvent;

public:
	//UFUNCTION(BlueprintCallable, DisplayName = "Begin Transmission")
	void StartGetResponseStream(UConvaiPlayerComponent* InConvaiPlayerComponent, FString InputText, UConvaiEnvironment* InEnvironment, bool InGenerateActions, bool VoiceResponse, bool RunOnServer, uint32 InToken);

	UFUNCTION(BlueprintCallable, Category = "Convai")
	void InterruptSpeech(float InVoiceFadeOutDuration);

private:
	// UActorComponent interface
	virtual void BeginPlay() override;
	//virtual void OnRegister() override;
	//virtual void OnUnregister() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	// End UActorComponent interface

	//~ Begin UObject Interface.
	virtual void BeginDestroy() override;
	//~ End UObject Interface.

private:
	UConvaiChatBotGetDetailsProxy* ConvaiGetDetails();

	TScriptDelegate<FWeakObjectPtr> ConvaiChatBotGetDetailsDelegate;

	UFUNCTION()
	void OnConvaiGetDetailsCompleted(FString ReceivedCharacterName, FString ReceivedVoiceType, FString ReceivedBackstory, bool HasReadyPlayerMeLink, FString ReceivedReadyPlayerMeLink);

private:
	// Used when both the voice component ring buffer is empty and we have already sent all the current audio data over the stream
	void onMicrophoneDataReceived();

	// Returns false if the player has decided to stop sending audio
	bool CheckTokenValidity();

	// Called when a player time outs while sending audio data (for example 2 secs have passed without us getting any audio from him)
	void OnPlayerTimeOut();

	void ClearTimeOutTimer();

private:
	void Start_GRPC_Request();

	void Bind_GRPC_Request_Delegates();

	void Unbind_GRPC_Request_Delegates();

	void Cleanup(bool StreamConnectionFinished = false);

private:
	void OnTranscriptionReceived(FString Transcription, bool IsTranscriptionReady, bool IsFinal);
	void onResponseDataReceived(FString ReceivedText, const TArray<uint8>& ReceivedAudio, uint32 SampleRate, bool IsFinal);
	void onSessionIDReceived(FString ReceivedSessionID);
	void onActionSequenceReceived(const TArray<FConvaiResultAction>& ReceivedSequenceOfActions);
	void onFinishedReceivingData();
	void onFailure();

private:
	UPROPERTY()
	UConvaiChatBotGetDetailsProxy* ConvaiChatBotGetDetailsProxy;

	UPROPERTY()
	UConvaiGRPCGetResponseProxy* ConvaiGRPCGetResponseProxy;

	UPROPERTY()
	UConvaiPlayerComponent* CurrentConvaiPlayerComponent;

	bool GenerateActions; // Should we generate actions
	bool TextInput; // Whether  to use text or audio as input to the API
	bool VoiceResponse; // Require audio response from the API
	FString UserText; // Input text to send to the API in case of TextInput is set to true
	uint32 Token; // Used to check if the Convai player component is still streaming to us 

	bool StreamInProgress = false; // Are we receiving mic audio from player?
	FTimerHandle TimeOutTimerHandle; // Timeout handler for player not sending audio data through mic

	FString LastTranscription;
	bool ReceivedFinalTranscription;
	bool ReceivedFinalData;
};