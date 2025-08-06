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
		CONVAI_LOG(Category, Warning, TEXT("Failure to get CTL %s"), #CTL); \
	}


DEFINE_LOG_CATEGORY(ConvaiAudioStreamerLog);

UConvaiAudioStreamer::UConvaiAudioStreamer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	bAutoActivate = true;

	// Pre-allocate the temporary buffer
	TempAudioBuffer.Reserve(TempBufferSize);

	// Initialize the audio buffer with a reasonable size
	AudioBuffer.Init(TempBufferSize);
}

void UConvaiAudioStreamer::BroadcastVoiceDataToClients_Implementation(TArray<uint8> const& EncodedVoiceData, uint32 SampleRate, uint32 NumChannels, uint32 SizeBeforeEncode)
{
	// Check that decoder is valid and able to decode the input sample rate and channels
	if (!Decoder || SampleRate != DecoderSampleRate || NumChannels != DecoderNumChannels)
	{
		DestroyOpusDecoder();
		InitDecoder(SampleRate, NumChannels);
		CONVAI_LOG(ConvaiAudioStreamerLog, Log, TEXT("Initialized Decoder with SampleRate:%d and Channels:%d"), DecoderSampleRate, DecoderNumChannels);
	}


	// Make sure we have a big enough buffer for decoding
	if (ReceivedEncodedAudioDataBuffer.GetAllocatedSize() < SizeBeforeEncode * 20) {
		ReceivedEncodedAudioDataBuffer.Reserve(SizeBeforeEncode * 20);
	}

	// Decode the Audio data
	uint32 outsize = ReceivedEncodedAudioDataBuffer.GetAllocatedSize();
	//CONVAI_LOG(ConvaiAudioStreamerLog, Log, TEXT("Allocated buffer size for decoding: %d bytes"), outsize);

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

	//CONVAI_LOG(ConvaiAudioStreamerLog, Log, TEXT("Decoder Received %d bytes and Outputted %d bytes"), EncodedVoiceData.Num(), outsize);
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
    // Do not play incoming audio on the client instance if muted
    if ((ShouldMuteLocal() && GetOwner()->HasLocalNetOwner()) || ShouldMuteGlobal())
    {
        return;
    }

    HandleAudioReceived(VoiceData, VoiceDataSize, ContainsHeaderData, SampleRate, NumChannels);
}

namespace
{
	void HandleAudioTimer(TWeakObjectPtr<UConvaiAudioStreamer> WeakSelf, int32 PCM_DataSize, int32 SampleRate)
	{
		if (!WeakSelf.IsValid() || !IsValid(WeakSelf->GetWorld()))
		{
			CONVAI_LOG(ConvaiAudioStreamerLog, Warning, TEXT("PlayVoiceData: Object or World is Invalid!"));
			return;
		}

		float NewAudioDuration = float(PCM_DataSize) / float(SampleRate * 2);

		
		double CurrentTime = FPlatformTime::Seconds();
		
		double RemainingAudioDuration = 0.0;
		if (WeakSelf->AudioEndTime > 0.0)
		{
			RemainingAudioDuration = WeakSelf->AudioEndTime - CurrentTime;
			if (RemainingAudioDuration < 0.0)
				RemainingAudioDuration = 0.0;
		}

		
		double TotalAudioDuration = RemainingAudioDuration + NewAudioDuration;
		
		WeakSelf->AudioEndTime = CurrentTime + TotalAudioDuration;

		if (WeakSelf.IsValid() && IsValid(WeakSelf->GetWorld()))
		{
			WeakSelf->GetWorld()->GetTimerManager().SetTimer(
				WeakSelf->AudioFinishedTimerHandle, 
				WeakSelf.Get(), 
				&UConvaiAudioStreamer::onAudioFinished, 
				TotalAudioDuration, 
				false);
		}
		else
		{
			CONVAI_LOG(ConvaiAudioStreamerLog, Warning, TEXT("PlayVoiceData: Object or World became invalid before setting timer!"));
		}
	}

	void SetupAndPlayAudio(TWeakObjectPtr<UConvaiAudioStreamer> WeakThis, TArray<uint8> AudioDataCopy, int32 SampleRate, int32 NumChannels)
	{
		if (!WeakThis.IsValid() || !IsValid(WeakThis->SoundWaveProcedural))
		{
			return;
		}

		WeakThis->SetSound(WeakThis->SoundWaveProcedural);
		WeakThis->SoundWaveProcedural->QueueAudio(AudioDataCopy.GetData(), AudioDataCopy.Num());
		WeakThis->Play();

		// Lipsync component process the audio data to generate the lipsync
		if (WeakThis.IsValid() && !(WeakThis->ConvaiLipSync && WeakThis->ConvaiLipSync->RequiresPrecomputedFaceData()))
		{
			uint8* NonConstData = const_cast<uint8*>(AudioDataCopy.GetData());
			WeakThis->PlayLipSync(NonConstData, AudioDataCopy.Num(), SampleRate, NumChannels);
		}

		WeakThis->ForceRecalculateLipsyncStartTime();
	}
};

