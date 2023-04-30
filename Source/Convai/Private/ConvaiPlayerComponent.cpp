// Copyright 2022 Convai Inc. All Rights Reserved.


#include "ConvaiPlayerComponent.h"
#include "ConvaiUtils.h"
#include "Misc/FileHelper.h"
#include "Http.h"
#include "ConvaiUtils.h"
#include "Containers/UnrealString.h"
#include "Kismet/GameplayStatics.h"
#include "ConvaiDefinitions.h"
#include "ConvaiActionUtils.h"
#include "ConvaiChatbotComponent.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Engine/GameEngine.h"
#include "Sound/SoundWave.h"
#include "ConvaiAudioCaptureComponent.h"
#include "AudioDevice.h"
#include "AudioMixerDevice.h"


DEFINE_LOG_CATEGORY(ConvaiPlayerLog);

static FAudioDevice* GetAudioDeviceFromWorldContext(const UObject* WorldContextObject)
{
	UWorld* ThisWorld = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!ThisWorld || !ThisWorld->bAllowAudioPlayback || ThisWorld->GetNetMode() == NM_DedicatedServer)
	{
		return nullptr;
	}

	return ThisWorld->GetAudioDevice().GetAudioDevice();
}

static Audio::FMixerDevice* GetAudioMixerDeviceFromWorldContext(const UObject* WorldContextObject)
{
	if (FAudioDevice* AudioDevice = GetAudioDeviceFromWorldContext(WorldContextObject))
	{
		if (!AudioDevice->IsAudioMixerEnabled())
		{
			return nullptr;
		}
		else
		{
			return static_cast<Audio::FMixerDevice*>(AudioDevice);
		}
	}
	return nullptr;
}

UConvaiPlayerComponent::UConvaiPlayerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	bAutoActivate = true;
	IsInit = false;
	VoiceCaptureRingBuffer.Init(ConvaiConstants::VoiceCaptureRingBufferCapacity);
	VoiceCaptureBuffer.Empty(ConvaiConstants::VoiceCaptureBufferSize);
	AudioCaptureComponent = CreateDefaultSubobject<UConvaiAudioCaptureComponent>(TEXT("ConvaiAudioCapture"));
	AudioCaptureComponent->SetupAttachment(this);

	const FString FoundSubmixPath = "/ConvAI/Submixes/AudioInput.AudioInput";
	auto _FoundSubmix = ConstructorHelpers::FObjectFinder<USoundSubmixBase>(*FoundSubmixPath).Object;
	if (_FoundSubmix != nullptr) {
		AudioCaptureComponent->SoundSubmix = _FoundSubmix;
	}
	else
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("UConvaiPlayerComponent: Audio Submix was not found, please ensure an audio submix exists at this directory: \"/ConvAI/Submixes/AudioInput\" then restart the Unreal Engine or "));
	}
}
 
bool UConvaiPlayerComponent::Init()
{
	if (IsInit)
	{
		UE_LOG(ConvaiPlayerLog, Log, TEXT("AudioCaptureComponent is already init"));
		return true;
	}

	FString commandMicNoiseGateThreshold = "voice.MicNoiseGateThreshold  0.01";
	FString commandSilenceDetectionThreshold = "voice.SilenceDetectionThreshold 0.001";

	APlayerController* PController = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (PController)
	{
		PController->ConsoleCommand(*commandMicNoiseGateThreshold, true);
		PController->ConsoleCommand(*commandSilenceDetectionThreshold, true);
	}

	AudioCaptureComponent = Cast<UConvaiAudioCaptureComponent>(GetOwner()->GetComponentByClass(UConvaiAudioCaptureComponent::StaticClass()));
	if (!AudioCaptureComponent.IsValid())
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("Init: AudioCaptureComponent is not valid"));
		return false;
	}

	IsInit = true;
	Token = 0;
	return true;
}

