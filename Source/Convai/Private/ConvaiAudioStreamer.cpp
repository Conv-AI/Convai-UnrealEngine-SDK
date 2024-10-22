// Copyright 2022 Convai Inc. All Rights Reserved.

#include "ConvaiAudioStreamer.h"
#include "Sound/SoundWave.h"
#include "Engine.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/KismetSystemLibrary.h"
#include "LipSyncInterface.h"
#include "VisionInterface.h"
#include "Math/UnrealMathUtility.h"
#include "ConvaiUtils.h"

// THIRD_PARTY_INCLUDES_START
#include "opus.h"
// THIRD_PARTY_INCLUDES_END

/** Maximum number of frames in a single Opus packet */
#define MAX_OPUS_FRAMES_PER_PACKET 48
/** Number of max frames for buffer calculation purposes */
#define MAX_OPUS_FRAMES 6

/**
 * Number of samples per channel of available space in PCM during decompression.
 * If this is less than the maximum packet duration (120ms; 5760 for 48kHz), opus will not be capable of decoding some packets.
 */
#define MAX_OPUS_FRAME_SIZE MAX_OPUS_FRAMES * 320
 /** Hypothetical maximum for buffer validation */
#define MAX_OPUS_UNCOMPRESSED_BUFFER_SIZE 48 * 1024
/** 20ms frame sizes are a good choice for most applications (1000ms / 20ms = 50) */
//#define NUM_OPUS_FRAMES_PER_SEC 50
#define NUM_OPUS_FRAMES_PER_SEC 50

#define OPUS_CHECK_CTL(Category, CTL) \
	if (ErrCode != OPUS_OK) \
	{ \
		UE_LOG(Category, Warning, TEXT("Failure to get CTL %s"), #CTL); \
	}


DEFINE_LOG_CATEGORY(ConvaiAudioStreamerLog);

UConvaiAudioStreamer::UConvaiAudioStreamer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	bAutoActivate = true;
}

void UConvaiAudioStreamer::BroadcastVoiceDataToClients_Implementation(TArray<uint8> const& EncodedVoiceData, uint32 SampleRate, uint32 NumChannels, uint32 SizeBeforeEncode)
{
	// Check that decoder is valid and able to decode the input sample rate and channels
	if (!Decoder || SampleRate != DecoderSampleRate || NumChannels != DecoderNumChannels)
	{
		DestroyOpusDecoder();
		InitDecoder(SampleRate, NumChannels);
		UE_LOG(ConvaiAudioStreamerLog, Log, TEXT("Initialized Decoder with SampleRate:%d and Channels:%d"), DecoderSampleRate, DecoderNumChannels);
	}


	// Make sure we have a big enough buffer for decoding
	if (ReceivedEncodedAudioDataBuffer.GetAllocatedSize() < SizeBeforeEncode * 20) {
		ReceivedEncodedAudioDataBuffer.Reserve(SizeBeforeEncode * 20);
	}

	// Decode the Audio data
	uint32 outsize = ReceivedEncodedAudioDataBuffer.GetAllocatedSize();
	//UE_LOG(ConvaiAudioStreamerLog, Log, TEXT("Allocated buffer size for decoding: %d bytes"), outsize);

	Decode(EncodedVoiceData.GetData(), EncodedVoiceData.Num(), ReceivedEncodedAudioDataBuffer.GetData(), outsize);

	// Do not play incomming audio on the client instance, if this component is owned by the client and "ShouldMuteLocal() == true", which means that we mute the audio locally
	// Do not play if we want to mute on all clients "ShouldMuteGlobal() == true"
	if (!(ShouldMuteLocal() && GetOwner()->HasLocalNetOwner()) && !ShouldMuteGlobal())
	{
		PlayVoiceSynced(ReceivedEncodedAudioDataBuffer.GetData(), outsize, false, SampleRate, NumChannels);
	}

	// Run this on server only
	if (UKismetSystemLibrary::IsServer(this))
	{
		OnServerAudioReceived(ReceivedEncodedAudioDataBuffer.GetData(), outsize, false, SampleRate, NumChannels);
	}

	//UE_LOG(ConvaiAudioStreamerLog, Log, TEXT("Decoder Received %d bytes and Outputted %d bytes"), EncodedVoiceData.Num(), outsize);
}

//void UConvaiAudioStreamer::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
//{
//	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
//
//	DOREPLIFETIME(UConvaiAudioStreamer, ReplicateVoiceToNetwork);
//}

void UConvaiAudioStreamer::ProcessEncodedVoiceData_Implementation(TArray<uint8> const& EncodedVoiceData, uint32 SampleRate, uint32 NumChannels, uint32 SizeBeforeEncode)
{
	BroadcastVoiceDataToClients(EncodedVoiceData, SampleRate, NumChannels, SizeBeforeEncode);
}

bool UConvaiAudioStreamer::ShouldMuteLocal()
{
	return false;
}

bool UConvaiAudioStreamer::ShouldMuteGlobal()
{
	return false;
}

void UConvaiAudioStreamer::PlayVoiceSynced(uint8* VoiceData, uint32 VoiceDataSize, bool ContainsHeaderData, uint32 SampleRate, uint32 NumChannels)
{
	// if ReplicateVoiceToNetwork is true then just play the voice data right away to avoid the lipsync cutoff issue
	if (!SupportsLipSync() || ConvaiLipSyncExtended == nullptr || !ConvaiLipSyncExtended->RequiresPreGeneratedFaceData() || ReplicateVoiceToNetwork)
	{
		PlayVoiceData(VoiceData, VoiceDataSize, ContainsHeaderData, SampleRate, NumChannels);
		return;
	}

	if (ContainsHeaderData)
	{
		VoiceDataSize -= 44;
	}

	float AudioDuration = UConvaiUtils::CalculateAudioDuration(VoiceDataSize, NumChannels, SampleRate, 2);
	ConvaiAudioChunk AudioChunk = ConvaiAudioChunk(TArray<uint8>(VoiceData, VoiceDataSize), AudioDuration, NumChannels, SampleRate, 2);
	DataBuffer.Enqueue(AudioChunk);

	UE_LOG(ConvaiAudioStreamerLog, Warning, TEXT("PlayVoiceSynced: Added Audio Chunk - Audio Duration: %f"), AudioDuration);

	//if (!IsTalking)
	//	PauseLipSync();
}