void UConvaiAudioStreamer::ProcessPendingAudio()
{
	if (!IsValid(SoundWaveProcedural) && PendingAudioBuffer.Num() <= 0)
		return;

    // Process the buffer if there's any data
    SoundWaveProcedural->QueueAudio(PendingAudioBuffer.GetData(), PendingAudioBuffer.Num());

	// Lipsync component process the audio data to generate the lipsync
	if (!(ConvaiLipSync && ConvaiLipSync->RequiresPrecomputedFaceData()))
	{
		uint32 SampleRate = SoundWaveProcedural->GetSampleRateForCurrentPlatform();
		uint32 NumChannels = SoundWaveProcedural->NumChannels;
		PlayLipSync(PendingAudioBuffer.GetData(), PendingAudioBuffer.Num(), SampleRate>0? SampleRate : 48000, NumChannels > 0? NumChannels : 1);
	}
    PendingAudioBuffer.Empty();
}

void UConvaiAudioStreamer::PlayVoiceData(uint8* VoiceData, uint32 VoiceDataSize, bool ContainsHeaderData, uint32 SampleRate, uint32 NumChannels)
{
    if (IsVoiceCurrentlyFading())
        StopVoice();
    ResetVoiceFade();

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

			// Play only the PCM data which start after 44 bytes
			VoiceData += 44;
			VoiceDataSize -= 44;
        }
        else if (!ParseSuccess)
        {
            CONVAI_LOG(ConvaiAudioStreamerLog, Warning, TEXT("PlayVoiceData: Failed to parse wav header, reason: %s"), *ErrorReason);
        }
    }

    TWeakObjectPtr<UConvaiAudioStreamer> WeakSelf = this;

    if (IsInGameThread())
    {
        HandleAudioTimer(WeakSelf, VoiceDataSize, SampleRate);
    }
    else
    {
        AsyncTask(ENamedThreads::GameThread, [WeakSelf, VoiceDataSize, SampleRate]()
        {
            HandleAudioTimer(WeakSelf, VoiceDataSize, SampleRate);
        });
    }
    
    if (!IsValid(SoundWaveProcedural))
        return;

    // If configuring audio then queue the audio and return
    if (IsAudioConfiguring)
    {
        // Lock is already held, queue this audio for later processing
        if (ContainsHeaderData)
        {
            // Skip header for the queue
            PendingAudioBuffer.Append(VoiceData + 44, VoiceDataSize - 44);
        }
        else
        {
            PendingAudioBuffer.Append(VoiceData, VoiceDataSize);
        }
        
        // Try the lock again before exiting - if it's available now, process the queue
        if (!IsAudioConfiguring)
        {
			IsAudioConfiguring.AtomicSet(true);
            ProcessPendingAudio();
			IsAudioConfiguring.AtomicSet(false);
        }
        
        return;
    }
    
    // We have the lock, proceed with processing
    
    // Check that SoundWaveProcedural is valid and able to play input sample rate and channels
    if (SoundWaveProcedural->GetSampleRateForCurrentPlatform() != SampleRate || SoundWaveProcedural->NumChannels != NumChannels)
    {
		IsAudioConfiguring.AtomicSet(true);

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

        CONVAI_LOG(ConvaiAudioStreamerLog, Log, TEXT("New SampleRate: %d"), SampleRate);
        CONVAI_LOG(ConvaiAudioStreamerLog, Log, TEXT("New Channels: %d"), NumChannels);

        // Create a copy of the audio data for thread safety
        TArray<uint8> AudioDataCopy;
		AudioDataCopy.Append(VoiceData, VoiceDataSize);
        
        // Store a copy of the weak pointer for the lambda
        TWeakObjectPtr<UConvaiAudioStreamer> WeakThis = this;
        

        if (IsInGameThread())
		{
            SetupAndPlayAudio(WeakThis, AudioDataCopy, SampleRate, NumChannels);
            
            // Process any pending audio before releasing the lock
            ProcessPendingAudio();

            // Release the lock
			IsAudioConfiguring.AtomicSet(false);
        }
        else
        {            
            AsyncTask(ENamedThreads::GameThread, [WeakThis, AudioDataCopy, SampleRate, NumChannels]()
                {
                    if (WeakThis.IsValid())
						SetupAndPlayAudio(WeakThis, AudioDataCopy, SampleRate, NumChannels);
                    
                    // Process any pending audio before releasing the lock
                    if (WeakThis.IsValid())
                        WeakThis->ProcessPendingAudio();

					// Release the lock
					if (WeakThis.IsValid())
						WeakThis->IsAudioConfiguring.AtomicSet(false);
                });
        }

        if (!IsTalking)
        {
            onAudioStarted();
            IsTalking = true;
        }
        
        return;
    }
    
    SoundWaveProcedural->QueueAudio(VoiceData, VoiceDataSize);

    if (!IsTalking)
    {
        onAudioStarted();
        IsTalking = true;
    }

    // Lipsync component process the audio data to generate the lipsync
    if (!(ConvaiLipSync && ConvaiLipSync->RequiresPrecomputedFaceData()))
    {
        PlayLipSync(VoiceData, VoiceDataSize, SampleRate, NumChannels);
    }
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
    // Reset the audio end time
    AudioEndTime = 0.0;
    
    // Clear audio and lipsync buffers
    AudioBuffer.Reset();
    LipSyncBuffer.Reset();

    // Reset tracking variables
    TotalPlayingDuration = 0.0f;
    TotalBufferedDuration = 0.0f;
    
    // Transition to Stopped state
    TransitionToState(EAudioLipSyncState::Stopped);

    // If we're not talking and buffers are empty, nothing to do
    if (!IsTalking)
        return;
        
    // Reset audio playback
    if (SoundWaveProcedural)
        SoundWaveProcedural->ResetAudio();
    
    // Reset lipsync state
    StopLipSync();
    
    // Notify that audio has finished
    onAudioFinished();
    
    // Clear any pending timers
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
	if (!IsTalking && AudioBuffer.IsEmpty() && LipSyncBuffer.IsEmpty())
		return;

	if (!IsValid(GetWorld()))
	{
		CONVAI_LOG(ConvaiAudioStreamerLog, Warning, TEXT("PlayVoiceData: GetWorld() is Invalid!"));
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
		CONVAI_LOG(ConvaiAudioStreamerLog, Warning, TEXT("ClearAudioFinishedTimer: GetWorld() is Invalid!"));
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
		ConvaiLipSync->OnVisemesDataReady.BindUObject(this, &UConvaiAudioStreamer::OnVisemesReadyCallback);

		// Should lipsync and audio be synchronized
		EnableSync = UConvaiSettingsUtils::GetParamValueAsFloat("EnableSync", EnableSync) ? EnableSync : 1;
		EnableSync = EnableSync >= 0.5 ? 1 : 0;
		
		// Update bIsSyncingAudioAndLipSync based on the new component
		bIsSyncingAudioAndLipSync = EnableSync && SupportsLipSync() && ConvaiLipSync->RequiresPrecomputedFaceData() && !ReplicateVoiceToNetwork;
		
		return true;
	}
	else
	{
		ConvaiLipSync = nullptr;
		
		// Update bIsSyncingAudioAndLipSync
		bIsSyncingAudioAndLipSync = false;
		
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

    // Initialize state
    CurrentState = EAudioLipSyncState::Stopped;
    
    // Initialize tracking variables
    TotalPlayingDuration = 0.0f;
    TotalBufferedDuration = 0.0f;
    
    // Initialize configuration parameters

	// Minimum buffer duration in seconds
	MinBufferDuration = UConvaiSettingsUtils::GetParamValueAsFloat("MinBufferDuration", MinBufferDuration) ? MinBufferDuration : 0.9f;
	MinBufferDuration = MinBufferDuration < 0 ? 0 : MinBufferDuration;
    	
	// Ratio of lipsync to audio duration required
	AudioLipSyncRatio = UConvaiSettingsUtils::GetParamValueAsFloat("AudioLipSyncRatio", AudioLipSyncRatio) ? AudioLipSyncRatio : 0.1f;
	AudioLipSyncRatio = AudioLipSyncRatio < 0 ? 0 : AudioLipSyncRatio;
	AudioLipSyncRatio = AudioLipSyncRatio > 1 ? 1 : AudioLipSyncRatio;

	// Initialize the audio component
	bAutoActivate = true;
	bAlwaysPlay = true;

	SoundWaveProcedural = NewObject<USoundWaveProcedural>();

	if (ConvaiLipSync == nullptr)
		FindFirstLipSyncComponent();

	if (ConvaiVision == nullptr)
		FindFirstVisionComponent();
}

