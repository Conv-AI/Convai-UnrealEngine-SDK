// Copyright 2022 Convai Inc. All Rights Reserved.

#include "ConvaiChatbotComponent.h"
#include "ConvaiPlayerComponent.h"
#include "../Convai.h"
#include "ConvaiGRPC.h"
#include "ConvaiActionUtils.h"
#include "ConvaiUtils.h"
#include "LipSyncInterface.h"

#include "Sound/SoundWaveProcedural.h"
#include "Net/UnrealNetwork.h"
#include "ConvaiChatBotProxy.h"
#include "Kismet/KismetSystemLibrary.h"

DEFINE_LOG_CATEGORY(ConvaiChatbotComponentLog);

UConvaiChatbotComponent::UConvaiChatbotComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	//SetIsReplicated(true);
	InterruptVoiceFadeOutDuration = 1.0;
	LastPlayerName = FString("Unknown");
	//Environment = CreateDefaultSubobject<UConvaiEnvironment>(TEXT("Environment"));
}

void UConvaiChatbotComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UConvaiChatbotComponent, CharacterID);
	DOREPLIFETIME(UConvaiChatbotComponent, SessionID);
	DOREPLIFETIME(UConvaiChatbotComponent, CharacterName);
	DOREPLIFETIME(UConvaiChatbotComponent, VoiceType);
	DOREPLIFETIME(UConvaiChatbotComponent, Backstory);
	DOREPLIFETIME(UConvaiChatbotComponent, ReadyPlayerMeLink);
	DOREPLIFETIME(UConvaiChatbotComponent, CurrentConvaiPlayerComponent);
	DOREPLIFETIME(UConvaiChatbotComponent, ActionsQueue);
	DOREPLIFETIME(UConvaiChatbotComponent, EmotionState);
	DOREPLIFETIME(UConvaiChatbotComponent, LockEmotionState);
	DOREPLIFETIME(UConvaiChatbotComponent, ConvaiEnvironmentDetails);
}

bool UConvaiChatbotComponent::IsInConversation()
{
	return (IsValid(ConvaiGRPCGetResponseProxy) || IsTalking);
}

bool UConvaiChatbotComponent::IsProcessing()
{
	return (IsValid(ConvaiGRPCGetResponseProxy));
}

bool UConvaiChatbotComponent::IsListening()
{
	return (StreamInProgress);
}

bool UConvaiChatbotComponent::GetIsTalking()
{
	return IsTalking;
}

float UConvaiChatbotComponent::GetTalkingTimeElapsed()
{
	float TimeElapsed = 0;
	if (IsValid(GetWorld()))
	{
		TimeElapsed = GetWorld()->GetTimerManager().GetTimerElapsed(AudioFinishedTimerHandle);
	}

	return TimeElapsed;
}

float UConvaiChatbotComponent::GetTalkingTimeRemaining()
{
	float TimeRemaing= 0;
	if (IsValid(GetWorld()))
	{
		TimeRemaing = GetWorld()->GetTimerManager().GetTimerRemaining(AudioFinishedTimerHandle);
	}

	return TimeRemaing;
}

void UConvaiChatbotComponent::ResetConversation()
{
	SessionID = "-1";
}

void UConvaiChatbotComponent::LoadCharacter(FString NewCharacterID)
{
	CharacterID = NewCharacterID;
	ConvaiGetDetails();
}

void UConvaiChatbotComponent::AppendActionsToQueue(TArray<FConvaiResultAction> NewActions)
{
	if (ActionsQueue.Num() > 0)
	{
		FConvaiResultAction FirstAction = ActionsQueue[0];
		NewActions.Insert(FirstAction, 0);
		ActionsQueue = NewActions;
	}
	else
	{
		ActionsQueue = NewActions;
	}
}

void UConvaiChatbotComponent::HandleActionCompletion(bool IsSuccessful, float Delay)
{
	if (!UConvaiUtils::IsNewActionSystemEnabled())
	{

		UE_LOG(ConvaiChatbotComponentLog, Warning, TEXT("HandleActionCompletion: New Action System is not enabled in settings"));
		return;
	}


	if (IsSuccessful)
		DequeueAction();

	if (IsActionsQueueEmpty())
		return;

	// Create a timer to call StartFirstAction after a delay
	if (Delay > 0.0f)
	{
		FTimerHandle TimerHandle;
		FTimerDelegate TimerDelegate;

		// Bind the function with parameters
		TimerDelegate.BindUFunction(this, FName("StartFirstAction"));

		// Set the timer
		GetWorld()->GetTimerManager().SetTimer(TimerHandle, TimerDelegate, Delay, false);
	}
	else
	{
		// Call the function immediately
		StartFirstAction();
	}
}