void UConvaiAudioStreamer::PlayVoiceData(uint8* VoiceData, uint32 VoiceDataSize, bool ContainsHeaderData, uint32 SampleRate, uint32 NumChannels)
{
	if (IsVoiceCurrentlyFading())
		StopVoice();
	ResetVoiceFade();

	uint32 PCM_DataSize = VoiceDataSize;

	if (ContainsHeaderData)
	{
		// Parse Wav header
		FWaveModInfo WaveInfo;
		FString ErrorReason;
		bool ParseSuccess = WaveInfo.ReadWaveInfo(VoiceData, VoiceDataSize, &ErrorReason);
		// Set the number of channels and sample rate for the first time reading from the stream
		if (ParseSuccess)
		{
			// Validate that the world exists
			if (!IsValid(GetWorld()))
				return;

			SampleRate = *WaveInfo.pSamplesPerSec;
			NumChannels = *WaveInfo.pChannels;
			PCM_DataSize = *WaveInfo.pWaveDataSize;
		}
		else if (!ParseSuccess)
		{
			UE_LOG(ConvaiAudioStreamerLog, Warning, TEXT("PlayVoiceData: Failed to parse wav header, reason: %s"), *ErrorReason);
		}
	}


	AsyncTask(ENamedThreads::GameThread, [this, PCM_DataSize, SampleRate]
	{
		if (!IsValid(GetWorld()))
		{
			UE_LOG(ConvaiAudioStreamerLog, Warning, TEXT("PlayVoiceData: GetWorld() is Invalid!"));
			return;
		}

		// TODO (Mohamed): take number of channels in consideration when calculating the duration
		// Duration = PCM Data Size / (Sample Rate * Bytes per sample)
		float NewAudioDuration = float(PCM_DataSize) / float(SampleRate * 2);

		float CurrentRemainingAudioDuration = GetWorld()->GetTimerManager().GetTimerRemaining(AudioFinishedTimerHandle);
		if (CurrentRemainingAudioDuration < 0)
			CurrentRemainingAudioDuration = 0; // Can never be less than zero

		//if (CurrentRemainingAudioDuration == 0)
		//	NewAudioDuration -= 0.1; // Hacky way - Reduce the duration by a small amount so that the OnAudioFinished would be called early and this will cause no gap between voice chunks

		// New Duration = Remaining Duration + New Duration
		float TotalAudioDuration = CurrentRemainingAudioDuration + NewAudioDuration;

		GetWorld()->GetTimerManager().SetTimer(AudioFinishedTimerHandle, this, &UConvaiAudioStreamer::onAudioFinished, TotalAudioDuration, false);
	});

	if (!IsValid(SoundWaveProcedural))
		return;

	// TODO (Mohamed) : Needs further testing, especially when sample rate or NumChannels changes
	// Check that SoundWaveProcedural is valid and able to play input sample rate and channels
	if (SoundWaveProcedural->GetSampleRateForCurrentPlatform() != SampleRate || SoundWaveProcedural->NumChannels != NumChannels)
	{
		//if (IsValid(SoundWaveProcedural)) // Destroy SoundWaveProcedural if it is valid
		//	SoundWaveProcedural->ConditionalBeginDestroy();

		SoundWaveProcedural->SetSampleRate(SampleRate);
		SoundWaveProcedural->NumChannels = NumChannels;
		SoundWaveProcedural->Duration = INDEFINITELY_LOOPING_DURATION;
		SoundWaveProcedural->SoundGroup = SOUNDGROUP_Voice;
		SoundWaveProcedural->bLooping = false;
		SoundWaveProcedural->bProcedural = true;
		SoundWaveProcedural->Pitch = 1.0f;
		SoundWaveProcedural->Volume = 1.0f;
		SoundWaveProcedural->AttenuationSettings = nullptr;
		SoundWaveProcedural->bDebug = true;
		SoundWaveProcedural->VirtualizationMode = EVirtualizationMode::PlayWhenSilent;

		UE_LOG(ConvaiAudioStreamerLog, Log, TEXT("New SampleRate: %d"), SampleRate);
		UE_LOG(ConvaiAudioStreamerLog, Log, TEXT("New Channels: %d"), NumChannels);

		AsyncTask(ENamedThreads::GameThread, [this]
		{
			SetSound(SoundWaveProcedural);
			Play();
		});
	}

	if (ContainsHeaderData)
	{
		// Play only the PCM data which start after 44 bytes
		VoiceData += 44;
		VoiceDataSize -= 44;
	}

	SoundWaveProcedural->QueueAudio(VoiceData, VoiceDataSize);


	if (!IsTalking)
	{
		onAudioStarted();
		IsTalking = true;
	}

	// Does the lipsync component require the blendshapes/Visemes to be sent to it
	if (ConvaiLipSyncExtended && ConvaiLipSyncExtended->RequiresPreGeneratedFaceData())
		return;
	else
		// Lipsync component process the audio data to generate the lipsync
		PlayLipSync(VoiceData, VoiceDataSize, SampleRate, NumChannels);
}

void UConvaiAudioStreamer::ForcePlayVoice(USoundWave* VoiceToPlay)
{
	int32 SampleRate;
	int32 NumChannels;
	TArray<uint8> PCMData = UConvaiUtils::ExtractPCMDataFromSoundWave(VoiceToPlay, SampleRate, NumChannels);
	PlayVoiceData(PCMData.GetData(), PCMData.Num(), false, SampleRate, NumChannels);
}

void UConvaiAudioStreamer::StopVoice()
{
	DataBuffer.Empty();
	CurrentChunkDuration = 0;
	CurrentChunkLipSyncFrameRate = 0;
	LastFrameIndex = -1;
	CurrentChunkFrameCounter = 0;

	if (!IsTalking && DataBuffer.IsEmpty())
		return;
	if (SoundWaveProcedural)
		SoundWaveProcedural->ResetAudio();
	
	bIsQueuingLipsync = true;
	StopLipSync();
	onAudioFinished();
	ClearAudioFinishedTimer();
}

void UConvaiAudioStreamer::PauseVoice()
{
	if (bIsPaused)
		return;

	GetWorld()->GetTimerManager().PauseTimer(AudioFinishedTimerHandle);
	SetPaused(true);
	IsTalking = false;
}

void UConvaiAudioStreamer::ResumeVoice()
{
	if (!bIsPaused)
		return;

	AsyncTask(ENamedThreads::GameThread, [this]
	{
		GetWorld()->GetTimerManager().UnPauseTimer(AudioFinishedTimerHandle);
	});
	SetPaused(false);
}

void UConvaiAudioStreamer::StopVoiceWithFade(float InVoiceFadeOutDuration)
{
	if (!IsTalking && DataBuffer.IsEmpty())
		return;

	if (!IsValid(GetWorld()))
	{
		UE_LOG(ConvaiAudioStreamerLog, Warning, TEXT("PlayVoiceData: GetWorld() is Invalid!"));
		return;
	}

	float CurrentRemainingAudioDuration = GetWorld()->GetTimerManager().GetTimerRemaining(AudioFinishedTimerHandle);
	TotalVoiceFadeOutTime = FMath::Min(InVoiceFadeOutDuration, CurrentRemainingAudioDuration);
	RemainingVoiceFadeOutTime = TotalVoiceFadeOutTime;

	if (TotalVoiceFadeOutTime <= 0)
		StopVoice();
}

