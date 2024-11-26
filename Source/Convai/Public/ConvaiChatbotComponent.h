// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once
//#include "CoreMinimal.h"
#include "Components/AudioComponent.h"
#include "ConvaiAudioStreamer.h"
#include "Containers/Map.h"
#include "ConvaiDefinitions.h"

#include "ConvaiChatbotComponent.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(ConvaiChatbotComponentLog, Log, All);

DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_ThreeParams(FOnTranscriptionReceivedSignature_Deprecated, UConvaiChatbotComponent, OnTranscriptionReceivedEvent, FString, Transcription, bool, IsTranscriptionReady, bool, IsFinal);
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_SixParams(FOnTranscriptionReceivedSignature_V2, UConvaiChatbotComponent, OnTranscriptionReceivedEvent_V2, UConvaiChatbotComponent*, ChatbotComponent, UConvaiPlayerComponent*, InteractingPlayerComponent, FString, PlayerName, FString, Transcription, bool, IsTranscriptionReady, bool, IsFinal);

DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_FourParams(FOnTextReceivedSignature_Deprecated, UConvaiChatbotComponent, OnTextReceivedEvent, FString, CharacterName, FString, BotText, float, AudioDuration, bool, IsFinal);
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_SixParams(FOnTextReceivedSignature_V2, UConvaiChatbotComponent, OnTextReceivedEvent_V2, UConvaiChatbotComponent*, ChatbotComponent, UConvaiPlayerComponent*, InteractingPlayerComponent, FString, CharacterName, FString, BotText, float, AudioDuration, bool, IsFinal);

DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_OneParam(FOnActionReceivedSignature_Deprecated, UConvaiChatbotComponent, OnActionReceivedEvent, const TArray<FConvaiResultAction>&, SequenceOfActions);
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_ThreeParams(FOnActionReceivedSignature_V2, UConvaiChatbotComponent, OnActionReceivedEvent_V2, UConvaiChatbotComponent*, ChatbotComponent, UConvaiPlayerComponent*, InteractingPlayerComponent, const TArray<FConvaiResultAction>&, SequenceOfActions);

DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_TwoParams(FOnEmotionReceivedSignature, UConvaiChatbotComponent, OnEmotionStateChangedEvent, UConvaiChatbotComponent*, ChatbotComponent, UConvaiPlayerComponent*, InteractingPlayerComponent);

DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_OneParam(FOnCharacterDataLoadSignature_Deprecated, UConvaiChatbotComponent, OnCharacterDataLoadEvent, bool, Success);
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_TwoParams(FOnCharacterDataLoadSignature_V2, UConvaiChatbotComponent, OnCharacterDataLoadEvent_V2, UConvaiChatbotComponent*, ChatbotComponent, bool, Success);

DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_TwoParams(FOnNarrativeSectionReceivedSignature, UConvaiChatbotComponent, OnNarrativeSectionReceivedEvent, UConvaiChatbotComponent*, ChatbotComponent, FString, NarrativeSectionID);

DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_ThreeParams(FOnInteractionIDReceivedSignature, UConvaiChatbotComponent, OnInteractionIDReceivedEvent, UConvaiChatbotComponent*, ChatbotComponent, UConvaiPlayerComponent*, InteractingPlayerComponent, FString, InteractionID);

DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE(FOnFailureSignature, UConvaiChatbotComponent, OnFailureEvent);
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_TwoParams(FOnInterruptedSignature, UConvaiChatbotComponent, OnInterruptedEvent, UConvaiChatbotComponent*, ChatbotComponent, UConvaiPlayerComponent*, InteractingPlayerComponent);

// TODO (Mohamed): Manage onDestroy/onEndPlay - should end any on-going streams

class UConvaiPlayerComponent;
class USoundWaveProcedural;
class UConvaiGRPCGetResponseProxy;
class UConvaiChatBotGetDetailsProxy;

