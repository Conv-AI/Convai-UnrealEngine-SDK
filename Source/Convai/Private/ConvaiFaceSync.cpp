// Copyright 2022 Convai Inc. All Rights Reserved.

#include "ConvaiFaceSync.h"
#include "Misc/ScopeLock.h"
#include "ConvaiUtils.h"

DEFINE_LOG_CATEGORY(ConvaiFaceSyncLog);

namespace
{
	// Helper function: Creates a zero blendshapes map
	TMap<FName, float> CreateZeroBlendshapes()
	{
		TMap<FName, float> ZeroBlendshapes;
		for (const auto& BlendShapeName : ConvaiConstants::BlendShapesNames)
		{
			ZeroBlendshapes.Add(*BlendShapeName, 0.0);
		}
		return ZeroBlendshapes;
	}

	// Helper function: Creates a zero visemes map
	TMap<FName, float> CreateZeroVisemes()
	{
		TMap<FName, float> ZeroVisemes;
		for (const auto& VisemeName : ConvaiConstants::VisemeNames)
		{
			ZeroVisemes.Add(*VisemeName, 0.0);
		}
		ZeroVisemes["sil"] = 1;
		return ZeroVisemes;
	}


	float Calculate1DBezierCurve(float t, float P0, float P1, float P2, float P3)
	{
		// Make sure t is in the range [0, 1]
		t = FMath::Clamp(t, 0.0f, 1.0f);

		// Calculate the 1D Bezier curve value at time t
		float result = FMath::Pow(1.0f - t, 3.0f) * P0 + 3.0f * FMath::Pow(1.0f - t, 2.0f) * t * P1 +
			3.0f * (1.0f - t) * FMath::Pow(t, 2.0f) * P2 + FMath::Pow(t, 3.0f) * P3;

		return result;
	}
};


const TMap<FName, float> UConvaiFaceSyncComponent::ZeroBlendshapeFrame = CreateZeroBlendshapes();
const TMap<FName, float> UConvaiFaceSyncComponent::ZeroVisemeFrame = CreateZeroVisemes();

UConvaiFaceSyncComponent::UConvaiFaceSyncComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	CurrentSequenceTimePassed = 0;
	//CurrentBlendShapesMap = ZeroBlendshapeFrame;
}

UConvaiFaceSyncComponent::~UConvaiFaceSyncComponent() // Implement destructor
{

}

void UConvaiFaceSyncComponent::BeginPlay()
{
	Super::BeginPlay();
	CurrentBlendShapesMap = GenerateZeroFrame();
}

void UConvaiFaceSyncComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Interpolate blendshapes and advance animation sequence
	if (IsValidSequence(MainSequenceBuffer))
	{
		SequenceCriticalSection.Lock();

		// Calculate time passed since ConvaiProcessLipSyncAdvanced was called
		double CurrentTime = FPlatformTime::Seconds();
		CurrentSequenceTimePassed = CurrentTime - StartTime;

		if (CurrentSequenceTimePassed > MainSequenceBuffer.Duration)
		{
			ConvaiStopLipSync();
			SequenceCriticalSection.Unlock();
			return;
		}

		bIsPlaying = true;

		// Calculate frame duration and offsets
		float FrameDuration = MainSequenceBuffer.Duration / MainSequenceBuffer.AnimationFrames.Num();
		float FrameOffset = FrameDuration * 0.5f;
		int32 FrameIndex;
		int32 BufferIndex;

		TMap<FName, float> StartFrame;
		TMap<FName, float> EndFrame;
		float Alpha;

		// Choose the current and next BlendShapes
		if (CurrentSequenceTimePassed <= FrameOffset)
		{
			//StartFrame = ZeroBlendshapeFrame;
			StartFrame = GetCurrentFrame();
			EndFrame = MainSequenceBuffer.AnimationFrames[0].BlendShapes;
			Alpha = CurrentSequenceTimePassed / FrameOffset + 0.5;
			FrameIndex = MainSequenceBuffer.AnimationFrames[0].FrameIndex;
			BufferIndex = 0;
		}
		else if (CurrentSequenceTimePassed >= MainSequenceBuffer.Duration - FrameOffset)
		{
			int LastFrameIdx = MainSequenceBuffer.AnimationFrames.Num() - 1;
			StartFrame = MainSequenceBuffer.AnimationFrames[LastFrameIdx].BlendShapes;
			EndFrame = GenerateZeroFrame();
			Alpha = (CurrentSequenceTimePassed - (MainSequenceBuffer.Duration - FrameOffset)) / FrameOffset;
			FrameIndex = MainSequenceBuffer.AnimationFrames[LastFrameIdx].FrameIndex;
			BufferIndex = LastFrameIdx;
		}
		else
		{
			int CurrentFrameIndex = FMath::FloorToInt((CurrentSequenceTimePassed - FrameOffset) / FrameDuration);
			CurrentFrameIndex = FMath::Min(CurrentFrameIndex, MainSequenceBuffer.AnimationFrames.Num() - 1);
			int NextFrameIndex = FMath::Min(CurrentFrameIndex + 1, MainSequenceBuffer.AnimationFrames.Num() - 1);
			StartFrame = MainSequenceBuffer.AnimationFrames[CurrentFrameIndex].BlendShapes;
			EndFrame = MainSequenceBuffer.AnimationFrames[NextFrameIndex].BlendShapes;
			Alpha = (CurrentSequenceTimePassed - FrameOffset - (CurrentFrameIndex * FrameDuration)) / FrameDuration;

			Apply_StartEndFrames_PostProcessing(CurrentFrameIndex, NextFrameIndex, Alpha, StartFrame, EndFrame);

			FrameIndex = MainSequenceBuffer.AnimationFrames[CurrentFrameIndex].FrameIndex;
			BufferIndex = CurrentFrameIndex;
		}
		SequenceCriticalSection.Unlock();

		// UE_LOG(ConvaiFaceSyncLog, Log, TEXT("Evaluate: FrameIndex:%d Alpha: %f FramesLeft: %d"), FrameIndex, Alpha, MainSequenceBuffer.AnimationFrames.Num() - BufferIndex);

		//CurrentBlendShapesMap = StartFrame;
		CurrentBlendShapesMap = InterpolateFrames(StartFrame, EndFrame, Alpha);

		ApplyPostProcessing();

		// Trigger the blueprint event
		OnVisemesDataReady.ExecuteIfBound();
	}
}

void UConvaiFaceSyncComponent::ConvaiProcessLipSyncAdvanced(uint8* InPCMData, uint32 InPCMDataSize, uint32 InSampleRate, uint32 InNumChannels, FAnimationSequence FaceSequence)
{
	SequenceCriticalSection.Lock();
	MainSequenceBuffer.AnimationFrames.Append(FaceSequence.AnimationFrames);
	MainSequenceBuffer.Duration += FaceSequence.Duration;
	MainSequenceBuffer.FrameRate = FaceSequence.FrameRate;
	CalculateStartingTime();
	SequenceCriticalSection.Unlock();

	if (IsRecordingLipSync)
	{
		FScopeLock ScopeLock(&RecordingCriticalSection);
		RecordedSequenceBuffer.AnimationFrames.Append(FaceSequence.AnimationFrames);
		RecordedSequenceBuffer.Duration += FaceSequence.Duration;
		RecordedSequenceBuffer.FrameRate = FaceSequence.FrameRate;
	}
}

