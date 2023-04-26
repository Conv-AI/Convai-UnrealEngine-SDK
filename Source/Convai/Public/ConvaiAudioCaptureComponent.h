// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AudioCaptureComponent.h"
#include "ConvaiAudioCaptureComponent.generated.h"

/**
 * 
 */
UCLASS(meta = (BlueprintSpawnableComponent), DisplayName = "Conv Audio Capture")
class UConvaiAudioCaptureComponent : public UAudioCaptureComponent
{
	GENERATED_BODY()

protected:

	//~ Begin USynthComponent interface
	virtual bool Init(int32& SampleRate) override;
	
public:
	void PrintDebug();
};