bool UConvaiPlayerComponent::GetDefaultCaptureDeviceInfo(FCaptureDeviceInfoBP& OutInfo)
{
	if (!AudioCaptureComponent.IsValid())
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("GetDefaultCaptureDeviceInfo: AudioCaptureComponent is not valid"));
		return false;
	}

	return false;
}

bool UConvaiPlayerComponent::GetCaptureDeviceInfo(FCaptureDeviceInfoBP& OutInfo, int DeviceIndex)
{
	if (!AudioCaptureComponent.IsValid())
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("GetCaptureDeviceInfo: AudioCaptureComponent is not valid"));
		return false;
	}
	Audio::FCaptureDeviceInfo OutDeviceInfo;
	if (AudioCaptureComponent->GetCaptureDeviceInfo(OutDeviceInfo, DeviceIndex))
	{
		OutInfo.bSupportsHardwareAEC = OutDeviceInfo.bSupportsHardwareAEC;
		OutInfo.LongDeviceId = OutDeviceInfo.DeviceId;
		OutInfo.DeviceName = OutDeviceInfo.DeviceName;
		OutInfo.InputChannels = OutDeviceInfo.InputChannels;
		OutInfo.PreferredSampleRate = OutDeviceInfo.PreferredSampleRate;
		OutInfo.DeviceIndex = DeviceIndex;
		return true;
	}

	return false;
}


TMap<int, FCaptureDeviceInfoBP> UConvaiPlayerComponent::GetCaptureDevicesAvailable()
{
	TMap<int, FCaptureDeviceInfoBP> FCaptureDevicesInfoBP;
	if (!AudioCaptureComponent.IsValid())
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("GetCaptureDevicesAvailable: AudioCaptureComponent is not valid"));
		return FCaptureDevicesInfoBP;
	}

	int i = 0;
	for (auto DeviceInfo : AudioCaptureComponent->GetCaptureDevicesAvailable())
	{
		FCaptureDeviceInfoBP CaptureDeviceInfoBP;
		CaptureDeviceInfoBP.bSupportsHardwareAEC = DeviceInfo.bSupportsHardwareAEC;
		CaptureDeviceInfoBP.LongDeviceId = DeviceInfo.DeviceId;
		CaptureDeviceInfoBP.DeviceName = DeviceInfo.DeviceName;
		CaptureDeviceInfoBP.InputChannels = DeviceInfo.InputChannels;
		CaptureDeviceInfoBP.PreferredSampleRate = DeviceInfo.PreferredSampleRate;
		CaptureDeviceInfoBP.DeviceIndex = i++;
		FCaptureDevicesInfoBP.Add(CaptureDeviceInfoBP.DeviceIndex, CaptureDeviceInfoBP);
	}
	return FCaptureDevicesInfoBP;
}

void UConvaiPlayerComponent::GetActiveCaptureDevice(FCaptureDeviceInfoBP& OutInfo)
{
	if (!AudioCaptureComponent.IsValid())
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("GetActiveCaptureDevice: AudioCaptureComponent is not valid"));
		return;
	}
	Audio::FCaptureDeviceInfo OutDeviceInfo;
	int SelectedDeviceIndex = AudioCaptureComponent->GetActiveCaptureDevice(OutDeviceInfo);
	OutInfo.bSupportsHardwareAEC = OutDeviceInfo.bSupportsHardwareAEC;
	OutInfo.LongDeviceId = OutDeviceInfo.DeviceId;
	OutInfo.DeviceName = OutDeviceInfo.DeviceName;
	OutInfo.InputChannels = OutDeviceInfo.InputChannels;
	OutInfo.PreferredSampleRate = OutDeviceInfo.PreferredSampleRate;
	OutInfo.DeviceIndex = SelectedDeviceIndex;

}

bool UConvaiPlayerComponent::SetCaptureDevice(int DeviceIndex)
{
	if (!AudioCaptureComponent.IsValid())
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("SetCaptureDevice: AudioCaptureComponent is not valid"));
		return false;
	}
	return AudioCaptureComponent->SetCaptureDevice(DeviceIndex);
}

void UConvaiPlayerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsInit || !AudioCaptureComponent.IsValid())
	{
		return;
	}

	UpdateVoiceCapture(DeltaTime);
}

void UConvaiPlayerComponent::UpdateVoiceCapture(float DeltaTime)
{
	if (IsRecording || IsStreaming) {
		RemainingTimeUntilNextUpdate -= DeltaTime;
		if (RemainingTimeUntilNextUpdate <= 0)
		{
			FAudioThread::RunCommandOnAudioThread([this]()
				{
					StopVoiceChunkCapture();
					StartVoiceChunkCapture();
				});
			RemainingTimeUntilNextUpdate = TIME_BETWEEN_VOICE_UPDATES_SECS;
		}
	}
	else
	{
		RemainingTimeUntilNextUpdate = 0;
	}
}

void UConvaiPlayerComponent::StartVoiceChunkCapture()
{
	//GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, FString::Printf(TEXT("StartVoiceChunkCapture() in VoiceCaptureComp.cpp")));
	//UE_LOG(LogTemp, Warning, TEXT("StartVoiceChunkCapture() in VoiceCaptureComp.cpp"));
	UAudioMixerBlueprintLibrary::StartRecordingOutput(this, TIME_BETWEEN_VOICE_UPDATES_SECS*2, Cast<USoundSubmix>(AudioCaptureComponent->SoundSubmix));
}

void UConvaiPlayerComponent::ReadRecordedBuffer(Audio::AlignedFloatBuffer& RecordedBuffer, float& OutNumChannels, float& OutSampleRate)
{
	if (Audio::FMixerDevice* MixerDevice = GetAudioMixerDeviceFromWorldContext(this))
	{
		// call the thing here.
		RecordedBuffer = MixerDevice->StopRecording(Cast<USoundSubmix>(AudioCaptureComponent->SoundSubmix), OutNumChannels, OutSampleRate);

		if (RecordedBuffer.Num() == 0)
		{
			//UE_LOG(ConvaiPlayerLog, Warning, TEXT("ReadRecordedBuffer: No audio data. Did you call Start Recording Output?"));
		}
	}
	else
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("ReadRecordedBuffer: Could not get MixerDevice"));
	}
}

void UConvaiPlayerComponent::StopVoiceChunkCapture()
{
	//USoundWave* SoundWaveMic = UAudioMixerBlueprintLibrary::StopRecordingOutput(this, EAudioRecordingExportType::SoundWave, "Convsound", "ConvSound", Cast<USoundSubmix>(AudioCaptureComponent->SoundSubmix));

	float NumChannels;
	float SampleRate;
	Audio::AlignedFloatBuffer RecordedBuffer = Audio::AlignedFloatBuffer();

	ReadRecordedBuffer(RecordedBuffer, NumChannels, SampleRate);
	//NumChannels = 2;
	//SampleRate = ConvaiConstants::VoiceCaptureSampleRate;

	if (RecordedBuffer.Num() == 0)
		return;

	Audio::TSampleBuffer<int16> Int16Buffer = Audio::TSampleBuffer<int16>(RecordedBuffer, NumChannels, SampleRate);
	TArray<int16> OutConverted;

	if (NumChannels > 1 || SampleRate != ConvaiConstants::VoiceCaptureSampleRate)
	{
		UConvaiUtils::ResampleAudio(SampleRate, ConvaiConstants::VoiceCaptureSampleRate, NumChannels, true, (TArray<int16>)Int16Buffer.GetArrayView(), Int16Buffer.GetNumSamples(), OutConverted);
	}
	else
	{
		OutConverted = (TArray<int16>)Int16Buffer.GetArrayView();
	}
	//OutConverted = (TArray<int16>)Int16Buffer.GetArrayView();
	//OutConverted = TArray<int16>(Int16Buffer.GetData(), Int16Buffer.GetNumSamples());

	//UE_LOG(ConvaiPlayerLog, Log, TEXT("Int16Buffer.GetNumSamples() %i, NumChannels %f,  SampleRate %f"), Int16Buffer.GetNumSamples(), NumChannels, SampleRate);
	//UE_LOG(ConvaiPlayerLog, Log, TEXT("OutConverted.Num() %i"), OutConverted.Num());


	if (!ReplicateVoiceToNetwork)
	{
		if (IsRecording)
			VoiceCaptureBuffer.Append((uint8*)OutConverted.GetData(), OutConverted.Num()*sizeof(int16));

		if (IsStreaming)
			VoiceCaptureRingBuffer.Enqueue((uint8*)OutConverted.GetData(), OutConverted.Num() * sizeof(int16));

		onDataReceived_Delegate.ExecuteIfBound();
	}
	else
	{
		// Stream voice data
		AddPCMDataToSend(TArray<uint8>((uint8*)OutConverted.GetData(), OutConverted.Num() * sizeof(int16)), false, ConvaiConstants::VoiceCaptureSampleRate, 1);
	}
}

