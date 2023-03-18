// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
//#include "ConvaiAudioStreamer.h"
#include "LipSyncInterface.generated.h"

DECLARE_DELEGATE(FOnVisemesDataReadySignature);

UINTERFACE()
class UConvaiLipSyncInterface : public UInterface
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
