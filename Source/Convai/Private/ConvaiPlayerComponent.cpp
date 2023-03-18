// Copyright 2022 Convai Inc. All Rights Reserved.


#include "ConvaiPlayerComponent.h"
#include "ConvaiUtils.h"
#include "Misc/FileHelper.h"
#include "Http.h"
#include "ConvaiUtils.h"
#include "VoiceModule.h"
#include "Containers/UnrealString.h"
#include "Kismet/GameplayStatics.h"
#include "ConviDefinitions.h"
#include "ConvaiActionUtils.h"

#include "ConvaiChatbotComponent.h"

#include "Sound/SoundWave.h"

DEFINE_LOG_CATEGORY(ConvaiPlayerLog);

UConvaiPlayerComponent::UConvaiPlayerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	bAutoActivate = true;
	VoiceCaptureRingBuffer.Init(ConvaiConstants::VoiceCaptureRingBufferCapacity);
	VoiceCaptureBuffer.Empty(ConvaiConstants::VoiceCaptureBufferSize);
}
 
void UConvaiPlayerComponent::Init()
{
	if (VoiceCapture)
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("VoiceCapture is already init"));
		return;
	}

	FString commandMicNoiseGateThreshold = "voice.MicNoiseGateThreshold  0.01";
	FString commandSilenceDetectionThreshold = "voice.SilenceDetectionThreshold 0.01";

	APlayerController* PController = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (PController)
	{
		PController->ConsoleCommand(*commandMicNoiseGateThreshold, true);
		PController->ConsoleCommand(*commandSilenceDetectionThreshold, true);
	}

	VoiceCapture = FVoiceModule::Get().CreateVoiceCapture("", ConvaiConstants::VoiceCaptureSampleRate, 1);
	if (VoiceCapture.IsValid())
	{
		//UE_LOG(ConvaiPlayerLog, Warning, TEXT("UConvaiPlayerComponent::Init VoiceCapture.IsValid()  valid VoiceCapture->GetBufferSize() %d"), VoiceCapture->GetBufferSize());
		IsInit = true;
	}
	else 
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("VoiceCapture is not valid - Please check that 1) you have edited DefaultEngine.ini as in the documentation/tutorials, and 2) your microphone is connected and is set as the default input"));
	}

	Token = 0;
}

void UConvaiPlayerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsInit)
	{
			return;
	}

	if (!VoiceCapture.IsValid())
	{
		return;
	}

	//UE_LOG(ConvaiPlayerLog, Log, TEXT("GetIsStreaming(): %d"), int(GetIsStreaming()));


	uint32 VoiceCaptureBytesAvailable = 0;
	EVoiceCaptureState::Type CaptureState = VoiceCapture->GetCaptureState(VoiceCaptureBytesAvailable);

	if (CaptureState != EVoiceCaptureState::Ok || VoiceCaptureBytesAvailable == 0)
	{
		//UE_LOG(ConvaiPlayerLog, Log, TEXT("CaptureState: %d, VoiceCaptureBytesAvailable: %d"), int(CaptureState), VoiceCaptureBytesAvailable);
		return;
	}

	TArray<uint8> TempBuffer;
	TempBuffer.SetNumUninitialized(VoiceCaptureBytesAvailable);

	// get current voice data
	uint64 OutSampleCounter = 0;
	uint32 OutAvailableVoiceData = 0;
	VoiceCapture->GetVoiceData(TempBuffer.GetData(), VoiceCaptureBytesAvailable, OutAvailableVoiceData, OutSampleCounter);

	//UE_LOG(ConvaiPlayerLog, Log, TEXT("VoiceCaptureBytesAvailable: %d bytes | OutAvailableVoiceData: %d | TempBuffer.Num(): %d bytes"), VoiceCaptureBytesAvailable, OutAvailableVoiceData, TempBuffer.Num());
	//UE_LOG(ConvaiPlayerLog, Log, TEXT("OutAvailableVoiceData: %d bytes| VoiceCaptureBuffer.Num(): %d bytes "), OutAvailableVoiceData, VoiceCaptureBuffer.Num());

	if (!ReplicateVoiceToNetwork)
	{
		if (IsRecording)
			VoiceCaptureBuffer.Append(TempBuffer);

		if (IsStreaming)
			VoiceCaptureRingBuffer.Enqueue(TempBuffer.GetData(), TempBuffer.Num());

		onDataReceived_Delegate.ExecuteIfBound();
	}
	else
	{
		// Stream voice data
		AddPCMDataToSend(TempBuffer, false, ConvaiConstants::VoiceCaptureSampleRate, 1);
	}

}