void UConvaiPlayerComponent::StartRecording()
{
	if (IsRecording)
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("StartRecording: already recording!"));
		return;
	}

	if (IsStreaming)
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("StartRecording: already talking!"));
		return;
	}

	if (!IsInit)
	{
		UE_LOG(ConvaiPlayerLog, Log, TEXT("StartRecording: Initializing..."));
		if (!Init())
		{
			UE_LOG(ConvaiPlayerLog, Warning, TEXT("StartRecording: Could not initialize"));
			return;
		}
	}

	UE_LOG(ConvaiPlayerLog, Log, TEXT("Started Recording "));
	AudioCaptureComponent->Start();    //Start the AudioCaptureComponent

	// reset audio buffers
	StartVoiceChunkCapture();
	StopVoiceChunkCapture();
	VoiceCaptureBuffer.Empty(ConvaiConstants::VoiceCaptureBufferSize);

	IsRecording = true;
}

USoundWave* UConvaiPlayerComponent::FinishRecording()
{
	if (!IsRecording)
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("FinishRecording: did not start recording"));
		return nullptr;
	}

	UE_LOG(ConvaiPlayerLog, Log, TEXT("Stopped Recording "));
	StopVoiceChunkCapture();
	USoundWave* OutSoundWave = UConvaiUtils::PCMDataToSoundWav(VoiceCaptureBuffer, 1, ConvaiConstants::VoiceCaptureSampleRate);
	AudioCaptureComponent->Stop();  //stop the AudioCaptureComponent
	UE_LOG(ConvaiPlayerLog, Log, TEXT("OutSoundWave->GetDuration(): %d seconds "), OutSoundWave->GetDuration());
	IsRecording = false;
	return OutSoundWave;
}

void UConvaiPlayerComponent::StartTalking(
	UConvaiChatbotComponent* ConvaiChatbotComponent,
	UConvaiEnvironment* Environment,
	bool GenerateActions,
	bool VoiceResponse,
	bool RunOnServer,
	bool StreamPlayerMic)
{
	if (IsStreaming)
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("StartTalking: already talking!"));
		return;
	}

	if (IsRecording)
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("StartTalking: already recording!"));
		return;
	}

	if (!IsInit)
	{
		UE_LOG(ConvaiPlayerLog, Log, TEXT("StartTalking Initializing..."));
		if (!Init())
		{
			UE_LOG(ConvaiPlayerLog, Warning, TEXT("StartTalking Could not initialize"));
			return;
		}
	}

	if (!IsValid(ConvaiChatbotComponent))
	{
		if (RunOnServer)
		{
			UE_LOG(ConvaiPlayerLog, Log, TEXT("StartTalking: ConvaiChatbotComponent is not valid, will still stream voice chat to network"));
		}
		else
		{
			UE_LOG(ConvaiPlayerLog, Warning, TEXT("StartTalking: ConvaiChatbotComponent is not valid"));
			return;
		}
	}

	UE_LOG(ConvaiPlayerLog, Log, TEXT("Started Talking"));

	AudioCaptureComponent->Start();    //Start the AudioCaptureComponent

	// reset audio buffers
	StartVoiceChunkCapture();
	StopVoiceChunkCapture();

	IsStreaming = true;
	VoiceCaptureRingBuffer.Empty();

	ReplicateVoiceToNetwork = RunOnServer;
	
	if (RunOnServer)
	{
		if (IsValid(Environment))
			StartTalkingServer(ConvaiChatbotComponent, true, Environment->Actions, Environment->Objects, Environment->Characters, Environment->MainCharacter, GenerateActions, VoiceResponse, StreamPlayerMic);
		else
			StartTalkingServer(ConvaiChatbotComponent, false, TArray<FString>(), TArray<FConvaiObjectEntry>(), TArray<FConvaiObjectEntry>(), FConvaiObjectEntry(), GenerateActions, VoiceResponse, StreamPlayerMic);
	}
	else
	{
		ConvaiChatbotComponent->StartGetResponseStream(this, FString(""), Environment, GenerateActions, VoiceResponse, false, Token);
	}
}

