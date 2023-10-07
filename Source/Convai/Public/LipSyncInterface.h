// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ConvaiDefinitions.h"
#include "LipSyncInterface.generated.h"

DECLARE_DELEGATE(FOnVisemesDataReadySignature);

UINTERFACE()
class CONVAI_API UConvaiLipSyncInterface : public UInterface
{
	GENERATED_BODY()
};

class IConvaiLipSyncInterface
{
	GENERATED_BODY()

public:
	FOnVisemesDataReadySignature OnVisemesDataReady;

	virtual void ConvaiProcessLipSync(uint8* InPCMData, uint32 InPCMDataSize, uint32 InSampleRate, uint32 InNumChannels) = 0;
	virtual void ConvaiStopLipSync() = 0;
	virtual TArray<float> ConvaiGetVisemes() = 0;
	virtual TArray<FString> ConvaiGetVisemeNames() = 0;
};

UINTERFACE()
class CONVAI_API UConvaiLipSyncExtendedInterface : public UConvaiLipSyncInterface
{
	GENERATED_BODY()
};

class IConvaiLipSyncExtendedInterface : public IConvaiLipSyncInterface
{
	GENERATED_BODY()

public:
	virtual void ConvaiProcessLipSyncAdvanced(uint8* InPCMData, uint32 InPCMDataSize, uint32 InSampleRate, uint32 InNumChannels, FAnimationSequence FaceSequence) = 0;
	virtual void ConvaiProcessLipSyncSingleFrame(FAnimationFrame FaceFrame, float Duration) = 0;
	virtual bool RequiresPreGeneratedFaceData() = 0;
	virtual bool GeneratesVisemesAsBlendshapes() = 0;
	virtual TMap<FName, float> ConvaiGetFaceBlendshapes() = 0;
};