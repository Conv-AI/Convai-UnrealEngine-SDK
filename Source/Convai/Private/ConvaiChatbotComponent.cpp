// Copyright 2022 Convai Inc. All Rights Reserved.

#include "ConvaiChatbotComponent.h"
#include "ConvaiPlayerComponent.h"
#include "../Convai.h"
#include "ConvaiGRPC.h"
#include "ConvaiActionUtils.h"
#include "ConvaiUtils.h"
#include "LipSyncInterface.h"
#include "VisionInterface.h"

#include "Sound/SoundWaveProcedural.h"
#include "Net/UnrealNetwork.h"
#include "ConvaiChatBotProxy.h"
#include "Kismet/KismetSystemLibrary.h"
#include "TimerManager.h"
#include "Async/Async.h"

DEFINE_LOG_CATEGORY(ConvaiChatbotComponentLog);

UConvaiChatbotComponent::UConvaiChatbotComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 1 / 15;
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
	DOREPLIFETIME(UConvaiChatbotComponent, LanguageCode);
	DOREPLIFETIME(UConvaiChatbotComponent, CurrentConvaiPlayerComponent);
	DOREPLIFETIME(UConvaiChatbotComponent, ActionsQueue);
	DOREPLIFETIME(UConvaiChatbotComponent, EmotionState);
	DOREPLIFETIME(UConvaiChatbotComponent, LockEmotionState);
	DOREPLIFETIME(UConvaiChatbotComponent, ConvaiEnvironmentDetails);
}

bool UConvaiChatbotComponent::IsInConversation()
{
	return ((IsValid(ConvaiGRPCGetResponseProxy) && !ReceivedFinalData) || IsTalking);
}

bool UConvaiChatbotComponent::IsProcessing()
{
	if (IsValid(ConvaiGRPCGetResponseProxy) && !ReceivedFinalData)
		return true;
	else if (!GetIsTalking() && !DataBuffer.IsEmpty())
		return true;
	else
		return false;
}

bool UConvaiChatbotComponent::IsListening()
{
	return CanWriteToGetResponseStream();
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
		// TODO: Reset DataBuffer.TotalAudioDurationElapsed after response is complete
		TimeElapsed = DataBuffer.TotalAudioDurationElapsed + GetWorld()->GetTimerManager().GetTimerElapsed(AudioFinishedTimerHandle);
	}

	return TimeElapsed;
}