void UConvaiAudioStreamer::ResetVoiceFade()
{
	if (IsValid(SoundWaveProcedural))
		SoundWaveProcedural->Volume = 1.0f;
	TotalVoiceFadeOutTime = 0;
	RemainingVoiceFadeOutTime = 0;
}

void UConvaiAudioStreamer::UpdateVoiceFade(float DeltaTime)
{
	if (!IsVoiceCurrentlyFading() || !IsValid(SoundWaveProcedural))
		return;
	RemainingVoiceFadeOutTime -= DeltaTime;
	if (RemainingVoiceFadeOutTime <= 0)
	{
		StopVoice();
		return;
	}
	float AudioVolume = RemainingVoiceFadeOutTime / TotalVoiceFadeOutTime;
	SoundWaveProcedural->Volume = AudioVolume;
}

bool UConvaiAudioStreamer::IsVoiceCurrentlyFading()
{
	return (TotalVoiceFadeOutTime > 0 && IsTalking);
}

void UConvaiAudioStreamer::ClearAudioFinishedTimer()
{
	if (!IsValid(GetWorld()))
	{
		UE_LOG(ConvaiAudioStreamerLog, Warning, TEXT("ClearAudioFinishedTimer: GetWorld() is Invalid!"));
		return;
	}
	GetWorld()->GetTimerManager().ClearTimer(AudioFinishedTimerHandle);
}

// Not used
bool UConvaiAudioStreamer::IsLocal()
{
	const ENetMode NetMode = GetNetMode();

	if (NetMode == NM_Standalone)
	{
		// Not networked.
		return true;
	}

	if (NetMode == NM_Client && GetOwner()->GetLocalRole() == ROLE_AutonomousProxy)
	{
		// Networked client in control.
		return true;
	}

	if (GetOwner()->GetRemoteRole() != ROLE_AutonomousProxy && GetOwner()->GetLocalRole() == ROLE_Authority)
	{
		// Local authority in control.
		return true;
	}
	return false;
}

IConvaiLipSyncInterface* UConvaiAudioStreamer::FindFirstLipSyncComponent()
{
	// Find the LipSync component
	auto LipSyncComponents = (GetOwner()->GetComponentsByInterface(UConvaiLipSyncInterface::StaticClass()));
	if (LipSyncComponents.Num())
	{
		SetLipSyncComponent(LipSyncComponents[0]);
	}
	return ConvaiLipSync;
}

bool UConvaiAudioStreamer::SetLipSyncComponent(UActorComponent* LipSyncComponent)
{
	if (!CanUseLipSync())
		return false;

	// Find the LipSync component
	if (LipSyncComponent && LipSyncComponent->GetClass()->ImplementsInterface(UConvaiLipSyncInterface::StaticClass()))
	{
		ConvaiLipSync = Cast<IConvaiLipSyncInterface>(LipSyncComponent);
		ConvaiLipSyncExtended = Cast<IConvaiLipSyncExtendedInterface>(LipSyncComponent);
		ConvaiLipSync->OnVisemesDataReady.BindUObject(this, &UConvaiAudioStreamer::OnVisemesReadyCallback);
		return true;
	}
	else
	{
		ConvaiLipSync = nullptr;
		ConvaiLipSyncExtended = nullptr;
		return false;
	}
}

bool UConvaiAudioStreamer::SupportsLipSync()
{
	if (!CanUseLipSync())
		return false;

	if (ConvaiLipSync == nullptr)
	{
		FindFirstLipSyncComponent();
	}
	return ConvaiLipSync != nullptr;
}

IConvaiVisionInterface* UConvaiAudioStreamer::FindFirstVisionComponent()
{
	// Find the Vision component
	auto VisionComponents = (GetOwner()->GetComponentsByInterface(UConvaiVisionInterface::StaticClass()));
	if (VisionComponents.Num())
	{
		SetVisionComponent(VisionComponents[0]);
	}
	return ConvaiVision;
}

bool UConvaiAudioStreamer::SetVisionComponent(UActorComponent* VisionComponent)
{
	if (!CanUseVision())
		return false;

	// Find the Vision component
	if (VisionComponent && VisionComponent->GetClass()->ImplementsInterface(UConvaiVisionInterface::StaticClass()))
	{
		ConvaiVision = Cast<IConvaiVisionInterface>(VisionComponent);
		return true;
	}
	else
	{
		ConvaiVision = nullptr;
		return false;
	}
}

bool UConvaiAudioStreamer::SupportsVision()
{
	if (!CanUseVision())
		return false;

	if (ConvaiVision == nullptr)
	{
		FindFirstVisionComponent();
	}
	return ConvaiVision != nullptr;
}

void UConvaiAudioStreamer::BeginPlay()
{
	Super::BeginPlay();
	// Initialize the audio component
	bAutoActivate = true;
	bAlwaysPlay = true;

	SoundWaveProcedural = NewObject<USoundWaveProcedural>();

	if (ConvaiLipSync == nullptr)
		FindFirstLipSyncComponent();

	if (ConvaiVision == nullptr)
		FindFirstVisionComponent();
}

void UConvaiAudioStreamer::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateVoiceFade(DeltaTime);

	int32 BytesPerFrame = EncoderFrameSize * EncoderNumChannels * sizeof(opus_int16);
	if (AudioDataBuffer.Num() >= BytesPerFrame && Encoder)
	{
		// Buffer to hold encoded data
		uint8* CurrentEncodedAudioDataPtr = (uint8*)malloc(AudioDataBuffer.Num());
		uint32 CurrentEncodedAudioDataSize = AudioDataBuffer.Num();

		// Encode the Audio data
		uint32 remainder = Encode(AudioDataBuffer.GetData(), AudioDataBuffer.Num(), CurrentEncodedAudioDataPtr, CurrentEncodedAudioDataSize);

		// Remove encoded data
		uint32 SizeOfDataBeforeEncoding = AudioDataBuffer.Num() - remainder;
		AudioDataBuffer.RemoveAt(0, SizeOfDataBeforeEncoding);

		// Send the encoded data over the network
		ProcessEncodedVoiceData(TArray<uint8>(CurrentEncodedAudioDataPtr, CurrentEncodedAudioDataSize), EncoderSampleRate, EncoderNumChannels, SizeOfDataBeforeEncoding);
		
		// Free the Encoded data buffer
		free(CurrentEncodedAudioDataPtr);

		//UE_LOG(ConvaiAudioStreamerLog, Log, TEXT("Encoder Received %d bytes and Outputted %d bytes"), SizeOfDataBeforeEncoding, CurrentEncodedAudioDataSize);
		//UE_LOG(ConvaiAudioStreamerLog, Log, TEXT("Remaining %d bytes"), AudioDataBuffer.Num());
	}
}

