// Copyright 2022 Convai Inc. All Rights Reserved.


#include "VoiceCaptureComp.h"
#include "ConvaiUtils.h"
#include "Misc/FileHelper.h"
#include "Http.h"
#include "ConvaiUtils.h"
#include "VoiceModule.h"
#include "Containers/UnrealString.h"
#include "Kismet/GameplayStatics.h"

#include "Sound/SoundWave.h"

DEFINE_LOG_CATEGORY(ConvaiVoiceCapLog);


UDEPRECATED_ConvaiVoiceComp::UDEPRECATED_ConvaiVoiceComp()
{
	PrimaryComponentTick.bCanEverTick = true;
	bAutoActivate = true;
}



//void UVoiceChatComp::BeginPlay()
//{
//	Super::BeginPlay();
//
//
//}




void UDEPRECATED_ConvaiVoiceComp::Init(int voiceSampleRate)
{
	if (VoiceCapture)
	{
		UE_LOG(ConvaiVoiceCapLog, Warning, TEXT("VoiceCapture is already init"));
		return;
	}

	FString commandMicNoiseGateThreshold = "voice.MicNoiseGateThreshold  0.01";
	FString commandSilenceDetectionThreshold = "voice.SilenceDetectionThreshold 0.01";

	APlayerController* PController = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (PController)
	{
		PController->ConsoleCommand(*commandMicNoiseGateThreshold, true);
		PController->ConsoleCommand(*commandSilenceDetectionThreshold, true);
	}

	VoiceCapture = FVoiceModule::Get().CreateVoiceCapture("", voiceSampleRate, 1);
	//FVoiceModule::Get().set
	if (VoiceCapture.IsValid())
	{
		//UE_LOG(ConvaiVoiceCapLog, Warning, TEXT("UDEPRECATED_ConvaiVoiceComp::Init VoiceCapture.IsValid()  valid VoiceCapture->GetBufferSize() %d"), VoiceCapture->GetBufferSize());
		IsInit = true;
	}
	else
	{
		UE_LOG(ConvaiVoiceCapLog, Warning, TEXT("StartRecording VoiceCapture is not valid"));
	}
}

void UDEPRECATED_ConvaiVoiceComp::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsInit)
	{
		return;
	}

	if (!VoiceCapture.IsValid())
	{
		return;
	}


	uint32 VoiceCaptureBytesAvailable = 0;
	EVoiceCaptureState::Type CaptureState = VoiceCapture->GetCaptureState(VoiceCaptureBytesAvailable);

	if (CaptureState != EVoiceCaptureState::Ok || VoiceCaptureBytesAvailable == 0)
	{
		//UE_LOG(ConvaiVoiceCapLog, Log, TEXT("CaptureState: %d, VoiceCaptureBytesAvailable: %d"), int(CaptureState), VoiceCaptureBytesAvailable);
		return;
	}

	TArray<uint8> TempBuffer;
	TempBuffer.SetNumUninitialized(VoiceCaptureBytesAvailable);

	// get current voice data
	uint64 OutSampleCounter = 0;
	uint32 OutAvailableVoiceData = 0;
	VoiceCapture->GetVoiceData(TempBuffer.GetData(), VoiceCaptureBytesAvailable, OutAvailableVoiceData, OutSampleCounter);

	//UE_LOG(ConvaiVoiceCapLog, Log, TEXT("VoiceCaptureBytesAvailable: %d bytes | OutAvailableVoiceData: %d | TempBuffer.Num(): %d bytes"), VoiceCaptureBytesAvailable, OutAvailableVoiceData, TempBuffer.Num());
	//UE_LOG(ConvaiVoiceCapLog, Log, TEXT("OutAvailableVoiceData: %d bytes| VoiceCaptureBuffer.Num(): %d bytes "), OutAvailableVoiceData, VoiceCaptureBuffer.Num());

	VoiceCaptureBuffer.Append(TempBuffer);
}

void UDEPRECATED_ConvaiVoiceComp::StartRecording()
{
	if (!IsInit)
	{
		UE_LOG(ConvaiVoiceCapLog, Log, TEXT("StartRecording Initializing..."));
		Init();
		if (!VoiceCapture)
			return;
	}

	if (VoiceCapture && !VoiceCapture.IsValid())
	{
		UE_LOG(ConvaiVoiceCapLog, Warning, TEXT("StartRecording VoiceCapture is not valid"));
		return;
	}

	UE_LOG(ConvaiVoiceCapLog, Log, TEXT("Started Recording "));
	VoiceCapture->Start();
	IsRecording = true;
	VoiceCaptureBuffer.Reset();
}

USoundWave* UDEPRECATED_ConvaiVoiceComp::FinishRecording()
{
	if (!VoiceCapture)
		return nullptr;

	if (!IsRecording)
	{
		UE_LOG(ConvaiVoiceCapLog, Warning, TEXT("StopRecording did not start recording"));
		return nullptr;
	}

	if (VoiceCapture && !VoiceCapture.IsValid())
	{
		UE_LOG(ConvaiVoiceCapLog, Warning, TEXT("StopRecording VoiceCapture is not valid"));
		return nullptr;
	}


	if (!VoiceCapture->IsCapturing())
	{
		UE_LOG(ConvaiVoiceCapLog, Warning, TEXT("StopRecording voice capture is not capturing"));
		return nullptr;
	}



	UE_LOG(ConvaiVoiceCapLog, Log, TEXT("Stopped Recording "));
	VoiceCapture->Stop();
	IsRecording = false;

	if (VoiceCaptureBuffer.Num() < 100)
	{
		UE_LOG(ConvaiVoiceCapLog, Warning, TEXT("StopRecording too little voice data: %d"), VoiceCaptureBuffer.Num());
		return nullptr;
	}

	//UE_LOG(ConvaiVoiceCapLog, Log, TEXT("VoiceCaptureBuffer.Num() final: %d bytes "), VoiceCaptureBuffer.Num());
	USoundWave* OutSoundWave = UConvaiUtils::PCMDataToSoundWav(VoiceCaptureBuffer, 1, 24000);
	//UE_LOG(ConvaiVoiceCapLog, Log, TEXT("OutSoundWave->GetDuration(): %d seconds "), OutSoundWave->GetDuration());

	//USoundWave* OutSoundWave = UConvaiUtils::WavDataToSoundWave(VoiceCaptureBuffer);
	return OutSoundWave;
}