bool UConvaiChatbotComponent::IsActionsQueueEmpty()
{
	if (!UConvaiUtils::IsNewActionSystemEnabled())
	{

		UE_LOG(ConvaiChatbotComponentLog, Warning, TEXT("IsActionsQueueEmpty: New Action System is not enabled in settings"));
		return true;
	}

	return ActionsQueue.Num() == 0;
}

bool UConvaiChatbotComponent::FetchFirstAction(FConvaiResultAction& ConvaiResultAction)
{
	if (!UConvaiUtils::IsNewActionSystemEnabled())
	{
		UE_LOG(ConvaiChatbotComponentLog, Warning, TEXT("FetchFirstAction: New Action System is not enabled in settings"));
		return false;
	}

	if (ActionsQueue.Num() == 0)
		return false;

	ConvaiResultAction = ActionsQueue[0];
	return true;
}

bool UConvaiChatbotComponent::DequeueAction()
{
	if (ActionsQueue.Num() > 0)
	{
		ActionsQueue.RemoveAt(0);
		return true;
	}
	return false;
}

bool UConvaiChatbotComponent::StartFirstAction()
{
	FConvaiResultAction ConvaiResultAction;
	if (FetchFirstAction(ConvaiResultAction))
	{
		if (ConvaiResultAction.Action.Compare(FString("None"), ESearchCase::IgnoreCase) == 0)
		{
			HandleActionCompletion(true, 0);
			return true;
		}

		AsyncTask(ENamedThreads::GameThread, [this, ConvaiResultAction]
		{
			TriggerNamedBlueprintAction(ConvaiResultAction.Action, ConvaiResultAction);
		});
		return true;
	}
	return false;
}

bool UConvaiChatbotComponent::TriggerNamedBlueprintAction(const FString& ActionName, FConvaiResultAction ConvaiActionStruct)
{
	if (AActor* Owner = GetOwner())
	{
		UFunction* Function = Owner->FindFunction(FName(*ActionName));

		if (Function)
		{
			// Check the function signature
			bool bCanCall = false;
			TFieldIterator<FStructProperty> PropIt(Function);
			if (PropIt)
			{
				FStructProperty* StructProp = *PropIt;
				if (StructProp->GetClass() == FStructProperty::StaticClass())
				{
					//FStructProperty* StructProp = Cast<FStructProperty>(ParamProp);
					if (StructProp && StructProp->Struct == FConvaiResultAction::StaticStruct())
					{
						bCanCall = true;
					}
				}
			}
			else
			{
				bCanCall = true; // No parameters
			}

			if (bCanCall)
			{
				Owner->ProcessEvent(Function, PropIt ? &ConvaiActionStruct : nullptr);
				return true;
			}
			else
			{
				UE_LOG(ConvaiChatbotComponentLog, Warning, TEXT("TriggerNamedBlueprintAction: Found a function/event with the same name of action: %s.\
					 However, could not run due to mismatched parameter type. Make sure the function/event has no input parameters or take a parameter of type \"ConvaiResultAction\""), *ActionName);
			}
		}
		else
		{
			//UE_LOG(ConvaiChatbotComponentLog, Log, TEXT("TriggerNamedBlueprintAction: Could not find an event or function with action name: %s"), *ActionName);
		}
	}
	else
	{
		UE_LOG(ConvaiChatbotComponentLog, Warning, TEXT("TriggerNamedBlueprintAction: Could not find pointer to owner"));
	}
	return false;
}

void UConvaiChatbotComponent::ForceSetEmotion(EBasicEmotions BasicEmotion, EEmotionIntensity Intensity, bool ResetOtherEmotions)
{
	EmotionState.ForceSetEmotion(BasicEmotion, Intensity, ResetOtherEmotions);
	OnEmotionStateChangedEvent.Broadcast(this, CurrentConvaiPlayerComponent);
}

float UConvaiChatbotComponent::GetEmotionScore(EBasicEmotions Emotion)
{
	return EmotionState.GetEmotionScore(Emotion);
}

void UConvaiChatbotComponent::ResetEmotionState()
{
	EmotionState.ResetEmotionScores();
	OnEmotionStateChangedEvent.Broadcast(this, CurrentConvaiPlayerComponent);
}