void UConvaiAudioStreamer::BeginDestroy()
{
	DestroyOpus();
	Super::BeginDestroy();
}

void UConvaiAudioStreamer::DestroyOpus()
{
	DestroyOpusEncoder();
	DestroyOpusDecoder();

}

void UConvaiAudioStreamer::PlayLipSyncWithPreGeneratedDataSynced(FAnimationSequence& FaceSequence)
{
	// Play the lipsync right away as a workaround for lipsync issue on multiplayer
	if (!SupportsLipSync() || ConvaiLipSyncExtended == nullptr || !ConvaiLipSyncExtended->RequiresPreGeneratedFaceData() || ReplicateVoiceToNetwork)
		PlayLipSyncWithPreGeneratedData(FaceSequence);

	CurrentChunkFrameCounter++;

	if (FaceSequence.AnimationFrames.Num() == 0)
		return;


	int32 CurrentFrameIndex = FaceSequence.AnimationFrames[0].FrameIndex;

	if (DetectNewLipSyncChunk(CurrentChunkFrameCounter, CurrentChunkDuration, CurrentChunkLipSyncFrameRate) && DataBuffer.NumAudioChunks > DataBuffer.NumLipSyncChunks)
	{
		DataBuffer.EnqueueLipSync(FaceSequence, true);
		int CurrentLipSyncChunkIndex = DataBuffer.NumLipSyncChunks - 1;

		CurrentChunkDuration = DataBuffer.ChunkDurations[CurrentLipSyncChunkIndex];
		CurrentChunkLipSyncFrameRate = FaceSequence.FrameRate;
		UE_LOG(ConvaiAudioStreamerLog, Log, TEXT("PlayLipSyncWithPreGeneratedDataSynced: Detected New LipSync Chunk ChunkDuration: %f ChunkLipSyncFrameRate: %f FrameIndex:%d ChunkFrameCounter: %d ExpectedFrameCount:%f ChunkFrameCounter: %d"), CurrentChunkDuration, CurrentChunkLipSyncFrameRate, CurrentFrameIndex, CurrentChunkFrameCounter, CurrentChunkLipSyncFrameRate * CurrentChunkDuration, CurrentChunkFrameCounter);
		CurrentChunkFrameCounter = 1;
		return;
	}
	else if (DetectNewLipSyncChunk(CurrentChunkFrameCounter, CurrentChunkDuration, CurrentChunkLipSyncFrameRate) && DataBuffer.NumAudioChunks <= DataBuffer.NumLipSyncChunks)
	{
		UE_LOG(ConvaiAudioStreamerLog, Log, TEXT("PlayLipSyncWithPreGeneratedDataSynced: Failed to detect New LipSync Chunk due to insufficent audio chunks NumAudioChunks: %d NumLipSyncChunks: %d FrameIndex:%d ChunkFrameCounter: %d ExpectedFrameCount:%f ChunkFrameCounter: %d"), DataBuffer.NumAudioChunks, DataBuffer.NumLipSyncChunks, CurrentFrameIndex, CurrentChunkFrameCounter, CurrentChunkLipSyncFrameRate * CurrentChunkDuration, CurrentChunkFrameCounter);
	}


	if (IsTalking && DataBuffer.NumLipSyncChunks > 0)
	{
		DataBuffer.EnqueueLipSync(FaceSequence, false);
		PlayAvailableAudioAndLipSync();
	}
	else if (IsTalking && DataBuffer.NumLipSyncChunks == 0)
	{
		PlayLipSyncWithPreGeneratedData(FaceSequence);
	}
	else if (bIsPaused || !IsTalking)
	{
		if (DataBuffer.NumLipSyncChunks == 0)
		{
			UE_LOG(ConvaiAudioStreamerLog, Log, TEXT("PlayLipSyncWithPreGeneratedDataSynced: Detected LipSync Frame coming late - ChunkDuration: %f ChunkLipSyncFrameRate: %f FrameIndex:%d ChunkFrameCounter: %d ExpectedFrameCount:%f ChunkFrameCounter: %d"), CurrentChunkDuration, CurrentChunkLipSyncFrameRate, CurrentFrameIndex, CurrentChunkFrameCounter, CurrentChunkLipSyncFrameRate * CurrentChunkDuration, CurrentChunkFrameCounter);
			return; // Lipsync came late - skip it
		}

		if (DataBuffer.IsEmptyLipSync())
		{
			UE_LOG(ConvaiAudioStreamerLog, Log, TEXT("PlayLipSyncWithPreGeneratedDataSynced: Trying to Enqueue while DataBuffer.IsEmptyLipSync() - IsTalking = %s - bIsPaused = %s"), IsTalking ? TEXT("true") : TEXT("false"), bIsPaused ? TEXT("true") : TEXT("false"));
		}

		DataBuffer.EnqueueLipSync(FaceSequence, false);

		if (HasSufficentLipsyncFrames())
		{
			UE_LOG(ConvaiAudioStreamerLog, Log, TEXT("PlayLipSyncWithPreGeneratedDataSynced: Resuming Voice and Lipsync"));
			PlayNextAudioInQueue();
			PlayNextLipSyncInQueue();
			ResumeVoice();
		}
	}
}

void UConvaiAudioStreamer::PlayLipSyncWithPreGeneratedData(FAnimationSequence FaceSequence)
{
	if (ConvaiLipSync)
	{
		if (ConvaiLipSyncExtended && ConvaiLipSyncExtended->RequiresPreGeneratedFaceData())
		{
			ConvaiLipSyncExtended->ConvaiProcessLipSyncAdvanced(nullptr, 0, 0, 0, FaceSequence);
		}
	}
}

void UConvaiAudioStreamer::PlayLipSync(uint8* InPCMData, uint32 InPCMDataSize, uint32 InSampleRate, uint32 InNumChannels)
{
	if (ConvaiLipSync)
	{
		ConvaiLipSync->ConvaiProcessLipSync(InPCMData, InPCMDataSize, InSampleRate, InNumChannels);
	}
}

void UConvaiAudioStreamer::StopLipSync()
{
	if (ConvaiLipSync)
	{
		ConvaiLipSync->ConvaiStopLipSync();
		ConvaiLipSync->OnVisemesDataReady.ExecuteIfBound(); // TODO (Mohamed): This is redundant and should be removed once all users update their OVR lipsync plugin
	}
}

void UConvaiAudioStreamer::PauseLipSync()
{
	if (ConvaiLipSync)
	{
		if (ConvaiLipSyncExtended)
		{
			//ConvaiLipSyncExtended->ConvaiPauseLipSync();
		}
	}
}

void UConvaiAudioStreamer::ResumeLipSync()
{
	if (ConvaiLipSync)
	{
		if (ConvaiLipSyncExtended)
		{
			//ConvaiLipSyncExtended->ConvaiResumeLipSync();
		}
	}
}