void UConvaiAudioStreamer::TransitionToState(EAudioLipSyncState NewState)
{
    if (CurrentState == NewState)
        return;
        
    // Use a simpler logging approach that doesn't require UEnum reflection
    const TCHAR* CurrentStateStr = nullptr;
    const TCHAR* NewStateStr = nullptr;
    
    switch (CurrentState)
    {
        case EAudioLipSyncState::Stopped: CurrentStateStr = TEXT("Stopped"); break;
        case EAudioLipSyncState::Playing: CurrentStateStr = TEXT("Playing"); break;
        case EAudioLipSyncState::WaitingOnLipSync: CurrentStateStr = TEXT("WaitingOnLipSync"); break;
        case EAudioLipSyncState::WaitingOnAudio: CurrentStateStr = TEXT("WaitingOnAudio"); break;
    }
    
    switch (NewState)
    {
        case EAudioLipSyncState::Stopped: NewStateStr = TEXT("Stopped"); break;
        case EAudioLipSyncState::Playing: NewStateStr = TEXT("Playing"); break;
        case EAudioLipSyncState::WaitingOnLipSync: NewStateStr = TEXT("WaitingOnLipSync"); break;
        case EAudioLipSyncState::WaitingOnAudio: NewStateStr = TEXT("WaitingOnAudio"); break;
    }
    
    CONVAI_LOG(ConvaiAudioStreamerLog, Log, TEXT("State transition: %s -> %s"), 
        CurrentStateStr, NewStateStr);
        
    CurrentState = NewState;
    
    // Handle state entry actions
    switch (CurrentState)
    {
        case EAudioLipSyncState::Stopped:
            // Clear buffers when stopping
            AudioBuffer.Reset();
            LipSyncBuffer.Reset();
            break;
            
        case EAudioLipSyncState::Playing:
            // Nothing special on entry to Playing state
            break;
            
        case EAudioLipSyncState::WaitingOnLipSync:
            // Nothing special on entry to WaitingOnLipSync state
            break;
            
        case EAudioLipSyncState::WaitingOnAudio:
            // Nothing special on entry to WaitingOnAudio state
            break;
    }
}