UCLASS(Blueprintable, BlueprintType, meta = (BlueprintSpawnableComponent), DisplayName = "Convai Chatbot")
class CONVAI_API UConvaiChatbotComponent : public UConvaiAudioStreamer
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
	UFUNCTION(BlueprintPure, BlueprintCallable, Category = "Convai", meta = (DisplayName = "Is Thinking"))
	bool IsProcessing();

	/**
	* Returns true, if the character is currently listening to a player.
	*/
	UFUNCTION(BlueprintPure, BlueprintCallable, Category = "Convai")
	bool IsListening();

	/**
	* Returns true, if the character is currently talking.
	*/
	UFUNCTION(BlueprintPure, BlueprintCallable, Category = "Convai", meta = (DisplayName = "Is Talking"))
	bool GetIsTalking();

	/** Returns time elapsed since the character started talking */
	UFUNCTION(BlueprintPure, BlueprintCallable, Category = "Convai|Voice")
	float GetTalkingTimeElapsed();

	/** Returns time remaining audio time for the character to speak */
	UFUNCTION(BlueprintPure, BlueprintCallable, Category = "Convai|Voice")
	float GetTalkingTimeRemaining();

	UPROPERTY(EditAnywhere, Category = "Convai", Replicated, BlueprintSetter = LoadCharacter)
	FString CharacterID;

	UPROPERTY(BlueprintReadOnly, Category = "Convai", Replicated)
	FString CharacterName;

	UPROPERTY(BlueprintReadOnly, Category = "Convai", Replicated)
	FString VoiceType;

	UPROPERTY(BlueprintReadOnly, Category = "Convai", Replicated)
	FString Backstory;

	UPROPERTY(BlueprintReadOnly, Category = "Convai", Replicated)
	FString LanguageCode;

	UPROPERTY(BlueprintReadOnly, Category = "Convai", Replicated)
	FString ReadyPlayerMeLink;

	UPROPERTY(BlueprintReadOnly, Category = "Convai", Replicated)
	FString AvatarImageLink;

	UPROPERTY(BlueprintReadOnly, Category = "Convai", Replicated, meta = (DisplayName = "Interacting Player"))
	UConvaiPlayerComponent* CurrentConvaiPlayerComponent;

	UPROPERTY(BlueprintReadOnly, Category = "Convai|Actions", Replicated)
	TArray<FConvaiResultAction> ActionsQueue;

	UPROPERTY(Replicated)
	FConvaiEmotionState EmotionState;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Convai|Emotion", Replicated)
	bool LockEmotionState = false;

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
	UPROPERTY(BlueprintReadWrite, Category = "Convai", BlueprintSetter = LoadEnvironment)
	UConvaiEnvironment* Environment;

	/**
	 *    Time in seconds, for the character's voice audio to gradually degrade until it is completely turned off when interrupted.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Convai")
	float InterruptVoiceFadeOutDuration;

	/**
	 *    Value between -1 and 1, a value greater than zero over extends the emotions strength and a value less than zero dimishes it.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Convai")
	float EmotionOffset = 0;

	/**
	 *    Contains key value pairs used for Narrative Design.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Convai|NarrativeDesign")
	TMap<FString, FString> NarrativeTemplateKeys;

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

	UFUNCTION(BlueprintCallable, BlueprintInternalUseOnly, Category = "Convai")
	void LoadEnvironment(UConvaiEnvironment* NewConvaiEnvironment);

	/**
	 * Appends a TArray of FConvaiResultAction items to the existing ActionsQueue.
	 * If ActionsQueue is not empty, it takes the first element, appends the new array to it,
	 * and then reassigns it back to ActionsQueue. If ActionsQueue is empty, it simply sets ActionsQueue to the new array.
	 *
	 * @param NewActions Array of FConvaiResultAction items to be appended.
	 *
	 * @category Convai
	 */
	 void AppendActionsToQueue(TArray<FConvaiResultAction> NewActions);

	/**
	 * Marks the current action as completed and handles post-execution logic.
	 *
	 * @param IsSuccessful A boolean flag indicating whether the executed action was successful or not. If true, the next action in the queue will be processed. If false, the current action will be retried.
	 * @param Delay A float value representing the time in seconds to wait before attempting either the next action or retrying the current action, depending on the value of IsSuccessful.
	 *
	 * @note This function should be invoked after each action execution to manage the action queue.
	 *
	 */
	UFUNCTION(BlueprintCallable, Category = "Convai|Actions")
	void HandleActionCompletion(bool IsSuccessful, float Delay);

	/**
	 * Checks if the ActionsQueue managed by the Convai chatbot component is empty.
	 *
	 * @return Returns true if the ActionsQueue is empty; otherwise, returns false.
	 *
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Convai|Actions")
	bool IsActionsQueueEmpty();

	/**
	 * Clears the ActionsQueue managed by the Convai chatbot component.
	 *
	 */
	UFUNCTION(BlueprintCallable, Category = "Convai|Actions")
	void ClearActionQueue();

	/**
	 * Fetches the first action from the ActionsQueue managed by the Convai chatbot component.
	 *
	 * @param ConvaiResultAction Reference to a struct that will be populated with the details of the first action in the queue.
	 *
	 * @return Returns true if there is at least one action in the ActionsQueue and the struct has been successfully populated; otherwise, returns false.
	 *
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Convai|Actions")
	bool FetchFirstAction(FConvaiResultAction& ConvaiResultAction);

	/**
	 * Removes the first action from the ActionsQueue managed by the Convai chatbot component.
	 *
	 * @return Returns true if an action was successfully removed; otherwise, returns false.
	 *
	 */
	bool DequeueAction();

	/**
	 * Starts executing the first action in the ActionsQueue by calling TriggerNamedBlueprintAction.
	 *
	 * @return Returns true if the first action was successfully started; otherwise, returns false.
	 *
	 */
	UFUNCTION()
	bool StartFirstAction();

	/**
	 * Triggers a specified Blueprint event or function on the owning actor based on the given action name and parameters.
	 *
	 * @param ActionName The name of the Blueprint event or function to trigger. This event or function should exist in the Blueprint that owns this component.
	 * @param ConvaiActionStruct A struct containing additional data or parameters to pass to the Blueprint event or function.
	 *
	 * @note The function attempts to dynamically find and call a Blueprint event or function in the owning actor's class. If the Blueprint event or function does not exist or if the signature doesn't match, the function will log a warning.
	 *
	 */
	bool TriggerNamedBlueprintAction(const FString& ActionName, FConvaiResultAction ConvaiActionStruct);

	UFUNCTION(BlueprintCallable, Category = "Convai|Emotion")
	void ForceSetEmotion(EBasicEmotions BasicEmotion, EEmotionIntensity Intensity, bool ResetOtherEmotions = false);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Convai|Emotion")
	float GetEmotionScore(EBasicEmotions Emotion);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Convai|Emotion")
	TMap<FName, float> GetEmotionBlendshapes();

	UFUNCTION(BlueprintCallable, Category = "Convai|Emotion")
	void ResetEmotionState();

