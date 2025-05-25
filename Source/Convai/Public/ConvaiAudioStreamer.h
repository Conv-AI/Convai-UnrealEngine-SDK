// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once
//#include "CoreMinimal.h"
// #undef UpdateResource
#include "Components/AudioComponent.h"
#include "ConvaiDefinitions.h"
#include "Misc/ScopeLock.h"
#include "Interfaces/VoiceCodec.h"

#include "CoreTypes.h"
#include "Templates/UnrealTemplate.h"
#include "HAL/PlatformAtomics.h"
#include "HAL/PlatformMisc.h"

#include "ConvaiAudioStreamer.generated.h"

#define NUM_ENTROPY_VALUES 5

DECLARE_LOG_CATEGORY_EXTERN(ConvaiAudioStreamerLog, Log, All);

class USoundWaveProcedural;
class IConvaiLipSyncInterface;
class IConvaiLipSyncExtendedInterface;
class IConvaiVisionInterface;


/**
 * Template for queues.
 *
 * This template implements an unbounded non-intrusive queue using a lock-free linked
 * list that stores copies of the queued items. The template can operate in two modes:
 * Multiple-producers single-consumer (MPSC) and Single-producer single-consumer (SPSC).
 *
 * The queue is thread-safe in both modes. The Dequeue() method ensures thread-safety by
 * writing it in a way that does not depend on possible instruction reordering on the CPU.
 * The Enqueue() method uses an atomic compare-and-swap in multiple-producers scenarios.
 *
 * @param T The type of items stored in the queue.
 * @param Mode The queue mode (single-producer, single-consumer by default).
 * @todo gmp: Implement node pooling.
 */
template<typename T, EQueueMode Mode = EQueueMode::Spsc>
class TConvaiQueue
{
public:
	using FElementType = T;

	/** Default constructor. */
	TConvaiQueue()
	{
		Head = Tail = new TNode();
	}

	/** Destructor. */
	~TConvaiQueue()
	{
		while (Tail != nullptr)
		{
			TNode* Node = Tail;
			Tail = Tail->NextNode;

			delete Node;
		}
	}

	/**
	 * Removes and returns the item from the tail of the queue.
	 *
	 * @param OutValue Will hold the returned value.
	 * @return true if a value was returned, false if the queue was empty.
	 * @note To be called only from consumer thread.
	 * @see Empty, Enqueue, IsEmpty, Peek, Pop
	 */
	bool Dequeue(FElementType& OutItem)
	{
		TNode* Popped = Tail->NextNode;

		if (Popped == nullptr)
		{
			return false;
		}

		TSAN_AFTER(&Tail->NextNode);
		OutItem = MoveTemp(Popped->Item);

		TNode* OldTail = Tail;
		Tail = Popped;
		Tail->Item = FElementType();
		delete OldTail;

		return true;
	}

	/**
	 * Empty the queue, discarding all items.
	 *
	 * @note To be called only from consumer thread.
	 * @see Dequeue, IsEmpty, Peek, Pop
	 */
	void Empty()
	{
		while (Pop());
	}

	/**
	 * Adds an item to the head of the queue.
	 *
	 * @param Item The item to add.
	 * @return true if the item was added, false otherwise.
	 * @note To be called only from producer thread(s).
	 * @see Dequeue, Pop
	 */
	bool Enqueue(const FElementType& Item)
	{
		TNode* NewNode = new TNode(Item);

		if (NewNode == nullptr)
		{
			return false;
		}

		TNode* OldHead;

		if (Mode == EQueueMode::Mpsc)
		{
			OldHead = (TNode*)FPlatformAtomics::InterlockedExchangePtr((void**)&Head, NewNode);
			TSAN_BEFORE(&OldHead->NextNode);
			FPlatformAtomics::InterlockedExchangePtr((void**)&OldHead->NextNode, NewNode);
		}
		else
		{
			OldHead = Head;
			Head = NewNode;
			TSAN_BEFORE(&OldHead->NextNode);
			FPlatformMisc::MemoryBarrier();
			OldHead->NextNode = NewNode;
		}

		return true;
	}

	/**
	 * Adds an item to the head of the queue.
	 *
	 * @param Item The item to add.
	 * @return true if the item was added, false otherwise.
	 * @note To be called only from producer thread(s).
	 * @see Dequeue, Pop
	 */
	bool Enqueue(FElementType&& Item)
	{
		TNode* NewNode = new TNode(MoveTemp(Item));

		if (NewNode == nullptr)
		{
			return false;
		}

		TNode* OldHead;

		if (Mode == EQueueMode::Mpsc)
		{
			OldHead = (TNode*)FPlatformAtomics::InterlockedExchangePtr((void**)&Head, NewNode);
			TSAN_BEFORE(&OldHead->NextNode);
			FPlatformAtomics::InterlockedExchangePtr((void**)&OldHead->NextNode, NewNode);
		}
		else
		{
			OldHead = Head;
			Head = NewNode;
			TSAN_BEFORE(&OldHead->NextNode);
			FPlatformMisc::MemoryBarrier();
			OldHead->NextNode = NewNode;
		}

		return true;
	}

