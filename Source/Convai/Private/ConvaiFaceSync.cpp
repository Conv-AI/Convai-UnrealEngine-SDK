// Copyright 2022 Convai Inc. All Rights Reserved.

#include "ConvaiFaceSync.h"
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
		CurrentSequenceTimePassed += DeltaTime;

		if (CurrentSequenceTimePassed > MainSequenceBuffer.Duration && Stopping)
		{
			CurrentSequenceTimePassed = 0;
			ClearMainSequence();
			SetCurrentFrametoZero();
			OnVisemesDataReady.ExecuteIfBound();
			Stopping = false;
			SequenceCriticalSection.Unlock();
			return;
		}
		else if (CurrentSequenceTimePassed > MainSequenceBuffer.Duration && !Stopping)
		{
			Stopping = true;
			ConvaiStopLipSync();
		}

		// Calculate frame duration and offsets
		float FrameDuration = MainSequenceBuffer.Duration / MainSequenceBuffer.AnimationFrames.Num();
		float FrameOffset = FrameDuration * 0.5f;

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
		}
		else if (CurrentSequenceTimePassed >= MainSequenceBuffer.Duration - FrameOffset)
		{
			int LastFrameIdx = MainSequenceBuffer.AnimationFrames.Num() - 1;
			StartFrame = MainSequenceBuffer.AnimationFrames[LastFrameIdx].BlendShapes;
			EndFrame = GenerateZeroFrame();
			Alpha = (CurrentSequenceTimePassed - (MainSequenceBuffer.Duration - FrameOffset)) / FrameOffset;
		}
		else
		{
			int CurrentFrameIndex = FMath::FloorToInt((CurrentSequenceTimePassed - FrameOffset) / FrameDuration);
			CurrentFrameIndex = FMath::Min(CurrentFrameIndex, MainSequenceBuffer.AnimationFrames.Num() - 1);
			int NextFrameIndex = FMath::Min(CurrentFrameIndex + 1, MainSequenceBuffer.AnimationFrames.Num() - 1);
			StartFrame = MainSequenceBuffer.AnimationFrames[CurrentFrameIndex].BlendShapes;
			EndFrame = MainSequenceBuffer.AnimationFrames[NextFrameIndex].BlendShapes;
			Alpha = (CurrentSequenceTimePassed - FrameOffset - (CurrentFrameIndex * FrameDuration)) / FrameDuration;
		}

		//AnchorValue = FMath::Clamp(AnchorValue, 0, 1);
		//Alpha = Calculate1DBezierCurve(Alpha, AnchorValue,0, 1-AnchorValue, 1);

		CurrentBlendShapesMap = InterpolateFrames(StartFrame, EndFrame, Alpha);

		// Trigger the blueprint event
		OnVisemesDataReady.ExecuteIfBound();
		SequenceCriticalSection.Unlock();
	}	
}

void UConvaiFaceSyncComponent::ConvaiProcessLipSyncAdvanced(uint8* InPCMData, uint32 InPCMDataSize, uint32 InSampleRate, uint32 InNumChannels, FAnimationSequence FaceSequence)
{
	SequenceCriticalSection.Lock();
	MainSequenceBuffer.AnimationFrames.Append(FaceSequence.AnimationFrames);
	MainSequenceBuffer.Duration += FaceSequence.Duration;
	SequenceCriticalSection.Unlock();
}

void UConvaiFaceSyncComponent::ConvaiProcessLipSyncSingleFrame(FAnimationFrame FaceFrame, float Duration)
{
	SequenceCriticalSection.Lock();
	if (Stopping)
	{
		CurrentSequenceTimePassed = 0;
		ClearMainSequence();
		Stopping = false;
	}

	if (!GeneratesVisemesAsBlendshapes())
	{
		float* sil = FaceFrame.BlendShapes.Find("sil");
		if (sil != nullptr && *sil < 0)
		{
			ClearMainSequence();
			return;
		}
	}

	MainSequenceBuffer.AnimationFrames.Add(FaceFrame);
	MainSequenceBuffer.Duration += Duration;
	SequenceCriticalSection.Unlock();
}

bool UConvaiFaceSyncComponent::IsValidSequence(FAnimationSequence Sequence)
{
	if (Sequence.Duration > 0 && Sequence.AnimationFrames.Num() > 0 && Sequence.AnimationFrames[0].BlendShapes.Num() > 0)
		return true;
	else
		return false;
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
	CurrentSequenceTimePassed = 0;
	ClearMainSequence();

	FAnimationFrame CurrentFrame = FAnimationFrame();
	CurrentFrame.BlendShapes = CurrentBlendShapesMap;
	FAnimationSequence StoppingSequence;
	StoppingSequence.Duration = 0.2;
	StoppingSequence.AnimationFrames.Add(CurrentFrame);
	ConvaiProcessLipSyncAdvanced(nullptr, 0, 0, 0, StoppingSequence);
}