bool UConvaiAudioStreamer::CanUseLipSync()
{
	return false;
}

bool UConvaiAudioStreamer::CanUseVision()
{
	return false;
}

void UConvaiAudioStreamer::OnVisemesReadyCallback()
{
	OnVisemesReady.Broadcast();
}

bool UConvaiAudioStreamer::DetectNewLipSyncChunk(int32 InFrameCount, float InChunkDuration, int32 FrameRate)
{
	if (InChunkDuration <= 0 || FrameRate <= 0)
		return true;

	int32 ExpectedNumFrames = InChunkDuration * FrameRate;
	return InFrameCount > ExpectedNumFrames;
}

void UConvaiAudioStreamer::OnLipSyncTimeOut()
{
}

const TArray<float> UConvaiAudioStreamer::GetVisemes() const
{
	if (ConvaiLipSync)
	{
		return ConvaiLipSync->ConvaiGetVisemes();
	}
	return TArray<float>();
}

const TArray<FString> UConvaiAudioStreamer::GetVisemeNames() const
{
	if (ConvaiLipSync)
	{
		return ConvaiLipSync->ConvaiGetVisemeNames();
	}
	return TArray<FString>();
}

const TMap<FName, float> UConvaiAudioStreamer::ConvaiGetFaceBlendshapes() const
{
	if (ConvaiLipSyncExtended)
	{
		return ConvaiLipSyncExtended->ConvaiGetFaceBlendshapes();
	}
	return TMap<FName, float>();
}

bool UConvaiAudioStreamer::GeneratesVisemesAsBlendshapes()
{
	if (ConvaiLipSyncExtended)
	{
		return ConvaiLipSyncExtended->GeneratesVisemesAsBlendshapes();
	}
	return false;
}

void UConvaiAudioStreamer::AddFaceDataToSend(FAnimationSequence FaceSequence)
{
	PlayLipSyncWithPreGeneratedDataSynced(FaceSequence);
}

void UConvaiAudioStreamer::AddPCMDataToSend(TArray<uint8> PCMDataToAdd,
											bool ContainsHeaderData,
                                            uint32 InSampleRate,
                                            uint32 InNumChannels) {
	if (ContainsHeaderData)
	{
		// Parse Wav header
		FWaveModInfo WaveInfo;
		FString ErrorReason;
		bool ParseSuccess = WaveInfo.ReadWaveInfo(PCMDataToAdd.GetData(), PCMDataToAdd.Num(), &ErrorReason);
		// Set the number of channels and sample rate for the first time reading from the stream
		if (ParseSuccess)
		{
			InSampleRate = *WaveInfo.pSamplesPerSec;
			InNumChannels = *WaveInfo.pChannels;
			PCMDataToAdd.RemoveAt(0, 44); // Remove the header bytes 
		}
		else if (!ParseSuccess)
		{
			UE_LOG(ConvaiAudioStreamerLog, Warning, TEXT("AddPCMDataToSend: Failed to parse wav header, reason: %s"), *ErrorReason);
		}
	}

	InNumChannels = FMath::Max((int)InNumChannels, 1);

	TArray<int16> OutConverted;
	
	if (ReplicateVoiceToNetwork && (InNumChannels > 1 || InSampleRate > 24000))
	{
		UConvaiUtils::ResampleAudio(InSampleRate, 24000, InNumChannels, true, (int16*)PCMDataToAdd.GetData(), PCMDataToAdd.Num()/2, OutConverted);
		InSampleRate = 24000;
		InNumChannels = 1;
	}
	else
	{
		OutConverted = TArray<int16>((int16*)PCMDataToAdd.GetData(), PCMDataToAdd.Num()/2);
	}

	// Send it over to the encoder if we are to stream the voice audio to other clients
	if (ReplicateVoiceToNetwork)
	{
		// Check that encoder is valid and able to encode the input sample rate and channels
		if (InSampleRate != EncoderSampleRate || InNumChannels != EncoderNumChannels)
		{
			AudioDataBuffer.Reset();
			DestroyOpusEncoder();
			InitEncoder(InSampleRate, InNumChannels, EAudioEncodeHint::VoiceEncode_Voice);
			UE_LOG(ConvaiAudioStreamerLog, Log, TEXT("Initialized Encoder with SampleRate:%d and Channels:%d"), EncoderSampleRate, EncoderNumChannels);
		}
		AudioDataBuffer.Append((uint8*)OutConverted.GetData(), OutConverted.Num()*2);
	}
	else if (!ShouldMuteLocal())
	{
		// Just play it locally
		PlayVoiceSynced((uint8*)OutConverted.GetData(), OutConverted.Num()*2, false, InSampleRate, InNumChannels);
	}
}

void UConvaiAudioStreamer::onAudioStarted()
{
	AsyncTask(ENamedThreads::GameThread, [this] {
		OnStartedTalking.Broadcast();
		});
	
}

void UConvaiAudioStreamer::onAudioFinished()
{
	UE_LOG(ConvaiAudioStreamerLog, Log, TEXT("onAudioFinished"));

	if (!DataBuffer.IsEmpty())
	{
		//AsyncTask(ENamedThreads::GameThread, [this] {
			if (HasSufficentLipsyncFrames()) // returns true if the lipsync component is not available
			{
				UE_LOG(ConvaiAudioStreamerLog, Log, TEXT("onAudioFinished: Resuming Voice and Lipsync"));
				IsTalking = true; // put IsTalking to true to prevent triggering of the OnStartedTalking Trigger
				PlayAvailableAudioAndLipSync();
				//PlayNextAudioInQueue();
				//PlayNextLipSyncInQueue();
				ResumeVoice();

				return;
			}
			else
			{
				UE_LOG(ConvaiAudioStreamerLog, Log, TEXT("onAudioFinished: Pausing Voice and Lipsync"));
				PauseVoice();
				StopLipSync();
			}
		//});
	}

	AsyncTask(ENamedThreads::GameThread, [this] {
		OnFinishedTalking.Broadcast();
	});
	IsTalking = false;
}

bool UConvaiAudioStreamer::PlayNextAudioInQueue()
{
	if (DataBuffer.IsEmpty())
		return false;
	ConvaiAudioChunk NextAudioChunk;
	DataBuffer.Dequeue(NextAudioChunk);
	PlayVoiceData(NextAudioChunk.AudioData.GetData(), NextAudioChunk.AudioData.Num(), false, NextAudioChunk.SampleRate, NextAudioChunk.NumChannels);
	UE_LOG(ConvaiAudioStreamerLog, Log, TEXT("PlayNextAudioInQueue - Duration: %f - Chunks Remaining: %d"), NextAudioChunk.AudioDuration, DataBuffer.NumAudioChunks);
	return true;
}