	/**
	 * Checks whether the queue is empty.
	 *
	 * @return true if the queue is empty, false otherwise.
	 * @note To be called only from consumer thread.
	 * @see Dequeue, Empty, Peek, Pop
	 */
	bool IsEmpty() const
	{
		return (Tail->NextNode == nullptr);
	}

	/**
	 * Peeks at the queue's tail item without removing it.
	 *
	 * @param OutItem Will hold the peeked at item.
	 * @return true if an item was returned, false if the queue was empty.
	 * @note To be called only from consumer thread.
	 * @see Dequeue, Empty, IsEmpty, Pop
	 */
	bool Peek(FElementType& OutItem) const
	{
		if (Tail->NextNode == nullptr)
		{
			return false;
		}

		OutItem = Tail->NextNode->Item;

		return true;
	}

	/**
	 * Peek at the queue's tail item without removing it.
	 *
	 * This version of Peek allows peeking at a queue of items that do not allow
	 * copying, such as TUniquePtr.
	 *
	 * @return Pointer to the item, or nullptr if queue is empty
	 */
	FElementType* Peek()
	{
		if (Tail->NextNode == nullptr)
		{
			return nullptr;
		}

		return &Tail->NextNode->Item;
	}

	/**
	 * Peek at the queue's head item
	 *
	 *
	 * @return Pointer to the item, or nullptr if queue is empty
	 */
	FElementType* PeekHead()
	{
		if (Tail->NextNode == nullptr)
		{
			return nullptr;
		}

		return &Head->Item;
	}


	FORCEINLINE const FElementType* Peek() const
	{
		return const_cast<TConvaiQueue*>(this)->Peek();
	}

	/**
	 * Removes the item from the tail of the queue.
	 *
	 * @return true if a value was removed, false if the queue was empty.
	 * @note To be called only from consumer thread.
	 * @see Dequeue, Empty, Enqueue, IsEmpty, Peek
	 */
	bool Pop()
	{
		TNode* Popped = Tail->NextNode;

		if (Popped == nullptr)
		{
			return false;
		}

		TSAN_AFTER(&Tail->NextNode);

		TNode* OldTail = Tail;
		Tail = Popped;
		Tail->Item = FElementType();
		delete OldTail;

		return true;
	}

private:

	/** Structure for the internal linked list. */
	struct TNode
	{
		/** Holds a pointer to the next node in the list. */
		TNode* volatile NextNode;

		/** Holds the node's item. */
		FElementType Item;

		/** Default constructor. */
		TNode()
			: NextNode(nullptr)
		{ }

		/** Creates and initializes a new node. */
		explicit TNode(const FElementType& InItem)
			: NextNode(nullptr)
			, Item(InItem)
		{ }

		/** Creates and initializes a new node. */
		explicit TNode(FElementType&& InItem)
			: NextNode(nullptr)
			, Item(MoveTemp(InItem))
		{ }
	};

	/** Holds a pointer to the head of the list. */
	MS_ALIGN(16) TNode* volatile Head GCC_ALIGN(16);

	/** Holds a pointer to the tail of the list. */
	TNode* Tail;

private:

	/** Hidden copy constructor. */
	TConvaiQueue(const TConvaiQueue&) = delete;

	/** Hidden assignment operator. */
	TConvaiQueue& operator=(const TConvaiQueue&) = delete;
};

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

	void PlayVoiceSynced(uint8* VoiceData, uint32 VoiceDataSize, bool ContainsHeaderData=true, uint32 SampleRate=21000, uint32 NumChannels=1);
	
	void PlayVoiceData(uint8* VoiceData, uint32 VoiceDataSize, bool ContainsHeaderData=true, uint32 SampleRate=21000, uint32 NumChannels=1);

	void PlayVoiceData(uint8* VoiceData, uint32 VoiceDataSize, bool ContainsHeaderData, FAnimationSequence FaceSequence, uint32 SampleRate = 21000, uint32 NumChannels = 1);

	//UFUNCTION(BlueprintCallable, Category = "Convai")
	void ForcePlayVoice(USoundWave* VoiceToPlay);

	void StopVoice();

	void PauseVoice();

	void ResumeVoice();

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

	IConvaiVisionInterface* FindFirstVisionComponent();

	UFUNCTION(BlueprintCallable, Category = "Convai|Vision")
	bool SetVisionComponent(UActorComponent* VisionComponent);

	/** Returns true, if an Vision Component was available and attached to the character */
	UFUNCTION(BlueprintPure, BlueprintCallable, Category = "Convai|Vision")
	bool SupportsVision();

	bool ReplicateVoiceToNetwork;