// Handle received audio data
void UConvaiAudioStreamer::HandleAudioReceived(uint8* AudioData, uint32 AudioDataSize, bool ContainsHeaderData, uint32 SampleRate, uint32 NumChannels)
{
    // Calculate audio duration
    uint32 PCM_DataSize = AudioDataSize;
    if (ContainsHeaderData)
    {
        // Parse WAV header
        FWaveModInfo WaveInfo;
        if (WaveInfo.ReadWaveInfo(AudioData, AudioDataSize))
        {
            PCM_DataSize = *WaveInfo.pWaveDataSize;
        }
    }
    
    float AudioDuration = UConvaiUtils::CalculateAudioDuration(PCM_DataSize, NumChannels, SampleRate, 2);
    
    // Add to buffer
    if (AudioBuffer.IsEmpty())
    {
        AudioBuffer.AppendData(AudioData, AudioDataSize);
        AudioBuffer.Duration = AudioDuration;
        AudioBuffer.SampleRate = SampleRate;
        AudioBuffer.NumChannels = NumChannels;
    }
    else
    {
        AudioBuffer.AppendData(AudioData, AudioDataSize);
        AudioBuffer.Duration += AudioDuration;
    }
    
    // Update buffered duration
    TotalBufferedDuration = AudioBuffer.Duration;
    
    // Handle based on current state
    switch (CurrentState)
    {
        case EAudioLipSyncState::Stopped:
        case EAudioLipSyncState::Playing:
        case EAudioLipSyncState::WaitingOnAudio:
            if (HasSufficientLipSync())
            {
				if (HasSufficientAudio())
				{
					TryPlayBufferedContent();
				}
				else
				{
					TransitionToState(EAudioLipSyncState::WaitingOnAudio);
					break;
				}                
            }
            else
            {
                TransitionToState(EAudioLipSyncState::WaitingOnLipSync);
            }
            break;
            
        case EAudioLipSyncState::WaitingOnLipSync:
            // Just buffer, waiting for lipsync
            break;
    }
}

// Handle received lipsync data
void UConvaiAudioStreamer::HandleLipSyncReceived(FAnimationSequence& FaceSequence)
{
    // Add to buffer
    LipSyncBuffer.AddSequence(FaceSequence);
    
    // Handle based on current state
    switch (CurrentState)
    {
        case EAudioLipSyncState::Stopped:
        case EAudioLipSyncState::Playing:
        case EAudioLipSyncState::WaitingOnLipSync:
			if (HasSufficientLipSync())
			{
				if (HasSufficientAudio())
				{
					TryPlayBufferedContent();
				}
				else
				{
					TransitionToState(EAudioLipSyncState::WaitingOnAudio);
					break;
				}
			}
			break;
            
        case EAudioLipSyncState::WaitingOnAudio:
            // Just buffer, waiting for audio
            break;
    }
}

// Check if we have sufficient lipsync data
bool UConvaiAudioStreamer::HasSufficientLipSync()
{        
	if (!SupportsLipSync() || !ConvaiLipSync->RequiresPrecomputedFaceData() || !bIsSyncingAudioAndLipSync)
		return true;
        
    float AudioDuration = AudioBuffer.GetTotalDuration();
    float LipSyncDuration = LipSyncBuffer.GetTotalDuration();
    
    return LipSyncDuration >= MinBufferDuration || LipSyncDuration >= AudioDuration * AudioLipSyncRatio;
}

// Check if we have sufficient audio data
bool UConvaiAudioStreamer::HasSufficientAudio() const
{
    if (AudioBuffer.IsEmpty())
	{
        return false;
	}

    // Get audio duration
    float AudioDuration = AudioBuffer.GetTotalDuration();
    
    // If we're not syncing audio and lipsync, just check against minimum buffer duration
    if (!bIsSyncingAudioAndLipSync)
    {
        return AudioDuration >= MinBufferDuration;
    }
    
    // Otherwise, check against both minimum duration and lipsync duration
    float LipSyncDuration = LipSyncBuffer.GetTotalDuration();
    
    return AudioDuration >= MinBufferDuration && AudioDuration >= LipSyncDuration;
}

