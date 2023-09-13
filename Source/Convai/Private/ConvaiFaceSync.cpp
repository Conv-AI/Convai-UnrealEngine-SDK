// Copyright 2022 Convai Inc. All Rights Reserved.

#include "ConvaiFaceSync.h"


DEFINE_LOG_CATEGORY(ConvaiFaceSyncLog);

namespace
{
	// Helper function: Interpolates between two given blendshape sets
	TMap<FName, float> InterpolateBlendshapes(
		const TMap<FName, float>& StartBlendShapes,
		const TMap<FName, float>& EndBlendShapes,
		float Alpha)
	{
		TMap<FName, float> Result;
		for (const auto& BlendShapeName : UConvaiFaceSyncComponent::BlendShapesNames)
		{
			float StartValue = StartBlendShapes.Contains(BlendShapeName) ? StartBlendShapes[BlendShapeName] : 0.0;
			float EndValue = EndBlendShapes.Contains(BlendShapeName) ? EndBlendShapes[BlendShapeName] : 0.0;
			Result.Add(BlendShapeName, FMath::Lerp(StartValue, EndValue, Alpha));
		}
		return Result;
	}

	// Helper function: Creates a zero blendshapes map
	TMap<FName, float> CreateZeroBlendshapes()
	{
		TMap<FName, float> ZeroBlendshapes;
		for (const auto& BlendShapeName : UConvaiFaceSyncComponent::BlendShapesNames)
		{
			ZeroBlendshapes.Add(BlendShapeName, 0.0);
		}
		return ZeroBlendshapes;
	}

	float Calculate1DBezierCurve(float t, float P0, float P1, float P2, float P3)
	{
		// Make sure t is in the range [0, 1]
		t = FMath::Clamp(t, 0.0f, 1.0f);

		// Calculate the 1D Bezier curve value at time t
		float result = FMath::Pow(1.0f - t, 3.0f) * P0
			+ 3.0f * FMath::Pow(1.0f - t, 2.0f) * t * P1
			+ 3.0f * (1.0f - t) * FMath::Pow(t, 2.0f) * P2
			+ FMath::Pow(t, 3.0f) * P3;

		return result;
	}

};

const TArray<FName> UConvaiFaceSyncComponent::BlendShapesNames = { "EyeBlinkLeft", "EyeLookDownLeft", "EyeLookInLeft", "EyeLookOutLeft", "EyeLookUpLeft", "EyeSquintLeft", "EyeWideLeft", "EyeBlinkRight", "EyeLookDownRight", "EyeLookInRight", "EyeLookOutRight", "EyeLookUpRight", "EyeSquintRight", "EyeWideRight", "JawForward", "JawLeft", "JawRight", "JawOpen", "MouthClose", "MouthFunnel", "MouthPucker", "MouthLeft", "MouthRight", "MouthSmileLeft", "MouthSmileRight", "MouthFrownLeft", "MouthFrownRight", "MouthDimpleLeft", "MouthDimpleRight", "MouthStretchLeft", "MouthStretchRight", "MouthRollLower", "MouthRollUpper", "MouthShrugLower", "MouthShrugUpper", "MouthPressLeft", "MouthPressRight", "MouthLowerDownLeft", "MouthLowerDownRight", "MouthUpperUpLeft", "MouthUpperUpRight", "BrowDownLeft", "BrowDownRight", "BrowInnerUp", "BrowOuterUpLeft", "BrowOuterUpRight", "CheekPuff", "CheekSquintLeft", "CheekSquintRight", "NoseSneerLeft", "NoseSneerRight", "TongueOut", "HeadRoll", "HeadPitch", "HeadYaw" };
const TMap<FName, float> UConvaiFaceSyncComponent::ZeroFrame = CreateZeroBlendshapes();


UConvaiFaceSyncComponent::UConvaiFaceSyncComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	CurrentSequenceTimePassed = 0;
	CurrentBlendShapes = ZeroFrame;
}

UConvaiFaceSyncComponent::~UConvaiFaceSyncComponent() // Implement destructor
{

}

void UConvaiFaceSyncComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	// Interpolate blendshapes and advance animation sequence
	if (Sequences.Num() > 0)
	{
		CurrentSequenceTimePassed += DeltaTime;
		FAnimationSequence& CurrentSequence = Sequences[0];

		if (CurrentSequenceTimePassed > CurrentSequence.Duration)
		{
			CurrentSequenceTimePassed -= CurrentSequence.Duration;
			Sequences.RemoveAt(0);

			if (Sequences.Num() == 0)
			{
				CurrentSequenceTimePassed = 0;
				CurrentBlendShapes = ZeroFrame;
				OnVisemesDataReady.ExecuteIfBound();
				return;
			}
			else
			{
				CurrentSequence = Sequences[0];
			}
		}

		// If sequence is invalid - wait it out until its duration is done
		if (!IsValidSequence(CurrentSequence))
		{
			return;
		}

		// Calculate frame duration and offsets
		float FrameDuration = CurrentSequence.Duration / CurrentSequence.AnimationFrames.Num();
		float FrameOffset = FrameDuration * 0.5f;

		TMap<FName, float> StartBlendShapes;
		TMap<FName, float> EndBlendShapes;
		float Alpha;

		// Choose the current and next BlendShapes
		if (CurrentSequenceTimePassed <= FrameOffset)
		{
			//StartBlendShapes = ZeroFrame;
			StartBlendShapes = CurrentBlendShapes;
			EndBlendShapes = CurrentSequence.AnimationFrames[0].BlendShapes;
			Alpha = CurrentSequenceTimePassed / FrameOffset + 0.5;
		}
		else if (CurrentSequenceTimePassed >= CurrentSequence.Duration - FrameOffset)
		{
			int LastFrameIdx = CurrentSequence.AnimationFrames.Num() - 1;
			StartBlendShapes = CurrentSequence.AnimationFrames[LastFrameIdx].BlendShapes;

			if (Sequences.Num() > 1 && IsValidSequence(Sequences[1]))
			{
				Alpha = (CurrentSequenceTimePassed - (CurrentSequence.Duration - FrameOffset)) / FrameOffset;
				EndBlendShapes = InterpolateBlendshapes(ZeroFrame, Sequences[1].AnimationFrames[0].BlendShapes, 0.5);
			}
			else
			{
				EndBlendShapes = ZeroFrame;
				Alpha = (CurrentSequenceTimePassed - (CurrentSequence.Duration - FrameOffset)) / FrameOffset;
			}
		}
		else
		{
			int CurrentFrameIndex = FMath::FloorToInt((CurrentSequenceTimePassed - FrameOffset) / FrameDuration);
			CurrentFrameIndex = FMath::Min(CurrentFrameIndex, CurrentSequence.AnimationFrames.Num() - 1);
			int NextFrameIndex = FMath::Min(CurrentFrameIndex + 1, CurrentSequence.AnimationFrames.Num() - 1);
			StartBlendShapes = CurrentSequence.AnimationFrames[CurrentFrameIndex].BlendShapes;
			EndBlendShapes = CurrentSequence.AnimationFrames[NextFrameIndex].BlendShapes;
			Alpha = (CurrentSequenceTimePassed - FrameOffset - (CurrentFrameIndex * FrameDuration)) / FrameDuration;
		}

		AnchorValue = FMath::Clamp(AnchorValue, 0, 1);
		Alpha = Calculate1DBezierCurve(Alpha, AnchorValue,0, 1-AnchorValue, 1);

		CurrentBlendShapes = InterpolateBlendshapes(StartBlendShapes, EndBlendShapes, Alpha);

		// Trigger the blueprint event
		OnVisemesDataReady.ExecuteIfBound();
	}
}

void UConvaiFaceSyncComponent::ConvaiProcessLipSyncAdvanced(uint8* InPCMData, uint32 InPCMDataSize, uint32 InSampleRate, uint32 InNumChannels, FAnimationSequence FaceSequence)
{
	Sequences.Add(FaceSequence);
}

bool UConvaiFaceSyncComponent::IsValidSequence(FAnimationSequence Sequence)
{
	if (Sequence.Duration > 0 && Sequence.AnimationFrames.Num() > 0 && Sequence.AnimationFrames[0].BlendShapes.Num() > 0)
		return true;
	else
		return false;
}

void UConvaiFaceSyncComponent::ConvaiStopLipSync()
{
	CurrentSequenceTimePassed = 0;
	Sequences.Empty(); // Clear any remaining sequences

	FAnimationFrame CurrentFrame = FAnimationFrame();
	CurrentFrame.BlendShapes = CurrentBlendShapes;

	FAnimationSequence StoppingSequence;
	StoppingSequence.Duration = 0.2;
	StoppingSequence.AnimationFrames.Add(CurrentFrame);

	Sequences.Add(StoppingSequence);
}