public:
	// UFUNCTION(BlueprintCallable, Category = "Convai|Voice")
	void StartRecordingVoice();

	// UFUNCTION(BlueprintCallable, Category = "Convai|Voice")
	USoundWave* FinishRecordingVoice();

	// UFUNCTION(BlueprintCallable, Category = "Convai|Voice")
	bool PlayRecordedVoice(USoundWave* RecordedVoice);

public:
	/** Called when a new action is received from the API */
	UPROPERTY(BlueprintAssignable, Category = "Convai", meta = (DisplayName = "_DEPRECATED On Actions Received"))
	FOnActionReceivedSignature_Deprecated OnActionReceivedEvent;
	UPROPERTY(BlueprintAssignable, Category = "Convai", meta = (DisplayName = "On Actions Received"))
	FOnActionReceivedSignature_V2 OnActionReceivedEvent_V2;
	
	UPROPERTY(BlueprintAssignable, Category = "Convai", meta = (DisplayName = "On Emotion State Changed"))
	FOnEmotionReceivedSignature OnEmotionStateChangedEvent;

	/** Called when new text is received from the API, AudioDuration = 0 if no audio was received */
	UPROPERTY(BlueprintAssignable, Category = "Convai", meta = (DisplayName = "_DEPRECATED On Text Received"))
	FOnTextReceivedSignature_Deprecated OnTextReceivedEvent;
	UPROPERTY(BlueprintAssignable, Category = "Convai", meta = (DisplayName = "On Text Received"))
	FOnTextReceivedSignature_V2 OnTextReceivedEvent_V2;

	/** Called when new transcription is available */
	UPROPERTY(BlueprintAssignable, Category = "Convai", meta = (DisplayName = "_DEPRECATED On Transcription Received"))
	FOnTranscriptionReceivedSignature_Deprecated OnTranscriptionReceivedEvent;
	UPROPERTY(BlueprintAssignable, Category = "Convai", meta = (DisplayName = "On Transcription Received"))
	FOnTranscriptionReceivedSignature_V2 OnTranscriptionReceivedEvent_V2;

	UPROPERTY(BlueprintAssignable, Category = "Convai", meta = (DisplayName = "_DEPRECATED On Character Data Loaded"))
	FOnCharacterDataLoadSignature_Deprecated OnCharacterDataLoadEvent;
	UPROPERTY(BlueprintAssignable, Category = "Convai", meta = (DisplayName = "On Character Data Loaded"))
	FOnCharacterDataLoadSignature_V2 OnCharacterDataLoadEvent_V2;

	UPROPERTY(BlueprintAssignable, Category = "Convai", meta = (DisplayName = "On Narrative Section Received"))
	FOnNarrativeSectionReceivedSignature OnNarrativeSectionReceivedEvent;

	UPROPERTY(BlueprintAssignable, Category = "Convai", meta = (DisplayName = "On Interaction ID Received"))
	FOnInteractionIDReceivedSignature OnInteractionIDReceivedEvent;

	/** Called when the character is interrupted */
	 UPROPERTY(BlueprintAssignable, Category = "Convai", meta = (DisplayName = "On Interrupted"))
	 FOnInterruptedSignature OnInterruptedEvent;

	/** Called when there is an error */
	UPROPERTY(BlueprintAssignable, Category = "Convai", meta = (DisplayName = "On Failure"))
	FOnFailureSignature OnFailureEvent;	