// Try to play buffered content
bool UConvaiAudioStreamer::TryPlayBufferedContent(bool force)
{
	if (force)
	{
		float AudioBufferDuration = AudioBuffer.GetTotalDuration();
		float PlayDuration = AudioBufferDuration;
		if (PlayDuration <= 0.0f)
		{
			return false;
		}

		// Play the content
		PlayBufferedContent(PlayDuration);

		// Transition to Playing state
		TransitionToState(EAudioLipSyncState::Playing);

		return true;
	}

    if (AudioBuffer.IsEmpty() || (bIsSyncingAudioAndLipSync && SupportsLipSync() && ConvaiLipSync->RequiresPrecomputedFaceData() && LipSyncBuffer.IsEmpty()))
    {
        return false;
    }

    // Calculate how much we can play
	float AudioBufferDuration = AudioBuffer.GetTotalDuration();
	float LipSyncBufferDuration = LipSyncBuffer.GetTotalDuration();
    float PlayDuration = bIsSyncingAudioAndLipSync? FMath::Min(AudioBufferDuration, LipSyncBufferDuration) : AudioBufferDuration;
    if (PlayDuration <= 0.0f)
	{
        return false;
	}
        
    // Play the content
    PlayBufferedContent(PlayDuration);
    
    // Transition to Playing state
    TransitionToState(EAudioLipSyncState::Playing);
	
    return true;
}