void UConvaiChatbotComponent::StartGetResponseStream(UConvaiPlayerComponent* InConvaiPlayerComponent, FString InputText, UConvaiEnvironment* InEnvironment, bool InGenerateActions, bool InVoiceResponse, bool RunOnServer, bool UseOverrideAPI_Key, FString OverrideAPI_Key, uint32 InToken)
{
	if (!IsValid(InConvaiPlayerComponent))
	{
		UE_LOG(ConvaiChatbotComponentLog, Warning, TEXT("StartGetResponseStream: ConvaiPlayerComponent is not valid"));
		return;
	}

	if (IsValid(CurrentConvaiPlayerComponent) && CurrentConvaiPlayerComponent != InConvaiPlayerComponent && CheckTokenValidity())
	{
		UE_LOG(ConvaiChatbotComponentLog, Warning, TEXT("StartGetResponseStream: character is currently being talked to by another player, make sure to run \"Finish Talking\""));
		return;
	}

	if (IsValid(Environment))
	{
		Environment->SetFromEnvironment(InEnvironment);
	}
	else
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("StartGetResponseStream: Environment is not valid"));
	}

	FString Error;
	bool ValidEnvironment = UConvaiActions::ValidateEnvironment(Environment, Error);
	if (GenerateActions && !ValidEnvironment)
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("StartGetResponseStream: %s"), *Error);
		UE_LOG(ConvaiPlayerLog, Log, TEXT("StartGetResponseStream: Environment object seems to have issues -> setting GenerateActions to false"));
		GenerateActions = false;
	}

	// Set the main character (player) to be the one talking
	//if (ValidEnvironment && Environment->MainCharacter.Name != InConvaiPlayerComponent->PlayerName)
	//{
	//	FConvaiObjectEntry MainCharacter;
	//	MainCharacter.Name = InConvaiPlayerComponent->PlayerName;
	//	MainCharacter.Ref = InConvaiPlayerComponent;
	//	Environment->MainCharacter = MainCharacter;
	//}

	if (GetIsTalking() || IsProcessing())
	{
		InterruptSpeech(InterruptVoiceFadeOutDuration);
	}

	UserText = InputText;
	TextInput = UserText.Len() > 0;
	GenerateActions = InGenerateActions;
	VoiceResponse = InVoiceResponse;
	ReplicateVoiceToNetwork = RunOnServer;
	Token = InToken;

	if (!TextInput)
	{
		StreamInProgress = true;
	}
	else
	{
		OnTranscriptionReceived(UserText, true, true);
	}

	CurrentConvaiPlayerComponent = InConvaiPlayerComponent;
	LastPlayerName = CurrentConvaiPlayerComponent->PlayerName;

	Start_GRPC_Request(UseOverrideAPI_Key, OverrideAPI_Key);
}

void UConvaiChatbotComponent::ExecuteNarrativeTrigger(FString TriggerName, FString TriggerMessage, UConvaiEnvironment* InEnvironment, bool InGenerateActions, bool InVoiceResponse, bool InReplicateVoiceToNetwork)
{
	if (TriggerName.IsEmpty() && TriggerMessage.IsEmpty())
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("SendNarrativeTrigger: Trigger Name and TriggerMessage are missing - Please supply one of them"));
	}

	if (IsValid(Environment))
	{
		Environment->SetFromEnvironment(InEnvironment);
	}
	else
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("SendNarrativeTrigger: Environment is not valid"));
	}

	FString Error;
	bool ValidEnvironment = UConvaiActions::ValidateEnvironment(Environment, Error);
	if (GenerateActions && !ValidEnvironment)
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("SendNarrativeTrigger: %s"), *Error);
		UE_LOG(ConvaiPlayerLog, Log, TEXT("SendNarrativeTrigger: Environment object seems to have issues -> setting GenerateActions to false"));
		GenerateActions = false;
	}

	// Set the main character (player) to be the one talking
	//if (ValidEnvironment && Environment->MainCharacter.Name != InConvaiPlayerComponent->PlayerName)
	//{
	//	FConvaiObjectEntry MainCharacter;
	//	MainCharacter.Name = InConvaiPlayerComponent->PlayerName;
	//	MainCharacter.Ref = InConvaiPlayerComponent;
	//	Environment->MainCharacter = MainCharacter;
	//}

	if (GetIsTalking() || IsProcessing())
	{
		InterruptSpeech(InterruptVoiceFadeOutDuration);
	}

	UserText = "";
	GenerateActions = InGenerateActions;
	VoiceResponse = InVoiceResponse;
	ReplicateVoiceToNetwork = InReplicateVoiceToNetwork;

	Start_GRPC_Request(false, "", TriggerName, TriggerMessage);
}