public:
	//UFUNCTION(BlueprintCallable, DisplayName = "Begin Transmission")
	void StartGetResponseStream(UConvaiPlayerComponent* InConvaiPlayerComponent, FString InputText, UConvaiEnvironment* InEnvironment, bool InGenerateActions, bool InVoiceResponse, bool ReplicateVoiceToNetwork, bool UseOverrideAuthKey, FString OverrideAuthKey, FString OverrideAuthHeader, uint32 InToken);
	
	void FinishGetResponseStream(UConvaiPlayerComponent* InConvaiPlayerComponent);

	UFUNCTION(BlueprintCallable, Category = "Convai", meta = (DisplayName = "Invoke Speech"))
	void ExecuteNarrativeTrigger(FString TriggerMessage, UConvaiEnvironment* InEnvironment, bool InGenerateActions, bool InVoiceResponse, bool InReplicateOnNetwork);
	
	UFUNCTION(BlueprintCallable, Category = "Convai", meta = (DisplayName = "Invoke Narrative Design Trigger"))
	void InvokeNarrativeDesignTrigger(FString TriggerName, UConvaiEnvironment* InEnvironment, bool InGenerateActions, bool InVoiceResponse, bool InReplicateOnNetwork);

	void InvokeTrigger_Internal(FString TriggerName, FString TriggerMessage, UConvaiEnvironment* InEnvironment, bool InGenerateActions, bool InVoiceResponse, bool InReplicateOnNetwork);

	// Interrupts the current speech with a provided fade-out duration. 
	// The fade-out duration is controlled by the parameter 'InVoiceFadeOutDuration'.
	UFUNCTION(BlueprintCallable, Category = "Convai")
	void InterruptSpeech(float InVoiceFadeOutDuration);

	// Trys to clear the Interacting player variable if there were no conversation going
	// If Force clear was true, it will interrupt the conversation and clear the interacting player
	UFUNCTION(BlueprintCallable, Category = "Convai")
	void TryClearInteractingPlayer(bool& Success, bool Interrupt);

	// Broadcasts an interruption of the current speech across a network, with a provided fade-out duration.
	// This function ensures that the interruption is communicated reliably to all connected clients.
	// The fade-out duration is controlled by the parameter 'InVoiceFadeOutDuration'.
	UFUNCTION(NetMulticast, Reliable, Category = "VoiceNetworking")
	void Broadcast_InterruptSpeech(float InVoiceFadeOutDuration);

public:
	// AActorComponent interface
	virtual void BeginPlay() override;
	//virtual void OnRegister() override;
	//virtual void OnUnregister() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	// End AActorComponent interface

	//~ Begin UObject Interface.
	virtual void BeginDestroy() override;
	//~ End UObject Interface.

	//~ Begin UConvaiAudioStreamer Interface.
	virtual bool CanUseLipSync() override;
	virtual bool CanUseVision() override;
	//~ End UConvaiAudioStreamer Interface.

private:
	UConvaiChatBotGetDetailsProxy* ConvaiGetDetails();

	TScriptDelegate<FWeakObjectPtr> ConvaiChatBotGetDetailsDelegate;

	UFUNCTION()
	void OnConvaiGetDetailsCompleted(FString ReceivedCharacterName, FString ReceivedVoiceType, FString ReceivedBackstory, FString ReceivedLanguageCode, bool HasReadyPlayerMeLink, FString ReceivedReadyPlayerMeLink, FString ReceivedAvatarImageLink);