// Play buffered content up to the specified duration
void UConvaiAudioStreamer::PlayBufferedContent(float Duration)
{
    // Update tracking variables
    TotalPlayingDuration += Duration;
    TotalBufferedDuration -= Duration;
    
    // Play audio
    if (!AudioBuffer.IsEmpty())
    {
        // Calculate how many bytes to play based on sample rate, channels, and bytes per sample
        uint32 BytesToPlay = AudioBuffer.Data.RingDataUsage();
        
        if (Duration < AudioBuffer.Duration)
        {
            // Calculate samples to play based on duration
            uint32 SampleRate = AudioBuffer.SampleRate;
            uint32 NumChannels = AudioBuffer.NumChannels;
            uint32 BytesPerSample = 2; // Assuming 16-bit PCM (2 bytes per sample)
            
            // Calculate total samples to play (per channel)
            uint32 SamplesToPlay = FMath::FloorToInt(Duration * SampleRate);
            
            // Calculate total bytes to play (all channels)
            BytesToPlay = SamplesToPlay * NumChannels * BytesPerSample;
            
            // Ensure we don't exceed the buffer size
            BytesToPlay = FMath::Min(BytesToPlay, AudioBuffer.Data.RingDataUsage());
            
            // Ensure we're aligned to a sample boundary
            BytesToPlay = (BytesToPlay / (NumChannels * BytesPerSample)) * (NumChannels * BytesPerSample);
        }
        
        // Ensure our temp buffer is large enough
        if ((uint32)TempAudioBuffer.Max() < BytesToPlay)
        {
            // Only resize if necessary
            TempAudioBuffer.SetNumUninitialized(BytesToPlay);
        }
        else
        {
            // Just set the used size without reallocating
            TempAudioBuffer.SetNumUninitialized(BytesToPlay, false);
        }
        
        // Get the data from the ring buffer
        AudioBuffer.GetData(TempAudioBuffer.GetData(), BytesToPlay);
        
        // Play the audio
        PlayVoiceData(TempAudioBuffer.GetData(), BytesToPlay, false, 
            AudioBuffer.SampleRate, AudioBuffer.NumChannels);
            
        // Remove played data from buffer
        if (BytesToPlay >= AudioBuffer.Data.RingDataUsage())
        {
            AudioBuffer.Reset();
        }
        else
        {
            // Efficiently remove data from the ring buffer
            AudioBuffer.RemoveData(BytesToPlay);
            
            // Recalculate the remaining duration based on the actual bytes removed
            float PlayedDuration = (float)BytesToPlay / ((float)AudioBuffer.SampleRate * (float)AudioBuffer.NumChannels * 2.0f);
            AudioBuffer.Duration -= PlayedDuration;
        }
    }
    
    // Play lipsync
    if (!LipSyncBuffer.IsEmpty() && SupportsLipSync())
    {
        float RemainingDuration = Duration;
        float TotalPlayedLipSyncDuration = 0.0f;
        TArray<FAnimationSequence> SequencesToPlay;
        
        // First, collect complete sequences that fit within our duration
        for (int32 i = 0; i < LipSyncBuffer.Sequences.Num() && RemainingDuration > 0.0f; ++i)
        {
            const FAnimationSequence& Sequence = LipSyncBuffer.Sequences[i];
            
            if (Sequence.Duration <= RemainingDuration)
            {
                // This sequence fits completely
                SequencesToPlay.Add(Sequence);
                RemainingDuration -= Sequence.Duration;
                TotalPlayedLipSyncDuration += Sequence.Duration;
            }
            else
            {
                // This sequence is too long - we need to split it
                FAnimationSequence PartialSequence;
                PartialSequence.FrameRate = Sequence.FrameRate;
                
                // Calculate how many frames we can include
                float FrameDuration = 1.0f / Sequence.FrameRate;
                int32 FramesToInclude = FMath::FloorToInt(RemainingDuration / FrameDuration);
                
                if (FramesToInclude > 0)
                {
                    // Copy the frames we can include
                    for (int32 j = 0; j < FramesToInclude && j < Sequence.AnimationFrames.Num(); ++j)
                    {
                        PartialSequence.AnimationFrames.Add(Sequence.AnimationFrames[j]);
                    }
                    
                    // Calculate actual duration of partial sequence
                    PartialSequence.Duration = FramesToInclude * FrameDuration;
                    
                    // Add to play list
                    SequencesToPlay.Add(PartialSequence);
                    TotalPlayedLipSyncDuration += PartialSequence.Duration;
                    
                    // Create remainder sequence for the buffer
                    FAnimationSequence RemainderSequence;
                    RemainderSequence.FrameRate = Sequence.FrameRate;
                    
                    // Copy the remaining frames
                    for (int32 j = FramesToInclude; j < Sequence.AnimationFrames.Num(); ++j)
                    {
                        RemainderSequence.AnimationFrames.Add(Sequence.AnimationFrames[j]);
                    }
                    
                    // Calculate duration of remainder
                    RemainderSequence.Duration = Sequence.Duration - PartialSequence.Duration;
                    
                    // Replace the original sequence with the remainder
                    LipSyncBuffer.Sequences[i] = RemainderSequence;
                }
                
                // We've used all our duration
                RemainingDuration = 0.0f;
                break;
            }
        }
        
        // Play all the sequences we collected
        for (const FAnimationSequence& Sequence : SequencesToPlay)
        {
            PlayLipSyncWithPrecomputedFacialAnimation(Sequence);
        }
        
        // Remove completely played sequences from buffer
        int32 SequencesToRemove = 0;
        for (int32 i = 0; i < LipSyncBuffer.Sequences.Num(); ++i)
        {
            if (i < SequencesToPlay.Num() && LipSyncBuffer.Sequences[i].Duration == SequencesToPlay[i].Duration)
            {
                SequencesToRemove++;
            }
            else
            {
                break;
            }
        }
        
        if (SequencesToRemove > 0)
        {
            LipSyncBuffer.Sequences.RemoveAt(0, SequencesToRemove);
            LipSyncBuffer.TotalDuration -= TotalPlayedLipSyncDuration;
        }
    }
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

		//CONVAI_LOG(ConvaiAudioStreamerLog, Log, TEXT("Encoder Received %d bytes and Outputted %d bytes"), SizeOfDataBeforeEncoding, CurrentEncodedAudioDataSize);
		//CONVAI_LOG(ConvaiAudioStreamerLog, Log, TEXT("Remaining %d bytes"), AudioDataBuffer.Num());
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

void UConvaiAudioStreamer::PlayLipSyncWithPrecomputedFacialAnimationSynced(FAnimationSequence& FaceSequence)
{
    // If we don't need lipsync synchronization, just play the lipsync directly
    if (!bIsSyncingAudioAndLipSync)
    {
        PlayLipSyncWithPrecomputedFacialAnimation(FaceSequence);
        return;
    }
    
    // Otherwise, handle the lipsync through our state machine
    HandleLipSyncReceived(FaceSequence);
}

void UConvaiAudioStreamer::PlayLipSyncWithPrecomputedFacialAnimation(FAnimationSequence FaceSequence)
{
	if (SupportsLipSync() && ConvaiLipSync->RequiresPrecomputedFaceData())
	{
		ConvaiLipSync->ConvaiApplyPrecomputedFacialAnimation(nullptr, 0, 0, 0, FaceSequence);
	}
}

void UConvaiAudioStreamer::PlayLipSync(uint8* InPCMData, uint32 InPCMDataSize, uint32 InSampleRate, uint32 InNumChannels)
{
	if (SupportsLipSync())
	{
		ConvaiLipSync->ConvaiInferFacialDataFromAudio(InPCMData, InPCMDataSize, InSampleRate, InNumChannels);
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
		//ConvaiLipSync->ConvaiPauseLipSync();
	}
}

void UConvaiAudioStreamer::ResumeLipSync()
{
	if (ConvaiLipSync)
	{
		//ConvaiLipSyncExtended->ConvaiResumeLipSync();
	}
}

bool UConvaiAudioStreamer::CanUseLipSync()
{
	return false;
}

void UConvaiAudioStreamer::ForceRecalculateLipsyncStartTime()
{
	if (SupportsLipSync())
	{
		ConvaiLipSync->ForceRecalculateStartTime();
	}
}

bool UConvaiAudioStreamer::CanUseVision()
{
	return false;
}

void UConvaiAudioStreamer::OnVisemesReadyCallback()
{
	OnVisemesReady.Broadcast();
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
	if (ConvaiLipSync)
	{
		return ConvaiLipSync->ConvaiGetFaceBlendshapes();
	}
	return TMap<FName, float>();
}

bool UConvaiAudioStreamer::GeneratesVisemesAsBlendshapes()
{
	if (SupportsLipSync())
	{
		return ConvaiLipSync->GeneratesVisemesAsBlendshapes();
	}
	return false;
}

void UConvaiAudioStreamer::AddFaceDataToSend(FAnimationSequence FaceSequence)
{
	PlayLipSyncWithPrecomputedFacialAnimationSynced(FaceSequence);
}

void UConvaiAudioStreamer::AddPCMDataToSend(TArray<uint8> PCMDataToAdd,
											bool ContainsHeaderData,
                                            uint32 InSampleRate,
                                            uint32 InNumChannels) {
	// Validate input parameters to prevent crashes from corrupted data
	const uint32 MaxReasonableBufferSize = 1024 * 1024 * 10; // 10 MB max
	const uint32 MaxReasonableSampleRate = 192000; // 192 kHz max
	const uint32 MaxReasonableChannels = 2; // 8 channels max

	// Check for invalid/corrupted parameters
	if (PCMDataToAdd.Num() == 0 || PCMDataToAdd.Num() > (int32)MaxReasonableBufferSize)
	{
		CONVAI_LOG(ConvaiAudioStreamerLog, Warning, TEXT("AddPCMDataToSend: Invalid PCMDataToAdd size: %d. Ignoring data."), PCMDataToAdd.Num());
		return;
	}

	if (InSampleRate > MaxReasonableSampleRate)
	{
		CONVAI_LOG(ConvaiAudioStreamerLog, Warning, TEXT("AddPCMDataToSend: Invalid InSampleRate value: %u. Ignoring data."), InSampleRate);
		return;
	}

	if (InNumChannels > MaxReasonableChannels)
	{
		CONVAI_LOG(ConvaiAudioStreamerLog, Warning, TEXT("AddPCMDataToSend: Invalid InNumChannels value: %u. Ignoring data."), InNumChannels);
		return;
	}

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

			// Validate parsed values
			if (InSampleRate == 0 || InSampleRate > MaxReasonableSampleRate)
			{
				CONVAI_LOG(ConvaiAudioStreamerLog, Warning, TEXT("AddPCMDataToSend: Invalid parsed SampleRate: %u. Ignoring data."), InSampleRate);
				return;
			}

			if (InNumChannels == 0 || InNumChannels > MaxReasonableChannels)
			{
				CONVAI_LOG(ConvaiAudioStreamerLog, Warning, TEXT("AddPCMDataToSend: Invalid parsed NumChannels: %u. Ignoring data."), InNumChannels);
				return;
			}

			PCMDataToAdd.RemoveAt(0, 44); // Remove the header bytes
		}
		else if (!ParseSuccess)
		{
			CONVAI_LOG(ConvaiAudioStreamerLog, Warning, TEXT("AddPCMDataToSend: Failed to parse wav header, reason: %s"), *ErrorReason);
			return; // Don't process invalid data
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
			CONVAI_LOG(ConvaiAudioStreamerLog, Log, TEXT("Initialized Encoder with SampleRate:%d and Channels:%d"), EncoderSampleRate, EncoderNumChannels);
		}
		AudioDataBuffer.Append((uint8*)OutConverted.GetData(), OutConverted.Num()*2);
	}
	else if (!ShouldMuteLocal())
	{
		// Just play it locally
		PlayVoiceSynced((uint8*)OutConverted.GetData(), OutConverted.Num()*2, false, InSampleRate, InNumChannels);
	}
}