void UConvaiChatbotComponent::InterruptSpeech(float InVoiceFadeOutDuration)
{
	// Broadcast to clients
	if (UKismetSystemLibrary::IsServer(this) && ReplicateVoiceToNetwork)
	{
		Broadcast_InterruptSpeech(InVoiceFadeOutDuration);
	}

	if (GetIsTalking() || IsProcessing())
	{
		UE_LOG(ConvaiChatbotComponentLog, Log, TEXT("InterruptSpeech: Interrupting character"));
		onFinishedReceivingData();
		StopVoiceWithFade(InVoiceFadeOutDuration);

		if (ReceivedFinalTranscription == false)
			OnTranscriptionReceived(LastTranscription, true, true);

		if (ReceivedFinalData == false)
			onResponseDataReceived(FString(""), TArray<uint8>(), 0, true);

		//AsyncTask(ENamedThreads::GameThread, [WeakThis = MakeWeakObjectPtr(this)]
		//	{
		//		if (!WeakThis.IsValid() || WeakThis->IsPendingKill())
		//		{
		//			// The object is no longer valid or is being destroyed.
		//			return;
		//		}
		//		WeakThis->OnInterruptedEvent.Broadcast(WeakThis.Get(), WeakThis->CurrentConvaiPlayerComponent);
		//	});
	}
}

void UConvaiChatbotComponent::TryClearInteractingPlayer(bool& Success, bool Interrupt)
{
	if (IsListening())
	{
		Success = false;
		return;
	}
	if (Interrupt)
		InterruptSpeech(0.5);
	CurrentConvaiPlayerComponent = nullptr;
}

void UConvaiChatbotComponent::Broadcast_InterruptSpeech_Implementation(float InVoiceFadeOutDuration)
{
	// Execute if you are a client
	if (!UKismetSystemLibrary::IsServer(this))
	{
		InterruptSpeech(InVoiceFadeOutDuration);
	}
}

void UConvaiChatbotComponent::Start_GRPC_Request(bool UseOverrideAPI_Key, FString OverrideAPI_Key, FString TriggerName, FString TriggerMessage)
{
	FString API_Key = UseOverrideAPI_Key ? OverrideAPI_Key : UConvaiUtils::GetAPI_Key();
	bool RequireFaceData = false;
	bool GeneratesVisemesAsBlendshapes = false;
	if (ConvaiLipSyncExtended)
	{
		RequireFaceData = ConvaiLipSyncExtended->RequiresPreGeneratedFaceData();
		GeneratesVisemesAsBlendshapes = ConvaiLipSyncExtended->GeneratesVisemesAsBlendshapes();
	}
	RequireFaceData = RequireFaceData && VoiceResponse;

	// Create the request proxy
	ConvaiGRPCGetResponseProxy = UConvaiGRPCGetResponseProxy::CreateConvaiGRPCGetResponseProxy(this, UserText, TriggerName, TriggerMessage, CharacterID, VoiceResponse, RequireFaceData, GeneratesVisemesAsBlendshapes, SessionID, Environment, GenerateActions, API_Key);

	// Bind the needed delegates
	Bind_GRPC_Request_Delegates();

	// Start the stream
	ConvaiGRPCGetResponseProxy->Activate();
}

void UConvaiChatbotComponent::Bind_GRPC_Request_Delegates()
{
	if (!IsValid(ConvaiGRPCGetResponseProxy))
	{
		UE_LOG(ConvaiChatbotComponentLog, Warning, TEXT("Bind_GRPC_Request_Delegates: ConvaiGRPCGetResponseProxy is invalid"));
		return;
	}

	ConvaiGRPCGetResponseProxy->OnTranscriptionReceived.BindUObject(this, &UConvaiChatbotComponent::OnTranscriptionReceived);
	ConvaiGRPCGetResponseProxy->OnDataReceived.BindUObject(this, &UConvaiChatbotComponent::onResponseDataReceived);
	ConvaiGRPCGetResponseProxy->OnFaceDataReceived.BindUObject(this, &UConvaiChatbotComponent::OnFaceDataReceived);
	ConvaiGRPCGetResponseProxy->OnSessionIDReceived.BindUObject(this, &UConvaiChatbotComponent::onSessionIDReceived);
	ConvaiGRPCGetResponseProxy->OnActionsReceived.BindUObject(this, &UConvaiChatbotComponent::onActionSequenceReceived);
	ConvaiGRPCGetResponseProxy->OnNarrativeDataReceived.BindUObject(this, &UConvaiChatbotComponent::OnNarrativeSectionReceived);
	ConvaiGRPCGetResponseProxy->OnEmotionReceived.BindUObject(this, &UConvaiChatbotComponent::onEmotionReceived);
	ConvaiGRPCGetResponseProxy->OnFinish.BindUObject(this, &UConvaiChatbotComponent::onFinishedReceivingData);
	ConvaiGRPCGetResponseProxy->OnFailure.BindUObject(this, &UConvaiChatbotComponent::onFailure);
}