private:
	// Used when both the voice component ring buffer is empty and we have already sent all the current audio data over the stream
	void onMicrophoneDataReceived();

	// Returns false if the player has decided to stop sending audio
	bool CheckTokenValidity();

	// Called when a player time outs while sending audio data (for example 2 secs have passed without us getting any audio from him)
	void OnPlayerTimeOut();

	void ClearTimeOutTimer();

	bool HasOnGoingGetResponseStream();

	bool CanWriteToGetResponseStream();

	bool ConsumeMicStreamIntoBuffer();

private:
	void Start_GRPC_Request(bool UseOverrideAuthKey, FString OverrideAuthKey, FString OverrideAuthHeader, FString TriggerName = "", FString TriggerMessage = "");

	void Bind_GRPC_Request_Delegates();

	void Unbind_GRPC_Request_Delegates();

	void Cleanup(bool StreamConnectionFinished = false);

private:
	UFUNCTION(NetMulticast, Reliable, Category = "Convai")
	void Broadcast_OnTranscriptionReceived(const FString& Transcription, bool IsTranscriptionReady, bool IsFinal);
	UFUNCTION(NetMulticast, Reliable, Category = "Convai")
	void Broadcast_onResponseDataReceived(const FString& ReceivedText, bool IsFinal);
	UFUNCTION(NetMulticast, Reliable, Category = "Convai")
	void Broadcast_onSessionIDReceived(const FString& ReceivedSessionID);
	UFUNCTION(NetMulticast, Reliable, Category = "Convai")
	void Broadcast_onActionSequenceReceived(const TArray<FConvaiResultAction>& ReceivedSequenceOfActions);	
	UFUNCTION(NetMulticast, Reliable, Category = "Convai")
	void Broadcast_OnNarrativeSectionReceived(const FString& BT_Code, const FString& BT_Constants, const FString& ReceivedNarrativeSectionID);
	UFUNCTION(NetMulticast, Reliable, Category = "Convai")
	void Broadcast_onInteractionIDReceived(const FString& ReceivedInteractionID);
	UFUNCTION(NetMulticast, Reliable, Category = "Convai")
	void Broadcast_onEmotionReceived(const FString& ReceivedEmotionResponse, bool MultipleEmotions);

	void OnTranscriptionReceived(FString Transcription, bool IsTranscriptionReady, bool IsFinal);
	void onResponseDataReceived(const FString ReceivedText, const TArray<uint8>& ReceivedAudio, uint32 SampleRate, bool IsFinal);
	void OnFaceDataReceived(FAnimationSequence FaceDataAnimation);
	void onSessionIDReceived(FString ReceivedSessionID);
	void onInteractionIDReceived(FString ReceivedInteractionID);
	void onActionSequenceReceived(const TArray<FConvaiResultAction>& ReceivedSequenceOfActions);
	void onEmotionReceived(FString ReceivedEmotionResponse, FAnimationFrame EmotionBlendshapesFrame, bool MultipleEmotions);
	void onFinishedReceivingData();
	void OnNarrativeSectionReceived(FString BT_Code, FString BT_Constants, FString ReceivedNarrativeSectionID);
	void onFailure();

private:
	UPROPERTY(Replicated, ReplicatedUsing = OnRep_EnvironmentData)
	FConvaiEnvironmentDetails ConvaiEnvironmentDetails;

	UFUNCTION()
	void OnRep_EnvironmentData();

	void UpdateEnvironmentData();

private:
	UPROPERTY()
	UConvaiChatBotGetDetailsProxy* ConvaiChatBotGetDetailsProxy;

	UPROPERTY()
	UConvaiGRPCGetResponseProxy* ConvaiGRPCGetResponseProxy;

	bool GenerateActions; // Should we generate actions
	bool TextInput; // Whether  to use text or audio as input to the API
	bool VoiceResponse; // Require audio response from the API
	FString UserText; // Input text to send to the API in case of TextInput is set to true
	uint32 Token; // Used to check if the Convai player component is still streaming to us 

	bool StreamInProgress = false; // Are we receiving mic audio from player?
	FTimerHandle TimeOutTimerHandle; // Timeout handler for player not sending audio data through mic

	FString LastTranscription;
	bool ReceivedFinalTranscription;
	bool ReceivedFinalData; // Did the character end his response
	FString LastPlayerName;
	TArray<uint8> PlayerInpuAudioBuffer;

	TMap<FName, float> EmotionBlendshapes;
	float TotalReceivedAudioDuration = 0;

	TArray<uint8> RecordedAudio;
	uint32 RecordedAudioSampleRate;
	bool IsRecordingAudio;
};