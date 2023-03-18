// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/AudioComponent.h"

#include "VoiceCaptureComp.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(ConvaiVoiceCapLog, Log, All);


class IVoiceCapture;

UCLASS(meta = (BlueprintSpawnableComponent), Deprecated)
class UDEPRECATED_ConvaiVoiceComp : public UActorComponent
{
	GENERATED_BODY()

		UDEPRECATED_ConvaiVoiceComp();


	void Init(int voiceSampleRate = 24000);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// This class "ConvaiVoiceComp" is deprecated, Use Convai Player Component for recording instead
	UFUNCTION(BlueprintCallable, Category = "Convai|Microphone", meta = (DisplayName = "DEPRECATED Start Recording"/*, DeprecatedFunction, DeprecationMessage = "This class \"ConvaiVoiceComp\" is deprecated, Use Convai Player Component for recording instead"*/)  )
	void StartRecording();

	// This class "ConvaiVoiceComp" is deprecated, Use Convai Player Component for recording instead
	UFUNCTION(BlueprintCallable, Category = "Convai|Microphone", meta = (DisplayName = "DEPRECATED Finish Recording"/*, DeprecatedFunction, DeprecationMessage = "This class \"ConvaiVoiceComp\" is deprecated, Use Convai Player Component for recording instead"*/) )
	USoundWave* FinishRecording();

	TSharedPtr<IVoiceCapture> VoiceCapture;
	TArray<uint8> VoiceCaptureBuffer;

	bool IsRecording = false;
	bool IsInit = false;
};