float UConvaiChatbotComponent::GetTalkingTimeRemaining()
{
	float TimeRemaing = 0;
	if (IsValid(GetWorld()))
	{
		TimeRemaing = GetWorld()->GetTimerManager().GetTimerRemaining(AudioFinishedTimerHandle);
		TimeRemaing = TimeRemaing < 0 ? 0 : TimeRemaing;
		float InSyncTimeRemaining = 0;
		HasSufficentLipsyncFrames(InSyncTimeRemaining); // Takes into consideration the lipsync time
		TimeRemaing += InSyncTimeRemaining;
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

void UConvaiChatbotComponent::ClearActionQueue()
{
	ActionsQueue.Empty();
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

TMap<FName, float> UConvaiChatbotComponent::GetEmotionBlendshapes()
{
	return EmotionBlendshapes;
}

void UConvaiChatbotComponent::ResetEmotionState()
{
	EmotionState.ResetEmotionScores();
	OnEmotionStateChangedEvent.Broadcast(this, CurrentConvaiPlayerComponent);
}

void UConvaiChatbotComponent::StartRecordingVoice()
{
	if (IsRecordingAudio)
	{
		UE_LOG(ConvaiChatbotComponentLog, Warning, TEXT("Cannot start Recording voice while already recording voice"));
		return;
	}
	IsRecordingAudio = true;
}

USoundWave* UConvaiChatbotComponent::FinishRecordingVoice()
{
	UE_LOG(ConvaiChatbotComponentLog, Log, TEXT("Finished Recording Audio - Total bytes: %d - Duration: %f"), RecordedAudio.Num(), UConvaiUtils::CalculateAudioDuration(RecordedAudio.Num(), 1,RecordedAudioSampleRate, 2));

	if (!IsRecordingAudio)
		return nullptr;
	USoundWave* SoundWave = UConvaiUtils::PCMDataToSoundWav(RecordedAudio, 1, RecordedAudioSampleRate);
	IsRecordingAudio = false;
	RecordedAudio.Empty();
	return SoundWave;
}

bool UConvaiChatbotComponent::PlayRecordedVoice(USoundWave* RecordedVoice)
{
	if (!IsValid(RecordedVoice))
	{
		UE_LOG(ConvaiChatbotComponentLog, Warning, TEXT("Recorded voice is not valid"));
		return false;
	}

	if (IsRecordingAudio)
	{
		UE_LOG(ConvaiChatbotComponentLog, Warning, TEXT("Cannot Play Recorded voice while Recording voice"));
		return false;
	}

	if (GetIsTalking())
	{
		UE_LOG(ConvaiChatbotComponentLog, Warning, TEXT("Playing Recorded voice and stopping currently playing voice"));
		InterruptSpeech(0);
	}

	UE_LOG(ConvaiChatbotComponentLog, Log, TEXT("Play Recorded Audio - Duration: %f"), RecordedVoice->Duration);

	InterruptSpeech(InterruptVoiceFadeOutDuration);

	ForcePlayVoice(RecordedVoice);

	return true;
}

void UConvaiChatbotComponent::StartGetResponseStream(UConvaiPlayerComponent* InConvaiPlayerComponent, FString InputText, UConvaiEnvironment* InEnvironment, bool InGenerateActions, bool InVoiceResponse, bool RunOnServer, bool UseOverrideAuthKey, FString OverrideAuthKey, FString OverrideAuthHeader, uint32 InToken)
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
		UE_LOG(ConvaiChatbotComponentLog, Warning, TEXT("StartGetResponseStream: Environment is not valid"));
	}

	FString Error;
	bool ValidEnvironment = UConvaiActions::ValidateEnvironment(Environment, Error);
	if (GenerateActions && !ValidEnvironment)
	{
		UE_LOG(ConvaiChatbotComponentLog, Warning, TEXT("StartGetResponseStream: %s"), *Error);
		UE_LOG(ConvaiChatbotComponentLog, Log, TEXT("StartGetResponseStream: Environment object seems to have issues -> setting GenerateActions to false"));
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


	InterruptSpeech(InterruptVoiceFadeOutDuration);

	UserText = InputText;
	TextInput = UserText.Len() > 0;
	GenerateActions = InGenerateActions;
	VoiceResponse = InVoiceResponse;
	ReplicateVoiceToNetwork = RunOnServer;
	Token = InToken;
	CurrentConvaiPlayerComponent = InConvaiPlayerComponent;
	LastPlayerName = CurrentConvaiPlayerComponent->PlayerName;

	if (!TextInput)
	{
		StreamInProgress = true;
	}
	else
	{
		OnTranscriptionReceived(UserText, true, true);
	}


	Start_GRPC_Request(UseOverrideAuthKey, OverrideAuthKey, OverrideAuthHeader);
}

void UConvaiChatbotComponent::FinishGetResponseStream(UConvaiPlayerComponent* InConvaiPlayerComponent)
{
	if (!HasOnGoingGetResponseStream())
	{
		UE_LOG(ConvaiChatbotComponentLog, Log, TEXT("UConvaiChatbotComponent::FinishGetResponseStream Trying to finish a non-existent stream | Character ID : %s | Session ID : %s"),
			*CharacterID,
			*SessionID);
		return;
	}

	if (!CanWriteToGetResponseStream())
	{
		UE_LOG(ConvaiChatbotComponentLog, Log, TEXT("UConvaiChatbotComponent::FinishGetResponseStream stream is no longer writable | Character ID : %s | Session ID : %s"),
			*CharacterID,
			*SessionID);
		return;
	}

	if (CurrentConvaiPlayerComponent != InConvaiPlayerComponent)
	{
		UE_LOG(ConvaiChatbotComponentLog, Log, TEXT("UConvaiChatbotComponent::FinishGetResponseStream Trying to finish using a player that did not start the stream | Character ID : %s | Session ID : %s"),
			*CharacterID,
			*SessionID);
		return;
	}

	ConvaiGRPCGetResponseProxy->FinishWriting();
	ClearTimeOutTimer();
}

void UConvaiChatbotComponent::ExecuteNarrativeTrigger(FString TriggerMessage, UConvaiEnvironment* InEnvironment, bool InGenerateActions, bool InVoiceResponse, bool InReplicateOnNetwork)
{
	if (TriggerMessage.IsEmpty())
	{
		UE_LOG(ConvaiChatbotComponentLog, Warning, TEXT("Invoke Speech: TriggerMessage is missing"));
		return;
	}
	UE_LOG(ConvaiChatbotComponentLog, Log, TEXT("Invoke Speech: Executed | Character ID : %s | Session ID : %s"),
		*CharacterID,
		*SessionID);
	InvokeTrigger_Internal("", TriggerMessage, InEnvironment, InGenerateActions, InVoiceResponse, InReplicateOnNetwork);
}

void UConvaiChatbotComponent::InvokeNarrativeDesignTrigger(FString TriggerName, UConvaiEnvironment* InEnvironment, bool InGenerateActions, bool InVoiceResponse, bool InReplicateOnNetwork)
{
	if (TriggerName.IsEmpty())
	{
		UE_LOG(ConvaiChatbotComponentLog, Warning, TEXT("Invoke Narrative Design Trigger: TriggerName is missing"));
		return;
	}
	UE_LOG(ConvaiChatbotComponentLog, Log, TEXT("Invoke Narrative Design Trigger: Executed | Character ID : %s | Session ID : %s"),
		*CharacterID,
		*SessionID);
	InvokeTrigger_Internal(TriggerName, "", InEnvironment, InGenerateActions, InVoiceResponse, InReplicateOnNetwork);
}

void UConvaiChatbotComponent::InvokeTrigger_Internal(FString TriggerName, FString TriggerMessage, UConvaiEnvironment* InEnvironment, bool InGenerateActions, bool InVoiceResponse, bool InReplicateOnNetwork)
{
	if (TriggerMessage.IsEmpty() && TriggerName.IsEmpty())
	{
		UE_LOG(ConvaiChatbotComponentLog, Warning, TEXT("InvokeTrigger_Internal: TriggerName and TriggerMessage are missing - Please supply one of them"));
		return;
	}

	if (IsValid(Environment))
	{
		Environment->SetFromEnvironment(InEnvironment);
	}
	else
	{
		UE_LOG(ConvaiChatbotComponentLog, Warning, TEXT("InvokeTrigger_Internal: Environment is not valid"));
	}

	FString Error;
	bool ValidEnvironment = UConvaiActions::ValidateEnvironment(Environment, Error);
	if (GenerateActions && !ValidEnvironment)
	{
		UE_LOG(ConvaiChatbotComponentLog, Warning, TEXT("InvokeTrigger_Internal: %s"), *Error);
		UE_LOG(ConvaiChatbotComponentLog, Log, TEXT("InvokeTrigger_Internal: Environment object seems to have issues -> setting GenerateActions to false"));
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

	InterruptSpeech(InterruptVoiceFadeOutDuration);

	UserText = "";
	GenerateActions = InGenerateActions;
	VoiceResponse = InVoiceResponse;
	ReplicateVoiceToNetwork = InReplicateOnNetwork;

	Start_GRPC_Request(false, "", "", TriggerName, TriggerMessage);
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
		UE_LOG(ConvaiChatbotComponentLog, Log, TEXT("InterruptSpeech: Interrupting character | Character ID : %s | Session ID : %s"),
			*CharacterID,
			*SessionID);
		onFinishedReceivingData();
		StopVoiceWithFade(InVoiceFadeOutDuration);

		if (ReceivedFinalTranscription == false)
			OnTranscriptionReceived(LastTranscription, true, true);

		if (ReceivedFinalData == false)
			onResponseDataReceived(FString(""), TArray<uint8>(), 0, true);

		AsyncTask(ENamedThreads::GameThread, [WeakThis = MakeWeakObjectPtr(this)]
			{
				if (!WeakThis.IsValid())
				{
					// The object is no longer valid or is being destroyed.
					return;
				}
				WeakThis->OnInterruptedEvent.Broadcast(WeakThis.Get(), WeakThis->CurrentConvaiPlayerComponent);
			});
	}
	else
	{
		StopVoice(); // Make sure to stop the voice either way
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

void UConvaiChatbotComponent::Start_GRPC_Request(bool UseOverrideAuthKey, FString OverrideAuthKey, FString OverrideAuthHeader, FString TriggerName, FString TriggerMessage)
{
	TPair<FString, FString> AuthHeaderAndKey = UConvaiUtils::GetAuthHeaderAndKey();
	FString AuthKey = AuthHeaderAndKey.Value;
	FString AuthHeader = AuthHeaderAndKey.Key;

	AuthKey = UseOverrideAuthKey ? OverrideAuthKey : AuthKey;
	AuthHeader = UseOverrideAuthKey ? OverrideAuthHeader : AuthHeader;

	bool RequireFaceData = false;
	bool GeneratesVisemesAsBlendshapes = false;
	ReceivedFinalData = false;
	if (ConvaiLipSyncExtended)
	{
		RequireFaceData = ConvaiLipSyncExtended->RequiresPreGeneratedFaceData();
		GeneratesVisemesAsBlendshapes = ConvaiLipSyncExtended->GeneratesVisemesAsBlendshapes();
	}
	RequireFaceData = RequireFaceData && VoiceResponse;

	FConvaiGRPCVisionParams ConvaiGRPCVisionParams;
	if (ConvaiVision && ConvaiVision->GetState() == EVisionState::Capturing)
	{
		bool bCaptureSuccess = ConvaiVision->GetCompressedData(ConvaiGRPCVisionParams.width, ConvaiGRPCVisionParams.height, ConvaiGRPCVisionParams.data);

		// Attempt to capture compressed data if pre-compressed data isn't available
		if (!bCaptureSuccess)
		{
			constexpr float CompressionQuality = 60.0f;
			bCaptureSuccess = ConvaiVision->CaptureCompressed(ConvaiGRPCVisionParams.width, ConvaiGRPCVisionParams.height, ConvaiGRPCVisionParams.data, CompressionQuality);
		}

		// Fallback to raw data capture if compressed capture fails
		if (!bCaptureSuccess)
		{
			ConvaiVision->CaptureRaw(ConvaiGRPCVisionParams.width, ConvaiGRPCVisionParams.height, ConvaiGRPCVisionParams.data);
		}
	}


	// Create the request proxy
	FConvaiGRPCGetResponseParams Params;
	Params.UserQuery = UserText;
	Params.TriggerName = TriggerName;
	Params.TriggerMessage = TriggerMessage;
	Params.CharID = CharacterID;
	Params.VoiceResponse = VoiceResponse;
	Params.RequireFaceData = RequireFaceData;
	Params.GeneratesVisemesAsBlendshapes = GeneratesVisemesAsBlendshapes;
	Params.Narrative_Template_Keys = NarrativeTemplateKeys;
	Params.SessionID = SessionID;
	Params.Environment = Environment;
	Params.GenerateActions = GenerateActions;
	Params.ConvaiGRPCVisionParams = ConvaiGRPCVisionParams;
	Params.AuthKey = AuthKey;
	Params.AuthHeader = AuthHeader;

	ConvaiGRPCGetResponseProxy = UConvaiGRPCGetResponseProxy::CreateConvaiGRPCGetResponseProxy(this, Params);

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
	ConvaiGRPCGetResponseProxy->OnInteractionIDReceived.BindUObject(this, &UConvaiChatbotComponent::onInteractionIDReceived);
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
	ConvaiGRPCGetResponseProxy->OnInteractionIDReceived.Unbind();
	ConvaiGRPCGetResponseProxy->OnEmotionReceived.Unbind();
	ConvaiGRPCGetResponseProxy->OnFinish.Unbind();
	ConvaiGRPCGetResponseProxy->OnFailure.Unbind();
}

void UConvaiChatbotComponent::Cleanup(bool StreamConnectionFinished)
{
	UE_LOG(ConvaiChatbotComponentLog, Log, TEXT("Cleanup | Character ID : %s | Session ID : %s"),
		*CharacterID,
		*SessionID);
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

void UConvaiChatbotComponent::Broadcast_onInteractionIDReceived_Implementation(const FString& ReceivedInteractionID)
{
	if (!UKismetSystemLibrary::IsServer(this))
	{
		onInteractionIDReceived(ReceivedInteractionID);
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

void UConvaiChatbotComponent::Broadcast_onEmotionReceived_Implementation(const FString& ReceivedEmotionResponse, bool MultipleEmotions)
{
	if (!UKismetSystemLibrary::IsServer(this))
	{
		onEmotionReceived(ReceivedEmotionResponse, FAnimationFrame(), MultipleEmotions);
	}
}

void UConvaiChatbotComponent::OnTranscriptionReceived(FString Transcription, bool IsTranscriptionReady, bool IsFinal)
{
	LastTranscription = Transcription;
	ReceivedFinalTranscription = IsFinal;

	// Broadcast to clients
	if (UKismetSystemLibrary::IsServer(this) && ReplicateVoiceToNetwork)
	{
		if (IsInGameThread())
		{
			Broadcast_OnTranscriptionReceived(Transcription, IsTranscriptionReady, IsFinal);
		}
		else
		{
			AsyncTask(ENamedThreads::GameThread, [this, Transcription, IsTranscriptionReady, IsFinal]
				{
					Broadcast_OnTranscriptionReceived(Transcription, IsTranscriptionReady, IsFinal);
				});
		}
	}

	FString PlayerName = IsValid(CurrentConvaiPlayerComponent) ? CurrentConvaiPlayerComponent->PlayerName : LastPlayerName;

	if (IsInGameThread())
	{
		OnTranscriptionReceivedEvent_V2.Broadcast(this, CurrentConvaiPlayerComponent, PlayerName, Transcription, IsTranscriptionReady, IsFinal);

		// Run the deprecated event
		OnTranscriptionReceivedEvent.Broadcast(Transcription, IsTranscriptionReady, IsFinal);
	}
	else
	{
		AsyncTask(ENamedThreads::GameThread, [this, Transcription, IsTranscriptionReady, IsFinal, PlayerName]
			{
				OnTranscriptionReceivedEvent_V2.Broadcast(this, CurrentConvaiPlayerComponent, PlayerName, Transcription, IsTranscriptionReady, IsFinal);

				// Run the deprecated event
				OnTranscriptionReceivedEvent.Broadcast(Transcription, IsTranscriptionReady, IsFinal);
			});
	}
}

void UConvaiChatbotComponent::onResponseDataReceived(const FString ReceivedText, const TArray<uint8>& ReceivedAudio, uint32 SampleRate, bool IsFinal)
{
	// Broadcast to clients
	if (UKismetSystemLibrary::IsServer(this) && ReplicateVoiceToNetwork)
	{
		if (IsInGameThread())
		{
			Broadcast_onResponseDataReceived(ReceivedText, IsFinal);
		}
		else
		{
			AsyncTask(ENamedThreads::GameThread, [this, ReceivedText, IsFinal]
				{
					Broadcast_onResponseDataReceived(ReceivedText, IsFinal);
				});
		}
		
	}

	float ReceieivedAudioDuration = float(ReceivedAudio.Num() - 44) / float(SampleRate * 2); // Assuming 1 channel

			if (VoiceResponse && ReceivedAudio.Num() > 0)
			{
				AddPCMDataToSend(ReceivedAudio, false, SampleRate, 1); // Should be called in the game thread

				//FString BasePath = TEXT("F:/Work/Convai/UE_Animation_Dev/TestRecordedAudioChunks");
				//FString Timestamp = FDateTime::Now().ToString();
				//Timestamp.ReplaceInline(TEXT(":"), TEXT("_"));
				//FString FileName = FString::Printf(TEXT("Audio_%s.wav"), *Timestamp);
				//FString FullPath = FPaths::Combine(*BasePath, *FileName);

				//TArray<uint8> OutWaveFileData;
				//UConvaiUtils::PCMDataToWav(ReceivedAudio, OutWaveFileData, 1, SampleRate);
				//UConvaiUtils::SaveByteArrayAsFile(FullPath, OutWaveFileData);

				if (IsRecordingAudio)
				{
					RecordedAudio.Append(ReceivedAudio);
					RecordedAudioSampleRate = SampleRate;
				}
			}



	if (IsInGameThread())
	{
		// Send text and audio duration to blueprint event
		OnTextReceivedEvent_V2.Broadcast(this, CurrentConvaiPlayerComponent, CharacterName, ReceivedText, ReceieivedAudioDuration, IsFinal);

		// Run the deprecated event
		OnTextReceivedEvent.Broadcast(CharacterName, ReceivedText, ReceieivedAudioDuration, IsFinal);
	}
	else
	{
		AsyncTask(ENamedThreads::GameThread, [this, ReceivedText, ReceivedAudio, SampleRate, IsFinal, ReceieivedAudioDuration]
			{
				// Send text and audio duration to blueprint event
				OnTextReceivedEvent_V2.Broadcast(this, CurrentConvaiPlayerComponent, CharacterName, ReceivedText, ReceieivedAudioDuration, IsFinal);

				// Run the deprecated event
				OnTextReceivedEvent.Broadcast(CharacterName, ReceivedText, ReceieivedAudioDuration, IsFinal);
			});
	}

	if (ReceieivedAudioDuration > 0)
	{
		TotalReceivedAudioDuration += ReceieivedAudioDuration;
	}
	if (IsFinal)
	{
		UE_LOG(ConvaiChatbotComponentLog, Log, TEXT("Chatbot Total Received Audio: %f seconds"), TotalReceivedAudioDuration);
		TotalReceivedAudioDuration = 0;
	}

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
		if (IsInGameThread())
		{
			Broadcast_onSessionIDReceived(ReceivedSessionID);
		}
		else
		{
			AsyncTask(ENamedThreads::GameThread, [this, ReceivedSessionID]
				{
					Broadcast_onSessionIDReceived(ReceivedSessionID);
				});
		}
	}

	SessionID = ReceivedSessionID;
}

void UConvaiChatbotComponent::onInteractionIDReceived(FString ReceivedInteractionID)
{
	// Broadcast to clients
	if (UKismetSystemLibrary::IsServer(this) && ReplicateVoiceToNetwork)
	{
		if (IsInGameThread())
		{
			Broadcast_onInteractionIDReceived(ReceivedInteractionID);
		}
		else
		{
			AsyncTask(ENamedThreads::GameThread, [this, ReceivedInteractionID]
				{
					Broadcast_onInteractionIDReceived(ReceivedInteractionID);
				});
		}
	}

	if (IsInGameThread())
	{
		// Send Interaction ID to blueprint event
		OnInteractionIDReceivedEvent.Broadcast(this, CurrentConvaiPlayerComponent, ReceivedInteractionID);
	}
	else
	{
		AsyncTask(ENamedThreads::GameThread, [this, ReceivedInteractionID]
			{
				// Send Interaction ID to blueprint event
				OnInteractionIDReceivedEvent.Broadcast(this, CurrentConvaiPlayerComponent, ReceivedInteractionID);
			});
	}
}

void UConvaiChatbotComponent::onActionSequenceReceived(const TArray<FConvaiResultAction>& ReceivedSequenceOfActions)
{
	// Broadcast to clients
	if (UKismetSystemLibrary::IsServer(this) && ReplicateVoiceToNetwork)
	{
		if (IsInGameThread())
		{
			Broadcast_onActionSequenceReceived(ReceivedSequenceOfActions);
		}
		else
		{
			AsyncTask(ENamedThreads::GameThread, [this, ReceivedSequenceOfActions]
				{
					Broadcast_onActionSequenceReceived(ReceivedSequenceOfActions);
				});
		}
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

void UConvaiChatbotComponent::onEmotionReceived(FString ReceivedEmotionResponse, FAnimationFrame EmotionBlendshapesFrame, bool MultipleEmotions)
{
	if (LockEmotionState)
		return;

	// Broadcast to clients
	if (UKismetSystemLibrary::IsServer(this) && ReplicateVoiceToNetwork)
	{
		if (IsInGameThread())
		{
			Broadcast_onEmotionReceived(ReceivedEmotionResponse, MultipleEmotions);
		}
		else
		{
			AsyncTask(ENamedThreads::GameThread, [this, ReceivedEmotionResponse, MultipleEmotions]
				{
					Broadcast_onEmotionReceived(ReceivedEmotionResponse, MultipleEmotions);
				});
		}
	}

	// Update the emotion state
	if (!ReceivedEmotionResponse.IsEmpty())
	{
		if (MultipleEmotions)
		{
			EmotionState.SetEmotionData(ReceivedEmotionResponse, EmotionOffset);
		}
		else
		{
			EmotionState.SetEmotionDataSingleEmotion(ReceivedEmotionResponse, EmotionOffset);
			//EmotionBlendshapes = EmotionBlendshapesFrame.BlendShapes;
		}
	}

	// Broadcast the emotion state changed event
	AsyncTask(ENamedThreads::GameThread, [this] {
		OnEmotionStateChangedEvent.Broadcast(this, CurrentConvaiPlayerComponent);
		});
}

void UConvaiChatbotComponent::onFinishedReceivingData()
{
	if (ConvaiGRPCGetResponseProxy)
	{
		UE_LOG(ConvaiChatbotComponentLog, Log, TEXT("UConvaiChatbotComponent Request Finished! | Character ID : %s | Session ID : %s"),
			*CharacterID,
			*SessionID);
		Unbind_GRPC_Request_Delegates();
		ConvaiGRPCGetResponseProxy = nullptr;
	}
}

void UConvaiChatbotComponent::OnNarrativeSectionReceived(FString BT_Code, FString BT_Constants, FString ReceivedNarrativeSectionID)
{
	// Broadcast to clients
	if (UKismetSystemLibrary::IsServer(this) && ReplicateVoiceToNetwork)
	{
		if (IsInGameThread())
		{
			Broadcast_OnNarrativeSectionReceived(BT_Code, BT_Constants, ReceivedNarrativeSectionID);
		}
		else
		{
			AsyncTask(ENamedThreads::GameThread, [this, BT_Code, BT_Constants, ReceivedNarrativeSectionID]
				{
					Broadcast_OnNarrativeSectionReceived(BT_Code, BT_Constants, ReceivedNarrativeSectionID);
				});
		}
		
	}

	AsyncTask(ENamedThreads::GameThread, [this, ReceivedNarrativeSectionID]
		{
			OnNarrativeSectionReceivedEvent.Broadcast(this, ReceivedNarrativeSectionID);
		});
}

void UConvaiChatbotComponent::onFailure()
{
	UE_LOG(ConvaiChatbotComponentLog, Warning, TEXT("UConvaiChatbotComponent Get Response Failed! | Character ID : %s | Session ID : %s"),
		*CharacterID,
		*SessionID);

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
		UE_LOG(ConvaiChatbotComponentLog, Warning, TEXT("OnRep_EnvironmentData: Environment is not valid"));
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
		UE_LOG(ConvaiChatbotComponentLog, Warning, TEXT("UpdateEnvironmentData: Environment is not valid"));
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
		UE_LOG(ConvaiChatbotComponentLog, Warning, TEXT("LoadEnvironment: Environment is not valid"));
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
	ClearTimeOutTimer();
	CurrentConvaiPlayerComponent = nullptr;
	UE_LOG(ConvaiChatbotComponentLog, Warning, TEXT("Player timed out | Character ID : %s | Session ID : %s"),
		*CharacterID,
		*SessionID);
}

void UConvaiChatbotComponent::ClearTimeOutTimer()
{
	if (TimeOutTimerHandle.IsValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(TimeOutTimerHandle);
		TimeOutTimerHandle.Invalidate();
	}
}

bool UConvaiChatbotComponent::HasOnGoingGetResponseStream()
{
	return IsValid(ConvaiGRPCGetResponseProxy) && !ConvaiGRPCGetResponseProxy->IsStreamFinished();
}

bool UConvaiChatbotComponent::CanWriteToGetResponseStream()
{
	return IsValid(ConvaiGRPCGetResponseProxy) && HasOnGoingGetResponseStream() && ConvaiGRPCGetResponseProxy->CanWriteToStream();
}

bool UConvaiChatbotComponent::ConsumeMicStreamIntoBuffer()
{
	if (!IsValid(CurrentConvaiPlayerComponent))
		return false;

	// Consume the mic stream into our buffer
	bool Successful = false;
	PlayerInpuAudioBuffer.Empty(PlayerInpuAudioBuffer.Max()); // Empty the buffer but keep its memory allocation intact
	Successful = CurrentConvaiPlayerComponent->ConsumeStreamingBuffer(PlayerInpuAudioBuffer);

	return Successful;
}

void UConvaiChatbotComponent::BeginPlay()
{
	Super::BeginPlay();

	Environment = NewObject<UConvaiEnvironment>();

	PlayerInpuAudioBuffer.SetNumUninitialized(ConvaiConstants::VoiceCaptureSampleRate * 10); // Buffer allocated 10 seconds of audio into memory

	if (IsValid(Environment))
	{
		Environment->OnEnvironmentChanged.BindUObject(this, &UConvaiChatbotComponent::UpdateEnvironmentData);

	}
	else
	{
		UE_LOG(ConvaiChatbotComponentLog, Warning, TEXT("BeginPlay: Environment is not valid"));
	}

	// Get character details
	if (CharacterID != "")
		ConvaiGetDetails();

}

void UConvaiChatbotComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!CanWriteToGetResponseStream())
	{
		ClearTimeOutTimer();
		return;
	}

	bool Successful = ConsumeMicStreamIntoBuffer();
	if (Successful)
		ConvaiGRPCGetResponseProxy->WriteAudioDataToSend(PlayerInpuAudioBuffer.GetData(), PlayerInpuAudioBuffer.Num());

	if (!Successful)
	{
		// Start the time out timer if we did not start yet
		if (!TimeOutTimerHandle.IsValid())
		{
			GetWorld()->GetTimerManager().SetTimer(TimeOutTimerHandle, this, &UConvaiChatbotComponent::OnPlayerTimeOut, ConvaiConstants::PlayerTimeOut / 1000, false);
		}
	}
	else	// Make sure the time out timer is cleared on a successful read or that we have finished reading from the mic
	{
		ClearTimeOutTimer();
	}
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

bool UConvaiChatbotComponent::CanUseLipSync()
{
	return true;
}

bool UConvaiChatbotComponent::CanUseVision()
{
    return true;
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