bool UConvaiAudioStreamer::PlayNextLipSyncInQueue()
{
	if (DataBuffer.IsEmptyLipSync())
		return false;
	FAnimationSequence NextLipSyncChunk;
	DataBuffer.DequeueLipSync(NextLipSyncChunk);
	PlayLipSyncWithPreGeneratedData(NextLipSyncChunk);
	UE_LOG(ConvaiAudioStreamerLog, Log, TEXT("PlayNextLipSyncInQueue - Duration: %f - Chunks Remaining: %d"), NextLipSyncChunk.Duration, DataBuffer.NumLipSyncChunks);
	return true;
}

bool UConvaiAudioStreamer::PlayAvailableAudioAndLipSync()
{
	if (DataBuffer.IsEmpty() || DataBuffer.IsEmptyLipSync())
		return false;

	TArray<uint8> MergedVoiceData;
	uint32 NumChannels;
	uint32 SampleRate;
	float AudioDuration = 0;
	int AudioChunks = 0;

	FAnimationSequence MergedLipSyncData;
	int LipSyncChunks = 0;

	while (HasSufficentLipsyncFrames() && !DataBuffer.IsEmpty())
	{

		ConvaiAudioChunk NextAudioChunk;
		if (DataBuffer.Dequeue(NextAudioChunk))
		{
			MergedVoiceData.Append(NextAudioChunk.AudioData);
			NumChannels = NextAudioChunk.NumChannels;
			SampleRate = NextAudioChunk.SampleRate;
			AudioDuration += NextAudioChunk.AudioDuration;
			AudioChunks++;
		}
		FAnimationSequence NextLipSyncChunk;
		if (DataBuffer.DequeueLipSync(NextLipSyncChunk))
		{
			MergedLipSyncData.FrameRate = NextLipSyncChunk.FrameRate;
			MergedLipSyncData.Duration += NextLipSyncChunk.Duration;
			MergedLipSyncData.AnimationFrames.Append(NextLipSyncChunk.AnimationFrames);
			LipSyncChunks++;
		}
	}

	if (MergedVoiceData.Num() == 0 && MergedLipSyncData.AnimationFrames.Num() == 0)
	{
		return false;
	}
	else
	{
		PlayVoiceData(MergedVoiceData.GetData(), MergedVoiceData.Num(), false, SampleRate, NumChannels);
		PlayLipSyncWithPreGeneratedData(MergedLipSyncData);
		UE_LOG(ConvaiAudioStreamerLog, Log, TEXT("Play Available Audio and LipSync - Audio Duration: %f - Audio Chunks: %d - LipSync Duration: %f - LipSync Chunks: %d - Audio Chunks Remaining: %d - LipSync Chunks Remaining: %d"), AudioDuration, AudioChunks, MergedLipSyncData.Duration, LipSyncChunks, DataBuffer.NumAudioChunks, DataBuffer.NumLipSyncChunks);
		return true;
	}
}

bool UConvaiAudioStreamer::HasSufficentLipsyncFrames()
{
	float Dummy;
	return HasSufficentLipsyncFrames(Dummy);
}

bool UConvaiAudioStreamer::HasSufficentLipsyncFrames(float& InSyncTimeRemaining)
{
	//float NextChunkAudioTime = 0;
	//if (auto Chunk = DataBuffer.Peek())
	//{
	//	NextChunkAudioTime = Chunk->AudioDuration;
	//}

	float RemainingVoiceTime = DataBuffer.TotalBufferedAudioDuration;

	if (ConvaiLipSync)
	{
		if (ConvaiLipSyncExtended)
		{
			float RemainingLipSyncTime = DataBuffer.TotalBufferedLipSyncDuration;

			//UE_LOG(ConvaiAudioStreamerLog, Log, TEXT("HasSufficentLipsyncFrames: TotalBufferedAudioDuration:%f TotalBufferedLipSyncDuration:%f, RemainingVoiceTime:%f, NumAudioChunks: %d, NumLipSyncChunks: %d"), DataBuffer.TotalBufferedAudioDuration, DataBuffer.TotalBufferedLipSyncDuration, RemainingVoiceTime, DataBuffer.NumAudioChunks, DataBuffer.NumLipSyncChunks);

			if (LipSyncThresholdSecs < 0)
			{
				LipSyncThresholdSecs = UConvaiSettingsUtils::GetParamValueAsFloat("LipSyncThresholdSecs", LipSyncThresholdSecs) ? LipSyncThresholdSecs : 0.7f;
				LipSyncThresholdSecs = LipSyncThresholdSecs < 0 ? 0 : LipSyncThresholdSecs;
			}
			if (VoiceTimeFactor < 0)
			{
				VoiceTimeFactor = UConvaiSettingsUtils::GetParamValueAsFloat("VoiceTimeFactor", VoiceTimeFactor) ? VoiceTimeFactor : 0.5f;
				VoiceTimeFactor = VoiceTimeFactor < 0 ? 0 : VoiceTimeFactor;
			}


			if (RemainingLipSyncTime > LipSyncThresholdSecs || (RemainingLipSyncTime >= RemainingVoiceTime * VoiceTimeFactor))
			{
				if (DataBuffer.LastLipSyncChunkDuration > LipSyncThresholdSecs || DataBuffer.LastLipSyncChunkDuration >= DataBuffer.LastAudioChunkDuration * VoiceTimeFactor)
				{
					InSyncTimeRemaining = DataBuffer.TotalBufferedLipSyncDuration;
				}
				else
				{
					InSyncTimeRemaining = DataBuffer.TotalBufferedLipSyncDuration - DataBuffer.LastLipSyncChunkDuration;
				}
				return true;
			}
			else
			{
				InSyncTimeRemaining = 0;
				return false;
			}
		}
	}

	InSyncTimeRemaining = RemainingVoiceTime;
	return true;
}