float UConvaiAudioStreamer::GetRemainingContentDuration()
{
    // If we're not talking, return 0
    if (!IsTalking)
        return 0.0f;
    
    // Calculate buffered durations
    float BufferedAudioDuration = AudioBuffer.IsEmpty() ? 0.0f : AudioBuffer.Duration;
    float BufferedLipSyncDuration = LipSyncBuffer.IsEmpty() ? 0.0f : LipSyncBuffer.TotalDuration;
    
    // Check if we have sufficient audio
    if (!HasSufficientAudio())
        return 0.0f;
    
    // If we need synced lipsync, also check if we have sufficient lipsync
    if (bIsSyncingAudioAndLipSync && !HasSufficientLipSync())
        return 0.0f;
    
    // For audio-only playback, return audio duration
    if (!bIsSyncingAudioAndLipSync)
        return BufferedAudioDuration;
    
    // For synced playback, return the minimum of audio and lipsync duration
    return FMath::Min(BufferedAudioDuration, BufferedLipSyncDuration);
}

void UConvaiAudioStreamer::onAudioStarted()
{
	AsyncTask(ENamedThreads::GameThread, [this] {
		OnStartedTalking.Broadcast();
		});
	
}

void UConvaiAudioStreamer::onAudioFinished()
{
    CONVAI_LOG(ConvaiAudioStreamerLog, Log, TEXT("onAudioFinished"));
    
    // Reset the audio end time
    AudioEndTime = 0.0;
    TotalPlayingDuration = 0.0f;
        
    // Broadcast that audio has finished
    AsyncTask(ENamedThreads::GameThread, [this] {
        OnFinishedTalking.Broadcast();
    });
    
    IsTalking = false;
}