void UConvaiPlayerComponent::StartRecording()
{
	if (!IsInit)
	{
		UE_LOG(ConvaiPlayerLog, Log, TEXT("StartRecording Initializing..."));
		Init();
		if (!VoiceCapture)
			return;
	}

	if (VoiceCapture && !VoiceCapture.IsValid())
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("StartRecording VoiceCapture is not valid"));
		IsInit = false;
		IsRecording = false;
		return;
	}

	if (IsRecording)
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("StartRecording: already recording!"));
		return;
	}

	UE_LOG(ConvaiPlayerLog, Log, TEXT("Started Recording "));
	VoiceCapture->Start();
	IsRecording = true;
	VoiceCaptureBuffer.Empty(ConvaiConstants::VoiceCaptureBufferSize);
}

USoundWave* UConvaiPlayerComponent::FinishRecording()
{
	if (!IsRecording)
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("StopRecording did not start recording"));
		return nullptr;
	}

	if (VoiceCapture && !VoiceCapture.IsValid())
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("StopRecording VoiceCapture is not valid"));
		return nullptr;
	}


	if (!VoiceCapture->IsCapturing()) 
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("StopRecording voice capture is not capturing"));
		return nullptr;
	}



	UE_LOG(ConvaiPlayerLog, Log, TEXT("Stopped Recording "));
	VoiceCapture->Stop();
	IsRecording = false;

	if (VoiceCaptureBuffer.Num() <100)
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("StopRecording too little voice data: %d"), VoiceCaptureBuffer.Num());
		return nullptr;
	}

	//UE_LOG(ConvaiPlayerLog, Log, TEXT("VoiceCaptureBuffer.Num() final: %d bytes "), VoiceCaptureBuffer.Num());
	USoundWave* OutSoundWave = UConvaiUtils::PCMDataToSoundWav(VoiceCaptureBuffer, 1, ConvaiConstants::VoiceCaptureSampleRate);
	//UE_LOG(ConvaiPlayerLog, Log, TEXT("OutSoundWave->GetDuration(): %d seconds "), OutSoundWave->GetDuration());
	
	//USoundWave* OutSoundWave = UConvaiUtils::WavDataToSoundWave(VoiceCaptureBuffer);


	return OutSoundWave;
}

void UConvaiPlayerComponent::StartTalking(
	UConvaiChatbotComponent* ConvaiChatbotComponent,
	UConvaiEnvironment* Environment,
	bool GenerateActions,
	bool VoiceResponse,
	bool RunOnServer,
	bool StreamPlayerMic)
{
	if (!IsInit)
	{
		UE_LOG(ConvaiPlayerLog, Log, TEXT("StartTalking Initializing..."));
		Init();
		if (!VoiceCapture)
			return;
	}

	if (VoiceCapture && !VoiceCapture.IsValid())
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("StartTalking VoiceCapture is not valid"));
		IsInit = false;
		IsStreaming = false;
		return;
	}

	if (IsStreaming)
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("StartTalking: already talking!"));
		return;
	}

	FString Error;
	if (GenerateActions && !UConvaiActions::ValidateEnvironment(Environment, Error))
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("StartTalking: %s"), *Error);
		UE_LOG(ConvaiPlayerLog, Log, TEXT("StartTalking: Environment object seems to have issues -> setting GenerateActions to false"));
		GenerateActions = false;
	}


	if (!IsValid(ConvaiChatbotComponent))
	{
		
		if (RunOnServer)
		{
			UE_LOG(ConvaiPlayerLog, Log, TEXT("StartTalking: ConvaiChatbotComponent is not valid, will stream voice chat to network instead"));
		}
		else
		{
			UE_LOG(ConvaiPlayerLog, Warning, TEXT("StartTalking: ConvaiChatbotComponent is not valid"));
			return;
		}
	}

	UE_LOG(ConvaiPlayerLog, Log, TEXT("Started Talking"));

	

	VoiceCapture->Start();
	IsStreaming = true;
	VoiceCaptureRingBuffer.Empty();


	ReplicateVoiceToNetwork = RunOnServer;
	
	if (RunOnServer)
	{
		if (GenerateActions)
			StartTalkingServer(ConvaiChatbotComponent, Environment->Actions, Environment->Objects, Environment->Characters, Environment->MainCharacter, GenerateActions, VoiceResponse, StreamPlayerMic);
		else
			StartTalkingServer(ConvaiChatbotComponent, TArray<FString>(), TArray<FConvaiObjectEntry>(), TArray<FConvaiObjectEntry>(), FConvaiObjectEntry(), GenerateActions, VoiceResponse, StreamPlayerMic);
	}
	else
	{
		ConvaiChatbotComponent->StartGetResponseStream(this, Environment, GenerateActions, VoiceResponse, false, Token);
	}
}

