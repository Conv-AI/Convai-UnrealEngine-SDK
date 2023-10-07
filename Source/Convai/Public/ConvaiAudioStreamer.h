// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once
//#include "CoreMinimal.h"
// #undef UpdateResource
#include "Components/AudioComponent.h"
#include "ConvaiDefinitions.h"
#include "Interfaces/VoiceCodec.h"

#include "ConvaiAudioStreamer.generated.h"

#define NUM_ENTROPY_VALUES 5

DECLARE_LOG_CATEGORY_EXTERN(ConvaiAudioStreamerLog, Log, All);

class USoundWaveProcedural;
class IConvaiLipSyncInterface;
class IConvaiLipSyncExtendedInterface;

UCLASS()
class UConvaiAudioStreamer : public UAudioComponent
{
	GENERATED_UCLASS_BODY()

	DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE(FOnStartedTalkingSignature, UConvaiAudioStreamer, OnStartedTalking);
	DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE(FOnFinishedTalkingSignature, UConvaiAudioStreamer, OnFinishedTalking);
	DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE(FOVRLipSyncVisemesDataReadySignature, UConvaiAudioStreamer, OnVisemesReady);

public:
	/** Send the encoded audio from the server to all clients (including the server again) */
	UFUNCTION(NetMulticast, Reliable, Category = "VoiceNetworking")
	void BroadcastVoiceDataToClients(TArray<uint8> const& EncodedVoiceData, uint32 SampleRate, uint32 NumChannels, uint32 SizeBeforeEncode);

	/** Send the encoded audio from a client(/server) to the server, it should call at the end BroadcastVoiceDataToClients() */
	UFUNCTION(Server, Reliable, Category = "VoiceNetworking")
	virtual void ProcessEncodedVoiceData(TArray<uint8> const& EncodedVoiceData, uint32 SampleRate, uint32 NumChannels, uint32 SizeBeforeEncode);

	/** If we should play audio on same client */
	virtual bool ShouldMuteLocal();

	/** If we should play audio on other clients */
	virtual bool ShouldMuteGlobal();

	virtual void OnServerAudioReceived(uint8* VoiceData, uint32 VoiceDataSize, bool ContainsHeaderData = true, uint32 SampleRate = 21000, uint32 NumChannels = 1) {};

	void PlayVoiceData(uint8* VoiceData, uint32 VoiceDataSize, bool ContainsHeaderData=true, uint32 SampleRate=21000, uint32 NumChannels=1);

	void PlayVoiceData(uint8* VoiceData, uint32 VoiceDataSize, bool ContainsHeaderData, FAnimationSequence FaceSequence, uint32 SampleRate = 21000, uint32 NumChannels = 1);

	//UFUNCTION(BlueprintCallable, Category = "Convai")
	void ForcePlayVoice(USoundWave* VoiceToPlay);

	void StopVoice();

	void StopVoiceWithFade(float InRemainingVoiceFadeOutTime);

	void ResetVoiceFade();

	void UpdateVoiceFade(float DeltaTime);

	bool IsVoiceCurrentlyFading();

	void ClearAudioFinishedTimer();

	bool IsLocal();

	/** Called when starts to talk */
	UPROPERTY(BlueprintAssignable, Category = "Convai")
	FOnStartedTalkingSignature OnStartedTalking;

	/** Called when stops to talk */
	UPROPERTY(BlueprintAssignable, Category = "Convai")
	FOnFinishedTalkingSignature OnFinishedTalking;

	/** Called when there are LipSync visemes available */
	UPROPERTY(BlueprintAssignable, Category = "Convai|LipSync")
	FOVRLipSyncVisemesDataReadySignature OnVisemesReady;

	IConvaiLipSyncInterface* FindFirstLipSyncComponent();

	UFUNCTION(BlueprintCallable, Category = "Convai|LipSync")
	bool SetLipSyncComponent(UActorComponent* LipSyncComponent);

	/** Returns true, if an LipSync Component was available and attached to the character */
	UFUNCTION(BlueprintPure, BlueprintCallable, Category = "Convai|LipSync")
	bool SupportsLipSync();

	bool ReplicateVoiceToNetwork;

public:
	// UActorComponent interface
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// UObject Interface.
	virtual void BeginDestroy() override;

public:

	FTimerHandle AudioFinishedTimerHandle;
	bool IsTalking = false;
	float TotalVoiceFadeOutTime;
	float RemainingVoiceFadeOutTime;

	UPROPERTY()
	USoundWaveProcedural* SoundWaveProcedural;

	TArray<uint8> AudioDataBuffer;
	TArray<uint8> ReceivedEncodedAudioDataBuffer;
 
	IConvaiLipSyncInterface* ConvaiLipSync;
	IConvaiLipSyncExtendedInterface* ConvaiLipSyncExtended;

	void PlayLipSyncWithPreGeneratedData(FAnimationSequence FaceSequence);

	void PlayLipSync(uint8* InPCMData, uint32 InPCMDataSize, uint32 InSampleRate, uint32 InNumChannels);

	void StopLipSync();

	void OnVisemesReadyCallback();

	UFUNCTION(BlueprintPure, Category = "Convai|LipSync", Meta = (Tooltip = "Returns last predicted viseme scores"))
	const TArray<float> GetVisemes() const;

	UFUNCTION(BlueprintPure, Category = "Convai|LipSync", Meta = (Tooltip = "Returns list of viseme names"))
	const TArray<FString> GetVisemeNames() const;

	UFUNCTION(BlueprintPure, Category = "Convai|LipSync", Meta = (Tooltip = "Returns map of blendshapes"))
	const TMap<FName, float> ConvaiGetFaceBlendshapes() const;

	UFUNCTION(BlueprintPure, Category = "Convai|LipSync", Meta = (Tooltip = "True if the output visemes is in Blendshape format"))
	bool GeneratesVisemesAsBlendshapes();

	void AddFaceDataToSend(FAnimationSequence FaceSequence);

	// Should be called in the game thread
	void AddPCMDataToSend(TArray<uint8> PCMDataToAdd, bool ContainsHeaderData = true, uint32 SampleRate = 21000, uint32 NumChannels = 1);

	virtual void onAudioStarted();
	virtual void onAudioFinished();

private:

	bool InitEncoder(int32 InSampleRate, int32 InNumChannels, EAudioEncodeHint EncodeHint);
	int32 Encode(const uint8* RawPCMData, uint32 RawDataSize, uint8* OutCompressedData, uint32& OutCompressedDataSize);
	void DestroyOpusEncoder();

	bool InitDecoder(int32 InSampleRate, int32 InNumChannels);
	void Decode(const uint8* CompressedData, uint32 CompressedDataSize, uint8* OutRawPCMData, uint32& OutRawDataSize);
	void DestroyOpusDecoder();

	void DestroyOpus();

	/** Sample rate encoding (supports 8000, 12000, 16000, 24000, 480000) */
	int32 EncoderSampleRate;
	/** Encoded channel count (supports 1,2) */
	int32 EncoderNumChannels;
	/**
	 * Number of samples encoded in a time slice "frame" (must match decoder)
	 * One frame defined as (2.5, 5, 10, 20, 40 or 60 ms) of audio data
	 * Voice encoding lower bound is 10ms (audio goes to 2.5ms).
	 * Voice encoding upper bound is 60ms (audio goes to 20ms).
	 * at 48 kHz the permitted values are 120 (2.5ms), 240 (5ms), 480 (10ms), 960 (20ms), 1920 (40ms), and 2880 (60ms)
	 */
	int32 EncoderFrameSize;
	/** Opus encoder stateful data */
	struct OpusEncoder* Encoder;
	/** Last value set in the call to Encode() */
	uint8 EncoderGeneration;


	/** Sample rate to decode into, regardless of encoding (supports 8000, 12000, 16000, 24000, 480000) */
	int32 DecoderSampleRate;
	/** Decoded channel count (supports 1,2) */
	int32 DecoderNumChannels;
	/**
	 * Number of samples encoded in a time slice (must match encoder)
	 * at 48 kHz the permitted values are 120, 240, 480, 960, 1920, and 2880
	 */
	int32 DecoderFrameSize;
	/** Opus decoder stateful data */
	struct OpusDecoder* Decoder;
	/** Generation value received from the last incoming packet */
	uint8 DecoderLastGeneration;

};