void UConvaiFaceSyncComponent::ConvaiProcessLipSyncSingleFrame(FAnimationFrame FaceFrame, float Duration)
{
	SequenceCriticalSection.Lock();
	if (!GeneratesVisemesAsBlendshapes())
	{
		float* sil = FaceFrame.BlendShapes.Find("sil");
		if (sil != nullptr && *sil < 0)
		{
			ClearMainSequence();
			Stopping = true;
			return;
		}
	}

	MainSequenceBuffer.AnimationFrames.Add(FaceFrame);
	MainSequenceBuffer.Duration += Duration;
	MainSequenceBuffer.FrameRate = Duration==0? -1 : 1.0/Duration;
	SequenceCriticalSection.Unlock();

	if (IsRecordingLipSync)
	{
		FScopeLock ScopeLock(&RecordingCriticalSection);
		RecordedSequenceBuffer.AnimationFrames.Add(FaceFrame);
		RecordedSequenceBuffer.Duration += Duration;
		RecordedSequenceBuffer.FrameRate = Duration == 0 ? -1 : 1.0 / Duration;
	}
}

void UConvaiFaceSyncComponent::StartRecordingLipSync()
{
	if (IsRecordingLipSync)
	{
		UE_LOG(ConvaiFaceSyncLog, Warning, TEXT("Cannot start Recording LipSync while already recording LipSync"));
		return;
	}

	UE_LOG(ConvaiFaceSyncLog, Log, TEXT("Started Recording LipSync"));
	IsRecordingLipSync = true;
}

FAnimationSequenceBP UConvaiFaceSyncComponent::FinishRecordingLipSync()
{
	UE_LOG(ConvaiFaceSyncLog, Log, TEXT("Finished Recording LipSync - Total Frames: %d - Duration: %f"), RecordedSequenceBuffer.AnimationFrames.Num(), RecordedSequenceBuffer.Duration);

	if (!IsRecordingLipSync)
		return FAnimationSequenceBP();

	FScopeLock ScopeLock(&RecordingCriticalSection);
	IsRecordingLipSync = false;

	FAnimationSequenceBP AnimationSequenceBP;
	AnimationSequenceBP.AnimationSequence = RecordedSequenceBuffer;
	RecordedSequenceBuffer.AnimationFrames.Empty();
	RecordedSequenceBuffer.Duration = 0;
	return AnimationSequenceBP;
}

bool UConvaiFaceSyncComponent::PlayRecordedLipSync(FAnimationSequenceBP RecordedLipSync, int StartFrame, int EndFrame, float OverwriteDuration)
{
	if (!IsValidSequence(RecordedLipSync.AnimationSequence))
	{
		UE_LOG(ConvaiFaceSyncLog, Warning, TEXT("Recorded LipSync is not valid - Total Frames: %d - Duration: %f"), RecordedLipSync.AnimationSequence.AnimationFrames.Num(), RecordedLipSync.AnimationSequence.Duration);
		return false;
	}

	if (IsRecordingLipSync)
	{
		UE_LOG(ConvaiFaceSyncLog, Warning, TEXT("Cannot Play Recorded LipSync while Recording LipSync"));
		return false;
	}

	if (IsValidSequence(MainSequenceBuffer))
	{
		UE_LOG(ConvaiFaceSyncLog, Warning, TEXT("Playing Recorded LipSync and stopping currently playing LipSync"));
		ConvaiStopLipSync();
	}

	if (StartFrame > 0 && StartFrame > RecordedLipSync.AnimationSequence.AnimationFrames.Num() - 1)
	{
		UE_LOG(ConvaiFaceSyncLog, Warning, TEXT("StartFrame is greater than the recorded LipSync - StartFrame: %d Total Frames: %d - Duration: %f"), StartFrame, RecordedLipSync.AnimationSequence.AnimationFrames.Num(), RecordedLipSync.AnimationSequence.Duration);
		return false;
	}


	if (EndFrame > 0 && EndFrame < StartFrame)
	{
		UE_LOG(ConvaiFaceSyncLog, Warning, TEXT("StartFrame cannot be greater than the EndFrame"));
		return false;
	}

	//if (EndFrame > 0 && EndFrame > RecordedLipSync.AnimationSequence.AnimationFrames.Num() - 1)
	//{
	//	UE_LOG(ConvaiFaceSyncLog, Warning, TEXT("EndFrame is greater than the recorded LipSync - EndFrame: %d Total Frames: %d - Duration: %f"), EndFrame, RecordedLipSync.AnimationSequence.AnimationFrames.Num(), RecordedLipSync.AnimationSequence.Duration);
	//	return false;
	//}
	
	if (EndFrame > 0 && EndFrame < RecordedLipSync.AnimationSequence.AnimationFrames.Num()-1)
	{
		float FrameDuration = RecordedLipSync.AnimationSequence.Duration / RecordedLipSync.AnimationSequence.AnimationFrames.Num();
		int NumRemovedFrames = RecordedLipSync.AnimationSequence.AnimationFrames.Num() - EndFrame - 1;
		RecordedLipSync.AnimationSequence.AnimationFrames.RemoveAt(EndFrame + 1, NumRemovedFrames);
		float RemovedDuration = NumRemovedFrames * FrameDuration;
		RecordedLipSync.AnimationSequence.Duration -= RemovedDuration;
	}

	if (StartFrame > 0)
	{
		float FrameDuration = RecordedLipSync.AnimationSequence.Duration / RecordedLipSync.AnimationSequence.AnimationFrames.Num();
		int NumRemovedFrames = StartFrame;
		RecordedLipSync.AnimationSequence.AnimationFrames.RemoveAt(0, NumRemovedFrames);
		float RemovedDuration = NumRemovedFrames * FrameDuration;
		RecordedLipSync.AnimationSequence.Duration -= RemovedDuration;
	}

	if (OverwriteDuration > 0)
	{
		RecordedLipSync.AnimationSequence.Duration = OverwriteDuration;
	}

	UE_LOG(ConvaiFaceSyncLog, Log, TEXT("Playing Recorded LipSync - Total Frames: %d - Duration: %f"), RecordedLipSync.AnimationSequence.AnimationFrames.Num(), RecordedLipSync.AnimationSequence.Duration);
	ConvaiProcessLipSyncAdvanced(nullptr, 0, 0, 0, RecordedLipSync.AnimationSequence);
	return true;
}