void UConvaiChatbotComponent::Unbind_GRPC_Request_Delegates()
{
	if (!IsValid(ConvaiGRPCGetResponseProxy))
	{
		return;
	}

	ConvaiGRPCGetResponseProxy->OnTranscriptionReceived.Unbind();
	ConvaiGRPCGetResponseProxy->OnDataReceived.Unbind();
	ConvaiGRPCGetResponseProxy->OnFaceDataReceived.Unbind();
	ConvaiGRPCGetResponseProxy->OnSessionIDReceived.Unbind();
	ConvaiGRPCGetResponseProxy->OnActionsReceived.Unbind();
	ConvaiGRPCGetResponseProxy->OnEmotionReceived.Unbind();
	ConvaiGRPCGetResponseProxy->OnFinish.Unbind();
	ConvaiGRPCGetResponseProxy->OnFailure.Unbind();
}

void UConvaiChatbotComponent::Cleanup(bool StreamConnectionFinished)
{
	UE_LOG(ConvaiChatbotComponentLog, Log, TEXT("Cleanup"));
	if (IsValid(CurrentConvaiPlayerComponent))
	{
		CurrentConvaiPlayerComponent->getOnDataReceivedDelegate().Unbind();
	}
	CurrentConvaiPlayerComponent = nullptr;

	StreamInProgress = false;
	//Environment.reset();
}

void UConvaiChatbotComponent::onMicrophoneDataReceived()
{
	//UE_LOG(ConvaiChatbotComponentLog, Log, TEXT("onMicrophoneDataReceived"));
	if (!IsValid(CurrentConvaiPlayerComponent))
	{
		UE_LOG(ConvaiChatbotComponentLog, Warning, TEXT("onMicrophoneDataReceived: CurrentConvaiPlayerComponent is invalid"));
		return;
	}

	CurrentConvaiPlayerComponent->getOnDataReceivedDelegate().Unbind();
}

//void UConvaiChatbotComponent::onAudioFinished()
//{
//	if (!StreamInProgress && IsTalking) // A false StreamInProgress means we have finished reading from the stream
//	{
//		OnFinishedTalking.Broadcast();
//		IsTalking = false;
//	}
//}

void UConvaiChatbotComponent::Broadcast_OnTranscriptionReceived_Implementation(const FString& Transcription, bool IsTranscriptionReady, bool IsFinal)
{
	// Execute if you are a client
	if (!UKismetSystemLibrary::IsServer(this))
	{
		OnTranscriptionReceived(Transcription, IsTranscriptionReady, IsFinal);
	}
}

void UConvaiChatbotComponent::Broadcast_onResponseDataReceived_Implementation(const FString& ReceivedText, bool IsFinal)
{
	if (!UKismetSystemLibrary::IsServer(this))
	{
		onResponseDataReceived(ReceivedText, TArray<uint8>(), 0, IsFinal);
	}
}

void UConvaiChatbotComponent::Broadcast_onSessionIDReceived_Implementation(const FString& ReceivedSessionID)
{
	if (!UKismetSystemLibrary::IsServer(this))
	{
		onSessionIDReceived(ReceivedSessionID);
	}
}

void UConvaiChatbotComponent::Broadcast_onActionSequenceReceived_Implementation(const TArray<FConvaiResultAction>& ReceivedSequenceOfActions)
{
	if (!UKismetSystemLibrary::IsServer(this))
	{
		onActionSequenceReceived(ReceivedSequenceOfActions);
	}
}