bool UConvaiAudioStreamer::InitEncoder(int32 InSampleRate, int32 InNumChannels, EAudioEncodeHint EncodeHint)
{
	if (InSampleRate != 8000 &&
		InSampleRate != 12000 &&
		InSampleRate != 16000 &&
		InSampleRate != 24000 &&
		InSampleRate != 48000)
	{
		UE_LOG(ConvaiAudioStreamerLog, Warning, TEXT("Voice encoder doesn't support %d hz"), InSampleRate);
		return false;
	}

	if (InNumChannels < 1 || InNumChannels > 2)
	{
		UE_LOG(ConvaiAudioStreamerLog, Warning, TEXT("Voice encoder only supports 1 or 2 channels"));
		return false;
	}

	EncoderSampleRate = InSampleRate;
	EncoderNumChannels = InNumChannels;

	// 20ms frame sizes are a good choice for most applications (1000ms / 20ms = 50)
	EncoderFrameSize = EncoderSampleRate / NUM_OPUS_FRAMES_PER_SEC;
	//MaxFrameSize = FrameSize * MAX_OPUS_FRAMES;

	int32 EncError = 0;

	const int32 Application = (EncodeHint == EAudioEncodeHint::VoiceEncode_Audio) ? OPUS_APPLICATION_AUDIO : OPUS_APPLICATION_VOIP;

	const int32 EncSize = opus_encoder_get_size(EncoderNumChannels);
	Encoder = (OpusEncoder*)FMemory::Malloc(EncSize);
	EncError = opus_encoder_init(Encoder, EncoderSampleRate, EncoderNumChannels, Application);


	if (EncError == OPUS_OK)
	{
		// Turn on variable bit rate encoding
		const int32 UseVbr = 1;
		opus_encoder_ctl(Encoder, OPUS_SET_VBR(UseVbr));

		// Turn off constrained VBR
		const int32 UseCVbr = 0;
		opus_encoder_ctl(Encoder, OPUS_SET_VBR_CONSTRAINT(UseCVbr));

		// Complexity (1-10)
		const int32 Complexity = 1;
		opus_encoder_ctl(Encoder, OPUS_SET_COMPLEXITY(Complexity));

		// Forward error correction
		const int32 InbandFEC = 0;
		opus_encoder_ctl(Encoder, OPUS_SET_INBAND_FEC(InbandFEC));
	}
	else
	{
		UE_LOG(ConvaiAudioStreamerLog, Warning, TEXT("Failed to init Opus Encoder: %s"), ANSI_TO_TCHAR(opus_strerror(EncError)));
		DestroyOpus();
	}

	return EncError == OPUS_OK;
}

int32 UConvaiAudioStreamer::Encode(const uint8* RawPCMData, uint32 RawDataSize, uint8* OutCompressedData, uint32& OutCompressedDataSize)
{
	check(Encoder);

	int32 HeaderSize = 0;
	const int32 BytesPerFrame = EncoderFrameSize * EncoderNumChannels * sizeof(opus_int16);
	const int32 MaxFramesEncoded = MAX_OPUS_UNCOMPRESSED_BUFFER_SIZE / BytesPerFrame;

	// total bytes / bytes per frame
	const int32 NumFramesToEncode = FMath::Min((int32)RawDataSize / BytesPerFrame, MaxFramesEncoded);
	const int32 DataRemainder = RawDataSize - NumFramesToEncode * BytesPerFrame;
	const int32 RawDataStride = BytesPerFrame;

	//UE_LOG(ConvaiAudioStreamerLog, Log, TEXT("NumFramesToEncode %d frames"), NumFramesToEncode);


	if (NumFramesToEncode == 0)
	{
		// We can avoid saving out an empty header if we know we're not going to send anything
		check(DataRemainder == RawDataSize);
		OutCompressedDataSize = 0;
		return DataRemainder;
	}

	// Store the number of frames to be encoded
	check(NumFramesToEncode < MAX_uint8);
	OutCompressedData[0] = (uint8)NumFramesToEncode;
	OutCompressedData[1] = EncoderGeneration;
	HeaderSize += 2 * sizeof(uint8);

	// Store the offset to each encoded frame
	uint16* CompressedOffsets = (uint16*)(OutCompressedData + HeaderSize);
	const uint32 LengthOfCompressedOffsets = NumFramesToEncode * sizeof(uint16);
	HeaderSize += LengthOfCompressedOffsets;

	// Space available after overhead
	int32 AvailableBufferSize = OutCompressedDataSize - HeaderSize;

	// Start of the actual compressed data
	uint8* CompressedDataStart = OutCompressedData + HeaderSize;
	int32 CompressedBufferOffset = 0;
	for (int32 i = 0; i < NumFramesToEncode; i++)
	{
		int32 CompressedLength = opus_encode(Encoder, (const opus_int16*)(RawPCMData + (i * RawDataStride)), EncoderFrameSize, CompressedDataStart + CompressedBufferOffset, AvailableBufferSize);
		if (CompressedLength < 0)
		{
			const char* ErrorStr = opus_strerror(CompressedLength);
			UE_LOG(ConvaiAudioStreamerLog, Warning, TEXT("Failed to encode: [%d] %s"), CompressedLength, ANSI_TO_TCHAR(ErrorStr));

			// Mark header as nothing encoded
			OutCompressedData[0] = 0;
			OutCompressedDataSize = 0;
			return 0;
		}
		else if (CompressedLength != 1)
		{
			AvailableBufferSize -= CompressedLength;
			CompressedBufferOffset += CompressedLength;

			check(CompressedBufferOffset < MAX_uint16);
			CompressedOffsets[i] = (uint16)CompressedBufferOffset;
		}
		else
		{
			UE_LOG(ConvaiAudioStreamerLog, Warning, TEXT("Nothing to encode!"));
			CompressedOffsets[i] = 0;
		}
	}

	// End of buffer
	OutCompressedDataSize = HeaderSize + CompressedBufferOffset;

	UE_LOG(ConvaiAudioStreamerLog, Verbose, TEXT("OpusEncode[%d]: RawSize: %d HeaderSize: %d CompressedSize: %d NumFramesEncoded: %d Remains: %d"), EncoderGeneration, RawDataSize, HeaderSize, OutCompressedDataSize, NumFramesToEncode, DataRemainder);

	EncoderGeneration = (EncoderGeneration + 1) % MAX_uint8;
	return DataRemainder;
}

void UConvaiAudioStreamer::DestroyOpusEncoder()
{
	if (Encoder)
	{
		FMemory::Free(Encoder);
		Encoder = nullptr;
	}
}

bool UConvaiAudioStreamer::InitDecoder(int32 InSampleRate, int32 InNumChannels)
{
	UE_LOG(ConvaiAudioStreamerLog, Display, TEXT("DecoderVersion: %s"), ANSI_TO_TCHAR(opus_get_version_string()));

	if (InSampleRate != 8000 &&
		InSampleRate != 12000 &&
		InSampleRate != 16000 &&
		InSampleRate != 24000 &&
		InSampleRate != 48000)
	{
		UE_LOG(ConvaiAudioStreamerLog, Warning, TEXT("Voice decoder doesn't support %d hz"), InSampleRate);
		return false;
	}

	if (InNumChannels < 1 || InNumChannels > 2)
	{
		UE_LOG(ConvaiAudioStreamerLog, Warning, TEXT("Voice decoder only supports 1 or 2 channels"));
		return false;
	}

	DecoderSampleRate = InSampleRate;
	DecoderNumChannels = InNumChannels;

	// 20ms frame sizes are a good choice for most applications (1000ms / 20ms = 50)
	DecoderFrameSize = DecoderSampleRate / NUM_OPUS_FRAMES_PER_SEC;

	int32 DecError = 0;

	const int32 DecSize = opus_decoder_get_size(DecoderNumChannels);
	Decoder = (OpusDecoder*)FMemory::Malloc(DecSize);
	DecError = opus_decoder_init(Decoder, DecoderSampleRate, DecoderNumChannels);

	if (DecError == OPUS_OK)
	{

	}
	else
	{
		UE_LOG(ConvaiAudioStreamerLog, Warning, TEXT("Failed to init Opus Decoder: %s"), ANSI_TO_TCHAR(opus_strerror(DecError)));
		DestroyOpus();
	}

	return DecError == OPUS_OK;
}

