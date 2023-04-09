// Copyright 2022 Convai Inc. All Rights Reserved.

#include "ConvaiChatbotComponent.h"
#include "ConvaiActionUtils.h"
#include "ConvaiUtils.h"
#include "ConvaiPlayerComponent.h"
#include "../Convai.h"
#include "Sound/SoundWaveProcedural.h"
#include "Net/UnrealNetwork.h"
#include "ConvaiChatBotProxy.h"
#include "ConvaiGRPC.h"

DEFINE_LOG_CATEGORY(ConvaiChatbotComponentLog);

UConvaiChatbotComponent::UConvaiChatbotComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
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
}

bool UConvaiChatbotComponent::IsInConversation()
{
	return (IsValid(ConvaiGRPCGetResponseProxy) || IsTalking);
}

bool UConvaiChatbotComponent::IsProcessing()
{
	return (IsValid(ConvaiGRPCGetResponseProxy));
}

bool UConvaiChatbotComponent::GetIsTalking()
{
	return IsTalking;
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

void UConvaiChatbotComponent::StartGetResponseStream(UConvaiPlayerComponent* InConvaiPlayerComponent, FString InputText, UConvaiEnvironment* InEnvironment, bool InGenerateActions, bool InVoiceResponse, bool RunOnServer, uint32 InToken)
{
	//if (!(IsValid(InEnvironment) || IsValid(Environment)) && InGenerateActions)
	//{
	//	UE_LOG(ConvaiChatbotComponentLog, Warning, TEXT("StartGetResponseStream: Environment is not valid"));
	//	return;
	//}

	if (!IsValid(InConvaiPlayerComponent))
	{
		UE_LOG(ConvaiChatbotComponentLog, Warning, TEXT("StartGetResponseStream: ConvaiPlayerComponent is not valid"));
		return;
	}

	if (IsValid(CurrentConvaiPlayerComponent) && CheckTokenValidity())
	{
		UE_LOG(ConvaiChatbotComponentLog, Warning, TEXT("StartGetResponseStream: character is currently being talked to by a player, make sure to run \"Finish Talking\""));
		return;
	}

	//if (IsInConversation())
	//{
	//	UE_LOG(ConvaiChatbotComponentLog, Warning, TEXT("StartGetResponseStream: character is currently in an on-going conversation with a player, use \"Is In Conversation\" function to find out if a character is in a conversation or not"));
	//	return;
	//}


	if (GenerateActions && IsValid(InEnvironment))
		Environment = InEnvironment;

	FString Error;
	if (GenerateActions && !UConvaiActions::ValidateEnvironment(Environment, Error))
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("StartGetResponseStream: %s"), *Error);
		UE_LOG(ConvaiPlayerLog, Log, TEXT("StartGetResponseStream: Environment object seems to have issues -> setting GenerateActions to false"));
		GenerateActions = false;
	}

	if (IsProcessing())
	{
		UE_LOG(ConvaiChatbotComponentLog, Warning, TEXT("StartGetResponseStream: character is still processing the response, use \"Is Processing\" function to find out if a character is still processing the response or not"));
		return;
	}

	UserText = InputText;
	TextInput = UserText.Len()>0;
	GenerateActions = InGenerateActions;
	VoiceResponse = InVoiceResponse;
	ReplicateVoiceToNetwork = RunOnServer;
	Token = InToken;

	if (!TextInput)
	{
		CurrentConvaiPlayerComponent = InConvaiPlayerComponent;
		StreamInProgress = true;
	}

	Start_GRPC_Request();
}

void UConvaiChatbotComponent::StartGetResponseStreamWithText(FString InputText, UConvaiEnvironment* InEnvironment, bool InGenerateActions, bool InVoiceResponse, bool RunOnServer)
{
	if (!(IsValid(InEnvironment) || IsValid(Environment)) && InGenerateActions)
	{
		UE_LOG(ConvaiChatbotComponentLog, Warning, TEXT("StartGetResponseStreamWithText Environment is not valid"));
		return;
	}

	if (IsValid(CurrentConvaiPlayerComponent) && CheckTokenValidity())
	{
		UE_LOG(ConvaiChatbotComponentLog, Warning, TEXT("StartGetResponseStreamWithText: character is currently being talked to by a player, make sure to run \"Finish Talking\" "));
		return;
	}

	if (IsInConversation())
	{
		UE_LOG(ConvaiChatbotComponentLog, Warning, TEXT("StartGetResponseStreamWithText: character is currently in an on-going conversation with a player, use \"Is In Conversation\" function to find out if a character is in a conversation or not"));
		return;
	}

	UserText = InputText;
	GenerateActions = InGenerateActions;
	if (IsValid(InEnvironment))
		Environment = InEnvironment;	
	VoiceResponse = InVoiceResponse;
	ReplicateVoiceToNetwork = RunOnServer;
	TextInput = true;

	Start_GRPC_Request();
}