bool UConvaiFaceSyncComponent::IsValidSequence(const FAnimationSequence& Sequence)
{
	if (Sequence.Duration > 0 && Sequence.AnimationFrames.Num() > 0 && Sequence.AnimationFrames[0].BlendShapes.Num() > 0)
		return true;
	else
		return false;
}

bool UConvaiFaceSyncComponent::IsPlaying()
{
	return bIsPlaying;
}

void UConvaiFaceSyncComponent::CalculateStartingTime()
{
	if (!bIsPlaying)
		StartTime = FPlatformTime::Seconds();
}

void UConvaiFaceSyncComponent::ClearMainSequence()
{
	SequenceCriticalSection.Lock();
	MainSequenceBuffer.AnimationFrames.Empty();
	MainSequenceBuffer.Duration = 0;
	SequenceCriticalSection.Unlock();
}

TMap<FName, float> UConvaiFaceSyncComponent::InterpolateFrames(const TMap<FName, float>& StartFrame, const TMap<FName, float>& EndFrame, float Alpha)
{
	const TArray<FString>& CurveNames = GeneratesVisemesAsBlendshapes() ? ConvaiConstants::BlendShapesNames : ConvaiConstants::VisemeNames;
	TMap<FName, float> Result;
	for (const auto& CurveName : CurveNames)
	{
		float StartValue = StartFrame.Contains(*CurveName) ? StartFrame[*CurveName] : 0.0;
		float EndValue = EndFrame.Contains(*CurveName) ? EndFrame[*CurveName] : 0.0;
		Result.Add(*CurveName, FMath::Lerp(StartValue, EndValue, Alpha));
	}
	return Result;
}

void UConvaiFaceSyncComponent::ConvaiStopLipSync()
{
	bIsPlaying = false;
	CurrentSequenceTimePassed = 0;
	ClearMainSequence();
	SetCurrentFrametoZero();
	OnVisemesDataReady.ExecuteIfBound();
	// UE_LOG(ConvaiFaceSyncLog, Warning, TEXT("Stopping LipSync"));
}