inline bool SanityCheckHeader(uint32 HeaderSize, uint32 CompressedDataSize, int32 NumFramesToDecode, const uint16* CompressedOffsets)
{
	bool bHeaderDataOk = (HeaderSize <= CompressedDataSize);
	if (bHeaderDataOk)
	{
		// Validate that the sum of the encoded data sizes fit under the given amount of compressed data
		uint16 LastCompressedOffset = 0;
		int32 TotalCompressedBufferSize = 0;
		for (int32 Idx = 0; Idx < NumFramesToDecode; Idx++)
		{
			// Offsets should be monotonically increasing (prevent later values intentionally reducing bad previous values)
			if (CompressedOffsets[Idx] >= LastCompressedOffset)
			{
				TotalCompressedBufferSize += (CompressedOffsets[Idx] - LastCompressedOffset);
				LastCompressedOffset = CompressedOffsets[Idx];
			}
			else
			{
				bHeaderDataOk = false;
				break;
			}
		}

		bHeaderDataOk = bHeaderDataOk && ((HeaderSize + TotalCompressedBufferSize) <= CompressedDataSize);
	}

	return bHeaderDataOk;
}

void UConvaiAudioStreamer::Decode(const uint8* InCompressedData, uint32 CompressedDataSize, uint8* OutRawPCMData, uint32& OutRawDataSize)
{
	uint32 HeaderSize = (2 * sizeof(uint8));
	if (!InCompressedData || (CompressedDataSize < HeaderSize))
	{
		OutRawDataSize = 0;
		return;
	}

	const int32 BytesPerFrame = DecoderFrameSize * DecoderNumChannels * sizeof(opus_int16);
	const int32 MaxFramesEncoded = MAX_OPUS_UNCOMPRESSED_BUFFER_SIZE / BytesPerFrame;

	const int32 NumFramesToDecode = InCompressedData[0];
	const int32 PacketGeneration = InCompressedData[1];

	//UE_LOG(ConvaiAudioStreamerLog, Log, TEXT("NumFramesToDecode %d frames"), NumFramesToDecode);

	if (PacketGeneration != DecoderLastGeneration + 1)
	{
		UE_LOG(ConvaiAudioStreamerLog, Warning, TEXT("Packet generation skipped from %d to %d"), DecoderLastGeneration, PacketGeneration);
	}

	if ((NumFramesToDecode > 0) && (NumFramesToDecode <= MaxFramesEncoded))
	{
		// Start of compressed data offsets
		const uint16* CompressedOffsets = (const uint16*)(InCompressedData + HeaderSize);
		uint32 LengthOfCompressedOffsets = NumFramesToDecode * sizeof(uint16);
		HeaderSize += LengthOfCompressedOffsets;

		// At this point we have all our pointer fix up complete, but the data it references may be invalid in corrupt/spoofed packets
		// Sanity check the numbers to make sure everything works out
		if (SanityCheckHeader(HeaderSize, CompressedDataSize, NumFramesToDecode, CompressedOffsets))
		{
			// Start of compressed data
			const uint8* CompressedDataStart = (InCompressedData + HeaderSize);

			int32 CompressedBufferOffset = 0;
			int32 DecompressedBufferOffset = 0;
			uint16 LastCompressedOffset = 0;

			for (int32 i = 0; i < NumFramesToDecode; i++)
			{
				const int32 UncompressedBufferAvail = (OutRawDataSize - DecompressedBufferOffset);

				if (UncompressedBufferAvail >= (MAX_OPUS_FRAMES * BytesPerFrame))
				{
					if (CompressedOffsets[i] > 0)
					{
						const int32 CompressedBufferSize = (CompressedOffsets[i] - LastCompressedOffset);

						check(Decoder);
						const int32 NumDecompressedSamples = opus_decode(Decoder,
							CompressedDataStart + CompressedBufferOffset, CompressedBufferSize,
							(opus_int16*)(OutRawPCMData + DecompressedBufferOffset), MAX_OPUS_FRAME_SIZE, 0);

						if (NumDecompressedSamples < 0)
						{
							const char* ErrorStr = opus_strerror(NumDecompressedSamples);
							UE_LOG(ConvaiAudioStreamerLog, Warning, TEXT("Failed to decode: [%d] %s"), NumDecompressedSamples, ANSI_TO_TCHAR(ErrorStr));
						}
						else
						{
							if (NumDecompressedSamples != DecoderFrameSize)
							{
								UE_LOG(ConvaiAudioStreamerLog, Warning, TEXT("Unexpected decode result NumSamplesDecoded %d != FrameSize %d"), NumDecompressedSamples, DecoderFrameSize);
							}

							// Advance within the decompressed output stream
							DecompressedBufferOffset += (NumDecompressedSamples * DecoderNumChannels * sizeof(opus_int16));
						}

						// Advance within the compressed input stream
						CompressedBufferOffset += CompressedBufferSize;
						LastCompressedOffset = CompressedOffsets[i];
					}
					else
					{
						UE_LOG(ConvaiAudioStreamerLog, Warning, TEXT("Decompression buffer skipped a frame"));
						// Nothing to advance within the compressed input stream
					}
				}
				else
				{
					UE_LOG(ConvaiAudioStreamerLog, Warning, TEXT("Decompression buffer too small to decode voice"));
					break;
				}
			}

			OutRawDataSize = DecompressedBufferOffset;
		}
		else
		{
			UE_LOG(ConvaiAudioStreamerLog, Warning, TEXT("Failed to decode: header corrupted"));
			OutRawDataSize = 0;
		}
	}
	else
	{
		UE_LOG(ConvaiAudioStreamerLog, Warning, TEXT("Failed to decode: buffer corrupted"));
		OutRawDataSize = 0;
	}

	UE_LOG(ConvaiAudioStreamerLog, Verbose, TEXT("OpusDecode[%d]: RawSize: %d HeaderSize: %d CompressedSize: %d NumFramesDecoded: %d "), PacketGeneration, OutRawDataSize, HeaderSize, CompressedDataSize, NumFramesToDecode);

	DecoderLastGeneration = PacketGeneration;
}

void UConvaiAudioStreamer::DestroyOpusDecoder()
{
	if (Decoder)
	{
		FMemory::Free(Decoder);
		Decoder = nullptr;
	}
}