bool UConvaiAudioStreamer::InitEncoder(int32 InSampleRate, int32 InNumChannels, EAudioEncodeHint EncodeHint)
{
	if (InSampleRate != 8000 &&
		InSampleRate != 12000 &&
		InSampleRate != 16000 &&
		InSampleRate != 24000 &&
		InSampleRate != 48000)
	{
		CONVAI_LOG(ConvaiAudioStreamerLog, Warning, TEXT("Voice encoder doesn't support %d hz"), InSampleRate);
		return false;
	}

	if (InNumChannels < 1 || InNumChannels > 2)
	{
		CONVAI_LOG(ConvaiAudioStreamerLog, Warning, TEXT("Voice encoder only supports 1 or 2 channels"));
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
		CONVAI_LOG(ConvaiAudioStreamerLog, Warning, TEXT("Failed to init Opus Encoder: %s"), ANSI_TO_TCHAR(opus_strerror(EncError)));
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

	//CONVAI_LOG(ConvaiAudioStreamerLog, Log, TEXT("NumFramesToEncode %d frames"), NumFramesToEncode);


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
			CONVAI_LOG(ConvaiAudioStreamerLog, Warning, TEXT("Failed to encode: [%d] %s"), CompressedLength, ANSI_TO_TCHAR(ErrorStr));

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
			CONVAI_LOG(ConvaiAudioStreamerLog, Warning, TEXT("Nothing to encode!"));
			CompressedOffsets[i] = 0;
		}
	}

	// End of buffer
	OutCompressedDataSize = HeaderSize + CompressedBufferOffset;

	CONVAI_LOG(ConvaiAudioStreamerLog, Verbose, TEXT("OpusEncode[%d]: RawSize: %d HeaderSize: %d CompressedSize: %d NumFramesEncoded: %d Remains: %d"), EncoderGeneration, RawDataSize, HeaderSize, OutCompressedDataSize, NumFramesToEncode, DataRemainder);

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
	CONVAI_LOG(ConvaiAudioStreamerLog, Display, TEXT("DecoderVersion: %s"), ANSI_TO_TCHAR(opus_get_version_string()));

	if (InSampleRate != 8000 &&
		InSampleRate != 12000 &&
		InSampleRate != 16000 &&
		InSampleRate != 24000 &&
		InSampleRate != 48000)
	{
		CONVAI_LOG(ConvaiAudioStreamerLog, Warning, TEXT("Voice decoder doesn't support %d hz"), InSampleRate);
		return false;
	}

	if (InNumChannels < 1 || InNumChannels > 2)
	{
		CONVAI_LOG(ConvaiAudioStreamerLog, Warning, TEXT("Voice decoder only supports 1 or 2 channels"));
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
		CONVAI_LOG(ConvaiAudioStreamerLog, Warning, TEXT("Failed to init Opus Decoder: %s"), ANSI_TO_TCHAR(opus_strerror(DecError)));
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

	//CONVAI_LOG(ConvaiAudioStreamerLog, Log, TEXT("NumFramesToDecode %d frames"), NumFramesToDecode);

	if (PacketGeneration != DecoderLastGeneration + 1)
	{
		CONVAI_LOG(ConvaiAudioStreamerLog, Warning, TEXT("Packet generation skipped from %d to %d"), DecoderLastGeneration, PacketGeneration);
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
							CONVAI_LOG(ConvaiAudioStreamerLog, Warning, TEXT("Failed to decode: [%d] %s"), NumDecompressedSamples, ANSI_TO_TCHAR(ErrorStr));
						}
						else
						{
							if (NumDecompressedSamples != DecoderFrameSize)
							{
								CONVAI_LOG(ConvaiAudioStreamerLog, Warning, TEXT("Unexpected decode result NumSamplesDecoded %d != FrameSize %d"), NumDecompressedSamples, DecoderFrameSize);
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
						CONVAI_LOG(ConvaiAudioStreamerLog, Warning, TEXT("Decompression buffer skipped a frame"));
						// Nothing to advance within the compressed input stream
					}
				}
				else
				{
					CONVAI_LOG(ConvaiAudioStreamerLog, Warning, TEXT("Decompression buffer too small to decode voice"));
					break;
				}
			}

			OutRawDataSize = DecompressedBufferOffset;
		}
		else
		{
			CONVAI_LOG(ConvaiAudioStreamerLog, Warning, TEXT("Failed to decode: header corrupted"));
			OutRawDataSize = 0;
		}
	}
	else
	{
		CONVAI_LOG(ConvaiAudioStreamerLog, Warning, TEXT("Failed to decode: buffer corrupted"));
		OutRawDataSize = 0;
	}

	CONVAI_LOG(ConvaiAudioStreamerLog, Verbose, TEXT("OpusDecode[%d]: RawSize: %d HeaderSize: %d CompressedSize: %d NumFramesDecoded: %d "), PacketGeneration, OutRawDataSize, HeaderSize, CompressedDataSize, NumFramesToDecode);

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