void UConvaiChatbotComponent::Broadcast_OnNarrativeSectionReceived_Implementation(const FString& BT_Code, const FString& BT_Constants, const FString& ReceivedNarrativeSectionID)
{
	if (!UKismetSystemLibrary::IsServer(this))
	{
		OnNarrativeSectionReceived(BT_Code, BT_Constants, ReceivedNarrativeSectionID);
	}
}

void UConvaiChatbotComponent::Broadcast_onEmotionReceived_Implementation(const FString& ReceivedEmotionResponse)
{
	if (!UKismetSystemLibrary::IsServer(this))
	{
		onEmotionReceived(ReceivedEmotionResponse);
	}
}

void UConvaiChatbotComponent::OnTranscriptionReceived(FString Transcription, bool IsTranscriptionReady, bool IsFinal)
{
	// Broadcast to clients
	if (UKismetSystemLibrary::IsServer(this) && ReplicateVoiceToNetwork)
	{
		Broadcast_OnTranscriptionReceived(Transcription, IsTranscriptionReady, IsFinal);
	}

	AsyncTask(ENamedThreads::GameThread, [this, Transcription, IsTranscriptionReady, IsFinal]
		{
			FString PlayerName = IsValid(CurrentConvaiPlayerComponent) ? CurrentConvaiPlayerComponent->PlayerName : LastPlayerName;
			OnTranscriptionReceivedEvent_V2.Broadcast(this, CurrentConvaiPlayerComponent, PlayerName, Transcription, IsTranscriptionReady, IsFinal);

			// Run the deprecated event
			OnTranscriptionReceivedEvent.Broadcast(Transcription, IsTranscriptionReady, IsFinal);
		});
	LastTranscription = Transcription;
	ReceivedFinalTranscription = IsFinal;
}

void UConvaiChatbotComponent::onResponseDataReceived(const FString ReceivedText, const TArray<uint8>& ReceivedAudio, uint32 SampleRate, bool IsFinal)
{
	// Broadcast to clients
	if (UKismetSystemLibrary::IsServer(this) && ReplicateVoiceToNetwork)
	{
		Broadcast_onResponseDataReceived(ReceivedText, IsFinal);
	}

	AsyncTask(ENamedThreads::GameThread, [this, ReceivedText, ReceivedAudio, SampleRate, IsFinal]
		{
			float AudioDuration = 0;
			if (VoiceResponse && ReceivedAudio.Num() > 0)
			{
				AudioDuration = float(ReceivedAudio.Num() - 44) / float(SampleRate * 2); // Assuming 1 channel
				AddPCMDataToSend(ReceivedAudio, false, SampleRate, 1); // Should be called in the game thread
			}

			// Send text and audio duration to blueprint event
			OnTextReceivedEvent_V2.Broadcast(this, CurrentConvaiPlayerComponent, CharacterName, ReceivedText, AudioDuration, IsFinal);

			// Run the deprecated event
			OnTextReceivedEvent.Broadcast(CharacterName, ReceivedText, AudioDuration, IsFinal);
		});
	ReceivedFinalData = IsFinal;
}

void UConvaiChatbotComponent::OnFaceDataReceived(FAnimationSequence FaceDataAnimation)
{
	AddFaceDataToSend(FaceDataAnimation);
}

void UConvaiChatbotComponent::onSessionIDReceived(const FString ReceivedSessionID)
{
	// Broadcast to clients
	if (UKismetSystemLibrary::IsServer(this) && ReplicateVoiceToNetwork)
	{
		Broadcast_onSessionIDReceived(ReceivedSessionID);
	}

	SessionID = ReceivedSessionID;
}

void UConvaiChatbotComponent::onActionSequenceReceived(const TArray<FConvaiResultAction>& ReceivedSequenceOfActions)
{
	// Broadcast to clients
	if (UKismetSystemLibrary::IsServer(this) && ReplicateVoiceToNetwork)
	{
		Broadcast_onActionSequenceReceived(ReceivedSequenceOfActions);
	}

	if (UConvaiUtils::IsNewActionSystemEnabled())
	{
		bool ActionsAlreadyStarted = !IsActionsQueueEmpty();

		// Fill the current queue of actions
		AppendActionsToQueue(ReceivedSequenceOfActions);

		if (!ActionsAlreadyStarted)
			StartFirstAction();
	}

	// Broadcast the actions
	AsyncTask(ENamedThreads::GameThread, [this, ReceivedSequenceOfActions] {
		OnActionReceivedEvent_V2.Broadcast(this, CurrentConvaiPlayerComponent, ReceivedSequenceOfActions);

		// Run the deprecated event
		OnActionReceivedEvent.Broadcast(ReceivedSequenceOfActions); 
		});
}