void UConvaiPlayerComponent::FinishTalking()
{
	if (!IsStreaming)
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("FinishTalking did not start talking"));
		return;
	}

	UE_LOG(ConvaiPlayerLog, Log, TEXT("Finished Talking"));
	StopVoiceChunkCapture();
	AudioCaptureComponent->Stop();  //stop the AudioCaptureComponent
	IsStreaming = false;

	if (ReplicateVoiceToNetwork)
	{
		FinishTalkingServer();
	}
	else
	{
		// Invalidate the token by generating a new one
		GenerateNewToken();
	}

	UE_LOG(ConvaiPlayerLog, Log, TEXT("Finished Talking"));
}

void UConvaiPlayerComponent::StartTalkingServer_Implementation(
	class UConvaiChatbotComponent* ConvaiChatbotComponent,
	bool EnvironemntSent,
	const TArray<FString>& Actions,
	const TArray<FConvaiObjectEntry>& Objects,
	const TArray<FConvaiObjectEntry>& Characters,
	FConvaiObjectEntry MainCharacter,
	bool GenerateActions,
	bool VoiceResponse,
	bool StreamPlayerMic)
{
	// if "StreamPlayerMic" is true then "bShouldMuteGlobal" should be false, meaning we will play the player's audio on other clients
	bShouldMuteGlobal = !StreamPlayerMic;

	// Make sure the ring buffer is empty
	VoiceCaptureRingBuffer.Empty();

	// if "ConvaiChatbotComponent" is valid then run StartGetResponseStream function
	if (IsValid(ConvaiChatbotComponent))
	{
		UConvaiEnvironment* Environment = nullptr;
		if (EnvironemntSent)
		{
			if (IsValid(ConvaiChatbotComponent->Environment))
				Environment = ConvaiChatbotComponent->Environment;
			else
			{
				Environment = UConvaiEnvironment::CreateConvaiEnvironment();
			}

			Environment->Actions = Actions;
			Environment->Characters = Characters;
			Environment->Objects = Objects;
			Environment->MainCharacter = MainCharacter;
		}
		ConvaiChatbotComponent->StartGetResponseStream(this, FString(""), Environment, GenerateActions, VoiceResponse, true, Token);
	}
}

void UConvaiPlayerComponent::FinishTalkingServer_Implementation()
{
	// Invalidate the token by generating a new one
	GenerateNewToken();
}