void UConvaiPlayerComponent::FinishTalking()
{
	if (!IsStreaming)
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("FinishTalking did not start talking"));
		return;
	}

	if (!VoiceCapture || !VoiceCapture.IsValid())
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("FinishTalking VoiceCapture is not valid"));
		return;
	}


	if (!VoiceCapture->IsCapturing())
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("FinishTalking voice capture is not capturing"));
		return;
	}


	VoiceCapture->Stop();
	IsStreaming = false;

	if (ReplicateVoiceToNetwork)
	{
		FinishTalkingServer();
	}
	else
	{
		// Invalidate the token by generating a new one
		GenerateNewToken();
	}

	UE_LOG(ConvaiPlayerLog, Log, TEXT("Finished Talking"));

	//if (!IsValid(ConvaiChatbotComponent))
	//{
	//	UE_LOG(ConvaiPlayerLog, Warning, TEXT("FinishTalking: ConvaiChatbotComponent is not valid"));
	//	return;
	//}

	//FinishStreaming();

	//if (ReplicateVoiceToNetwork)
	//{
	//	FinishTalkingServer(ConvaiChatbotComponent);
	//}
	//else
	//{
	//	ConvaiChatbotComponent->FinishGetResponseStream();
	//}
}

void UConvaiPlayerComponent::StartTalkingServer_Implementation(
	class UConvaiChatbotComponent* ConvaiChatbotComponent,
	const TArray<FString>& Actions,
	const TArray<FConvaiObjectEntry>& Objects,
	const TArray<FConvaiObjectEntry>& Characters,
	FConvaiObjectEntry MainCharacter,
	bool GenerateActions,
	bool VoiceResponse,
	bool StreamPlayerMic)
{
	// if "StreamPlayerMic" is true then "bShouldMuteGlobal" should be false, meaning we will play the player's audio on other clients
	bShouldMuteGlobal = !StreamPlayerMic;

	// Make sure the ring buffer is empty
	VoiceCaptureRingBuffer.Empty();

	// if "ConvaiChatbotComponent" is valid then run StartGetResponseStream function
	if (IsValid(ConvaiChatbotComponent))
	{
		UConvaiEnvironment* Environment = nullptr;
		if (GenerateActions)
		{
			if (IsValid(ConvaiChatbotComponent->Environment))
				Environment = ConvaiChatbotComponent->Environment;
			else
			{
				Environment = UConvaiEnvironment::CreateConvaiEnvironment();
			}

			Environment->Actions = Actions;
			Environment->Characters = Characters;
			Environment->Objects = Objects;
			Environment->MainCharacter = MainCharacter;
		}
		ConvaiChatbotComponent->StartGetResponseStream(this, Environment, GenerateActions, VoiceResponse, true, Token);
	}
}

void UConvaiPlayerComponent::FinishTalkingServer_Implementation()
{
	// Invalidate the token by generating a new one
	GenerateNewToken();
}