void UConvaiChatbotComponent::onEmotionReceived(FString ReceivedEmotionResponse)
{
	if (LockEmotionState)
		return;

	// Broadcast to clients
	if (UKismetSystemLibrary::IsServer(this) && ReplicateVoiceToNetwork)
	{
		Broadcast_onEmotionReceived(ReceivedEmotionResponse);
	}

	// Update teh emotion state
	EmotionState.SetEmotionData(ReceivedEmotionResponse);

	// Broadcast the emotion state changed event
	AsyncTask(ENamedThreads::GameThread, [this] {
		OnEmotionStateChangedEvent.Broadcast(this, CurrentConvaiPlayerComponent);
		});
}

void UConvaiChatbotComponent::onFinishedReceivingData()
{
	if (ConvaiGRPCGetResponseProxy)
	{
		UE_LOG(ConvaiChatbotComponentLog, Log, TEXT("UConvaiChatbotComponent Request Finished!"));
		Unbind_GRPC_Request_Delegates();
		ConvaiGRPCGetResponseProxy = nullptr;
	}
}

void UConvaiChatbotComponent::OnNarrativeSectionReceived(FString BT_Code, FString BT_Constants, FString ReceivedNarrativeSectionID)
{
	// Broadcast to clients
	if (UKismetSystemLibrary::IsServer(this) && ReplicateVoiceToNetwork)
	{
		Broadcast_OnNarrativeSectionReceived(BT_Code, BT_Constants, ReceivedNarrativeSectionID);
	}

	AsyncTask(ENamedThreads::GameThread, [this, ReceivedNarrativeSectionID]
		{
			OnNarrativeSectionReceivedEvent.Broadcast(this, ReceivedNarrativeSectionID);
		});
}

void UConvaiChatbotComponent::onFailure()
{
	UE_LOG(ConvaiChatbotComponentLog, Warning, TEXT("UConvaiChatbotComponent Get Response Failed!"));

	// Broadcast the failure
	AsyncTask(ENamedThreads::GameThread, [this] {OnFailureEvent.Broadcast(); });

	onFinishedReceivingData();
}

void UConvaiChatbotComponent::OnRep_EnvironmentData()
{
	if (IsValid(Environment))
	{
		Environment->SetFromEnvironment(ConvaiEnvironmentDetails);
	}
	else
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("OnRep_EnvironmentData: Environment is not valid"));
	}
}

void UConvaiChatbotComponent::UpdateEnvironmentData()
{
	if (IsValid(Environment))
	{
		ConvaiEnvironmentDetails = Environment->ToEnvironmentStruct();
	}
	else
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("UpdateEnvironmentData: Environment is not valid"));
	}
}

void UConvaiChatbotComponent::LoadEnvironment(UConvaiEnvironment* NewConvaiEnvironment)
{
	if (IsValid(Environment))
	{
		Environment->SetFromEnvironment(NewConvaiEnvironment);
	}
	else
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("LoadEnvironment: Environment is not valid"));
	}
}

bool UConvaiChatbotComponent::CheckTokenValidity()
{
	if (!IsValid(CurrentConvaiPlayerComponent) || !CurrentConvaiPlayerComponent->CheckTokenValidty(Token))
	{
		// Clear the time out timer just in case it was triggered by previous lack of data
		ClearTimeOutTimer();
		return false;
	}
	return true;
}

void UConvaiChatbotComponent::OnPlayerTimeOut()
{
	TimeOutTimerHandle.Invalidate();
	CurrentConvaiPlayerComponent = nullptr;
	UE_LOG(ConvaiChatbotComponentLog, Warning, TEXT("Player timed out"));
}

void UConvaiChatbotComponent::ClearTimeOutTimer()
{
	if (TimeOutTimerHandle.IsValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(TimeOutTimerHandle);
		TimeOutTimerHandle.Invalidate();
	}
}

void UConvaiChatbotComponent::BeginPlay()
{
	Super::BeginPlay();
	
	Environment = NewObject<UConvaiEnvironment>();

	if (IsValid(Environment))
	{
		Environment->OnEnvironmentChanged.BindUObject(this, &UConvaiChatbotComponent::UpdateEnvironmentData);

	}
	else
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("BeginPlay: Environment is not valid"));
	}

	// Get character details
	if (CharacterID != "")
		ConvaiGetDetails();

}

void UConvaiChatbotComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsValid(ConvaiGRPCGetResponseProxy) || !StreamInProgress)
		return;

	bool ThisIsTheLastWrite = false;

	// CheckTokenValidity() checks if player has stopped sending audio
	if ((!CheckTokenValidity() && StreamInProgress))
	{
		StreamInProgress = false;
		ThisIsTheLastWrite = true;
	}

	bool Successfull = false;
	uint8 Buffer[ConvaiConstants::VoiceCaptureChunk];
	if (IsValid(CurrentConvaiPlayerComponent))
	{
		// Consume the mic stream into our buffer
		Successfull = CurrentConvaiPlayerComponent->ConsumeStreamingBuffer(Buffer, ConvaiConstants::VoiceCaptureChunk);
	}
	else
	{
		// Finish the stream
		StreamInProgress = false;
		ThisIsTheLastWrite = true;
	}

	if (Successfull) // If there is mic data to send
	{
		ConvaiGRPCGetResponseProxy->WriteAudioDataToSend(Buffer, ConvaiConstants::VoiceCaptureChunk, ThisIsTheLastWrite);
	}
	else if (ThisIsTheLastWrite) // If there is no data to send, and we do not expect more mic data to send  
	{
		ConvaiGRPCGetResponseProxy->FinishWriting();
	}
	else // If there is no data to send, but we expect the mic data to come in the near future
	{
		// We did not receive audio from player although the player did not explicitly end sending the audio
		// Start the time out timer if we did not start yet
		if (!TimeOutTimerHandle.IsValid())
		{
			GetWorld()->GetTimerManager().SetTimer(TimeOutTimerHandle, this, &UConvaiChatbotComponent::OnPlayerTimeOut, ConvaiConstants::PlayerTimeOut / 1000, false);
		}
	}

	// Make sure the time out timer is cleared on a successful read or that we have finished reading from the mic
	if (Successfull || !StreamInProgress)
		ClearTimeOutTimer();
}

void UConvaiChatbotComponent::BeginDestroy()
{
	//InterruptSpeech(0);
	if (IsValid(Environment))
	{
		Environment->OnEnvironmentChanged.Unbind();
	}
	Unbind_GRPC_Request_Delegates();
	Cleanup(true);
	Super::BeginDestroy();
}

UConvaiChatBotGetDetailsProxy* UConvaiChatbotComponent::ConvaiGetDetails()
{
	ConvaiChatBotGetDetailsDelegate.BindUFunction(this, "OnConvaiGetDetailsCompleted");

	if (IsValid(ConvaiChatBotGetDetailsProxy))
	{
		ConvaiChatBotGetDetailsProxy->OnSuccess.Clear();
		ConvaiChatBotGetDetailsProxy->OnFailure.Clear();
	}


	ConvaiChatBotGetDetailsProxy = UConvaiChatBotGetDetailsProxy::CreateCharacterGetDetailsProxy(this, CharacterID);
	ConvaiChatBotGetDetailsProxy->OnSuccess.Add(ConvaiChatBotGetDetailsDelegate);
	ConvaiChatBotGetDetailsProxy->OnFailure.Add(ConvaiChatBotGetDetailsDelegate);
	ConvaiChatBotGetDetailsProxy->Activate();
	return ConvaiChatBotGetDetailsProxy;
}

void UConvaiChatbotComponent::OnConvaiGetDetailsCompleted(FString ReceivedCharacterName, FString ReceivedVoiceType, FString ReceivedBackstory, FString ReceivedLanguageCode, bool HasReadyPlayerMeLink, FString ReceivedReadyPlayerMeLink, FString ReceivedAvatarImageLink)
{
	if (ReceivedCharacterName == "" && ReceivedVoiceType == "" && ReceivedBackstory == "")
	{
		UE_LOG(ConvaiChatbotComponentLog, Warning, TEXT("OnConvaiGetDetailsCompleted: Could not get character details for charID:\"%s\""), *CharacterID);
		OnCharacterDataLoadEvent.Broadcast(false);
		return;
	}

	CharacterName = ReceivedCharacterName;
	VoiceType = ReceivedVoiceType;
	Backstory = ReceivedBackstory;
	LanguageCode = ReceivedLanguageCode;
	ReadyPlayerMeLink = ReceivedReadyPlayerMeLink;
	AvatarImageLink = ReceivedAvatarImageLink;

	OnCharacterDataLoadEvent.Broadcast(true);
	ConvaiChatBotGetDetailsProxy = nullptr;
}