void UConvaiPlayerComponent::SendText(UConvaiChatbotComponent* ConvaiChatbotComponent, FString Text, UConvaiEnvironment* Environment, bool GenerateActions, bool VoiceResponse, bool RunOnServer)
{
	//FString Error;
	//if (GenerateActions && !UConvaiActions::ValidateEnvironment(Environment, Error))
	//{
	//	UE_LOG(ConvaiPlayerLog, Warning, TEXT("SendText: %s"), *Error);
	//	UE_LOG(ConvaiPlayerLog, Log, TEXT("SendText: Environment object seems to have issues -> setting GenerateActions to false"));
	//	GenerateActions = false;
	//}


	if (!IsValid(ConvaiChatbotComponent))
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("SendText: ConvaiChatbotComponent is not valid"));
		return;
	}

	if (Text.Len() == 0)
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("SendText: Text is empty"));
		return;
	}

	ReplicateVoiceToNetwork = RunOnServer;

	if (RunOnServer)
	{
		if (IsValid(Environment))
			SendTextServer(ConvaiChatbotComponent, Text, true, Environment->Actions, Environment->Objects, Environment->Characters, Environment->MainCharacter, GenerateActions, VoiceResponse);
		else
			SendTextServer(ConvaiChatbotComponent, Text, false, TArray<FString>(), TArray<FConvaiObjectEntry>(), TArray<FConvaiObjectEntry>(), FConvaiObjectEntry(), GenerateActions, VoiceResponse);
	}
	else
	{
		ConvaiChatbotComponent->StartGetResponseStream(this, Text, Environment, GenerateActions, VoiceResponse, false, Token);

		// Invalidate the token by generating a new one
		GenerateNewToken();
	}
}

void UConvaiPlayerComponent::SendTextServer_Implementation(
	UConvaiChatbotComponent* ConvaiChatbotComponent,
	const FString& Text,
	bool EnvironemntSent,
	const TArray<FString>& Actions,
	const TArray<FConvaiObjectEntry>& Objects,
	const TArray<FConvaiObjectEntry>& Characters,
	FConvaiObjectEntry MainCharacter,
	bool GenerateActions,
	bool VoiceResponse)
{
	if (!IsValid(ConvaiChatbotComponent))
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("SendTextServer: ConvaiChatbotComponent is not valid"));
		return;
	}

	UConvaiEnvironment* Environment;
	if (IsValid(ConvaiChatbotComponent->Environment))
		Environment = ConvaiChatbotComponent->Environment;
	else
	{
		Environment = UConvaiEnvironment::CreateConvaiEnvironment();
	}

	Environment->Actions = Actions;
	Environment->Characters = Characters;
	Environment->Objects = Objects;
	Environment->MainCharacter = MainCharacter;

	ConvaiChatbotComponent->StartGetResponseStream(this, Text, Environment, GenerateActions, VoiceResponse, true, Token);

	// Invalidate the token by generating a new one
	GenerateNewToken();
}

bool UConvaiPlayerComponent::ShouldMuteLocal()
{
	return true;
}

bool UConvaiPlayerComponent::ShouldMuteGlobal()
{
	return bShouldMuteGlobal;
}

void UConvaiPlayerComponent::OnServerAudioReceived(uint8* VoiceData, uint32 VoiceDataSize, bool ContainsHeaderData, uint32 SampleRate, uint32 NumChannels)
{
	//if (IsRecording)
	//	VoiceCaptureBuffer.Append(VoiceData, VoiceDataSize);

	//if (IsStreaming)
	//	VoiceCaptureRingBuffer.Enqueue(VoiceData, VoiceDataSize);

	//UE_LOG(ConvaiPlayerLog, Log, TEXT("OnServerAudioReceived received %d bytes"), VoiceDataSize);
	//onDataReceived_Delegate.ExecuteIfBound();

	// Simply enque the data into the ring buffer
	VoiceCaptureRingBuffer.Enqueue(VoiceData, VoiceDataSize);
}

void UConvaiPlayerComponent::BeginPlay()
{
	Super::BeginPlay();
}

bool UConvaiPlayerComponent::ConsumeStreamingBuffer(uint8* Buffer, uint32 length)
{
	if (VoiceCaptureRingBuffer.RingDataUsage() < length)
		return false;
	// TODO (Mohamed): add functionality to consume data <= Length
	VoiceCaptureRingBuffer.Dequeue(Buffer, length);
	return true;
}

void UConvaiPlayerComponent::SetIsStreamingServer_Implementation(bool value)
{
	IsStreaming = value;
	if (IsStreaming == false)
		onDataReceived_Delegate.ExecuteIfBound(); // In case a consumer was waiting on this delegate
}