public:
	// UActorComponent interface
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// UObject Interface.
	virtual void BeginDestroy() override;

public:

	FTimerHandle AudioFinishedTimerHandle;
	FTimerHandle LypSyncTimeoutTimerHandle;
	bool IsTalking = false;
	float TotalVoiceFadeOutTime;
	float RemainingVoiceFadeOutTime;

	UPROPERTY()
	USoundWaveProcedural* SoundWaveProcedural;

	TArray<uint8> AudioDataBuffer;
	TArray<uint8> ReceivedEncodedAudioDataBuffer;
 
	IConvaiLipSyncInterface* ConvaiLipSync;
	IConvaiVisionInterface* ConvaiVision;

	void PlayLipSyncWithPrecomputedFacialAnimationSynced(FAnimationSequence& FaceSequence);

	void PlayLipSyncWithPrecomputedFacialAnimation(FAnimationSequence FaceSequence);

	void PlayLipSync(uint8* InPCMData, uint32 InPCMDataSize, uint32 InSampleRate, uint32 InNumChannels);

	void StopLipSync();

	void PauseLipSync();

	void ResumeLipSync();

	virtual bool CanUseLipSync();

	virtual void ForceRecalculateLipsyncStartTime();

	virtual bool CanUseVision();

	void OnVisemesReadyCallback();

	void OnLipSyncTimeOut();

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


// Add to public section
enum class EAudioLipSyncState : uint8
{
    Stopped UMETA(DisplayName = "Stopped"),
    Playing UMETA(DisplayName = "Playing"),
    WaitingOnLipSync UMETA(DisplayName = "Waiting On LipSync"),
    WaitingOnAudio UMETA(DisplayName = "Waiting On Audio")
};

// Add to protected section
EAudioLipSyncState CurrentState;

// Simplified buffer structure
struct FAudioBuffer
{
    TArray<uint8> Data;
    float Duration;
    uint32 SampleRate;
    uint32 NumChannels;
    
    FAudioBuffer() : Duration(0.0f), SampleRate(0), NumChannels(0) {}
    
    void Reset()
    {
        Data.Empty();
        Duration = 0.0f;
        SampleRate = 0;
        NumChannels = 0;
    }
    
    bool IsEmpty() const { return Data.Num() == 0; }
    
    float GetTotalDuration() const { return Duration; }
};

struct FLipSyncBuffer
{
    TArray<FAnimationSequence> Sequences;
    float TotalDuration;
    
    FLipSyncBuffer() : TotalDuration(0.0f) {}
    
    void Reset()
    {
        Sequences.Empty();
        TotalDuration = 0.0f;
    }
    
    bool IsEmpty() const { return Sequences.Num() == 0; }
    
    float GetTotalDuration() const { return TotalDuration; }
    
    void AddSequence(const FAnimationSequence& Sequence)
    {
        Sequences.Add(Sequence);
        TotalDuration += Sequence.Duration;
    }
};

FAudioBuffer AudioBuffer;
FLipSyncBuffer LipSyncBuffer;

// Configuration parameters
float MinBufferDuration;
float AudioLipSyncRatio;

// State management functions
void TransitionToState(EAudioLipSyncState NewState);
void HandleAudioReceived(uint8* AudioData, uint32 AudioDataSize, bool ContainsHeaderData, uint32 SampleRate, uint32 NumChannels);
void HandleLipSyncReceived(FAnimationSequence& FaceSequence);
bool TryPlayBufferedContent();
bool HasSufficientLipSync();
bool HasSufficientAudio() const;
void PlayBufferedContent(float Duration);

/**
 * Returns the duration of content (audio and lipsync if applicable) that is 
 * currently playing or buffered and ready to play.
 * @return Duration in seconds of content remaining
 */
UFUNCTION(BlueprintPure, Category = "Convai|Audio")
float GetRemainingContentDuration();

// Add to protected section
// Tracking variables for content
float TotalPlayingDuration;       // Total duration of content currently being played
float TotalBufferedDuration;      // Total duration of content buffered but not yet played
bool bIsSyncingAudioAndLipSync;   // Whether we're syncing audio and lipsync

protected:

	float LipSyncThresholdSecs = -1;
	float VoiceTimeFactor = -1;

private:

	// Critical section for protecting SoundWaveProcedural operations
	FCriticalSection AudioConfigLock;
	FThreadSafeBool IsAudioConfiguring;
	
	// Buffer for pending audio data when lock is held
	TArray<uint8> PendingAudioBuffer;
	
	// Process any pending audio data
	void ProcessPendingAudio();

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