void UConvaiPlayerComponent::SendText(UConvaiChatbotComponent* ConvaiChatbotComponent, FString Text, UConvaiEnvironment* Environment, bool GenerateActions, bool VoiceResponse, bool RunOnServer)
{
	FString Error;
	if (GenerateActions && !UConvaiActions::ValidateEnvironment(Environment, Error))
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("SendText: %s"), *Error);
		UE_LOG(ConvaiPlayerLog, Log, TEXT("SendText: Environment object seems to have issues -> setting GenerateActions to false"));
		GenerateActions = false;
	}


	if (!IsValid(ConvaiChatbotComponent))
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("SendText: ConvaiChatbotComponent is not valid"));
		return;
	}

	ReplicateVoiceToNetwork = RunOnServer;

	if (RunOnServer)
	{
		if (GenerateActions)
			SendTextServer(ConvaiChatbotComponent, Text, Environment->Actions, Environment->Objects, Environment->Characters, Environment->MainCharacter, GenerateActions, VoiceResponse);
		else
			SendTextServer(ConvaiChatbotComponent, Text, TArray<FString>(), TArray<FConvaiObjectEntry>(), TArray<FConvaiObjectEntry>(), FConvaiObjectEntry(), GenerateActions, VoiceResponse);
	}
	else
	{
		ConvaiChatbotComponent->StartGetResponseStreamWithText(Text, Environment, GenerateActions, VoiceResponse, false);
	}
}

void UConvaiPlayerComponent::SendTextServer_Implementation(
	UConvaiChatbotComponent* ConvaiChatbotComponent,
	const FString& Text,
	const TArray<FString>& Actions,
	const TArray<FConvaiObjectEntry>& Objects,
	const TArray<FConvaiObjectEntry>& Characters,
	FConvaiObjectEntry MainCharacter,
	bool GenerateActions,
	bool VoiceResponse)
{
	if (!IsValid(ConvaiChatbotComponent))
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("SendTextServer: ConvaiChatbotComponent is not valid"));
		return;
	}

	UConvaiEnvironment* Environment;
	if (IsValid(ConvaiChatbotComponent->Environment))
		Environment = ConvaiChatbotComponent->Environment;
	else
	{
		Environment = UConvaiEnvironment::CreateConvaiEnvironment();
	}

	Environment->Actions = Actions;
	Environment->Characters = Characters;
	Environment->Objects = Objects;
	Environment->MainCharacter = MainCharacter;

	ConvaiChatbotComponent->StartGetResponseStreamWithText(Text, Environment, GenerateActions, VoiceResponse, true);
}

bool UConvaiPlayerComponent::ShouldMuteLocal()
{
	return true;
}

bool UConvaiPlayerComponent::ShouldMuteGlobal()
{
	return bShouldMuteGlobal;
}

void UConvaiPlayerComponent::OnServerAudioReceived(uint8* VoiceData, uint32 VoiceDataSize, bool ContainsHeaderData, uint32 SampleRate, uint32 NumChannels)
{
	//if (IsRecording)
	//	VoiceCaptureBuffer.Append(VoiceData, VoiceDataSize);

	//if (IsStreaming)
	//	VoiceCaptureRingBuffer.Enqueue(VoiceData, VoiceDataSize);

	//UE_LOG(ConvaiPlayerLog, Log, TEXT("OnServerAudioReceived received %d bytes"), VoiceDataSize);
	//onDataReceived_Delegate.ExecuteIfBound();

	// Simply enque the data into the ring buffer
	VoiceCaptureRingBuffer.Enqueue(VoiceData, VoiceDataSize);
}

void UConvaiPlayerComponent::BeginPlay()
{
	Super::BeginPlay();
}

bool UConvaiPlayerComponent::ConsumeStreamingBuffer(uint8* Buffer, uint32 length)
{
	if (VoiceCaptureRingBuffer.RingDataUsage() < length)
		return false;
	// TODO (Mohamed): add functionality to consume data <= Length
	VoiceCaptureRingBuffer.Dequeue(Buffer, length);
	return true;
}

void UConvaiPlayerComponent::SetIsStreamingServer_Implementation(bool value)
{
	IsStreaming = value;
	if (IsStreaming == false)
		onDataReceived_Delegate.ExecuteIfBound(); // In case a consumer was waiting on this delegate
}