void UConvaiChatbotComponent::Start_GRPC_Request()
{
	// Create the request proxy
	ConvaiGRPCGetResponseProxy = UConvaiGRPCGetResponseProxy::CreateConvaiGRPCGetResponseProxy(this, UserText, CharacterID, VoiceResponse, SessionID, Environment, GenerateActions);

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
	ConvaiGRPCGetResponseProxy->OnSessionIDReceived.BindUObject(this, &UConvaiChatbotComponent::onSessionIDReceived);
	ConvaiGRPCGetResponseProxy->OnActionsReceived.BindUObject(this, &UConvaiChatbotComponent::onActionSequenceReceived);
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
	ConvaiGRPCGetResponseProxy->OnSessionIDReceived.Unbind();
	ConvaiGRPCGetResponseProxy->OnActionsReceived.Unbind();
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

void UConvaiChatbotComponent::OnTranscriptionReceived(FString Transcription, bool IsTranscriptionReady, bool IsFinal)
{
	AsyncTask(ENamedThreads::GameThread, [this, Transcription, IsTranscriptionReady, IsFinal]
		{
			OnTranscriptionReceivedEvent.Broadcast(Transcription, IsTranscriptionReady, IsFinal);
		});
}

void UConvaiChatbotComponent::onResponseDataReceived(const FString ReceivedText, const TArray<uint8>& ReceivedAudio, uint32 SampleRate, bool IsFinal)
{
	AsyncTask(ENamedThreads::GameThread, [this, ReceivedText, ReceivedAudio, SampleRate, IsFinal]
		{
			float AudioDuration = 0;
			if (VoiceResponse && ReceivedAudio.Num() > 0)
			{
				AddPCMDataToSend(ReceivedAudio, false, SampleRate, 1); // Should be called in the game thread
				AudioDuration = float(ReceivedAudio.Num() - 44) / float(SampleRate * 2); // Assuming 1 channel
			}

			// Send text and audio duration to blueprint event
			OnTextReceivedEvent.Broadcast(CharacterName, ReceivedText, AudioDuration, IsFinal);
		});
}

void UConvaiChatbotComponent::onSessionIDReceived(const FString ReceivedSessionID)
{
	SessionID = ReceivedSessionID;
}

void UConvaiChatbotComponent::onActionSequenceReceived(const TArray<FConvaiResultAction>& ReceivedSequenceOfActions)
{
	// Broadcast the actions
	AsyncTask(ENamedThreads::GameThread, [this, ReceivedSequenceOfActions] {OnActionReceivedEvent.Broadcast(ReceivedSequenceOfActions); });
}

void UConvaiChatbotComponent::onFinishedReceivingData()
{
	UE_LOG(ConvaiChatbotComponentLog, Log, TEXT("UConvaiChatbotComponent Request Finished!"));
	Unbind_GRPC_Request_Delegates();
	ConvaiGRPCGetResponseProxy = nullptr;
}

void UConvaiChatbotComponent::onFailure()
{
	UE_LOG(ConvaiChatbotComponentLog, Warning, TEXT("UConvaiChatbotComponent Get Response Failed!"));

	// Broadcast the failure
	AsyncTask(ENamedThreads::GameThread, [this] {OnFailureEvent.Broadcast(); });

	onFinishedReceivingData();
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
	Environment = NewObject<UConvaiEnvironment>();

	// Get character details
	if (CharacterID != "")
		ConvaiGetDetails();

	Super::BeginPlay();
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
	Unbind_GRPC_Request_Delegates();
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

void UConvaiChatbotComponent::OnConvaiGetDetailsCompleted(FString ReceivedCharacterName, FString ReceivedVoiceType, FString ReceivedBackstory, bool HasReadyPlayerMeLink, FString ReceivedReadyPlayerMeLink)
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
	ReadyPlayerMeLink = ReceivedReadyPlayerMeLink;

	OnCharacterDataLoadEvent.Broadcast(true);
	ConvaiChatBotGetDetailsProxy = nullptr;
}
