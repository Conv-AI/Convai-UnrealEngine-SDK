// Copyright 2022 Convai Inc. All Rights Reserved.


#include "ConvaiPlayerComponent.h"
#include "ConvaiAudioCaptureComponent.h"
#include "ConvaiChatbotComponent.h"
#include "ConvaiActionUtils.h"
#include "ConvaiUtils.h"
#include "ConvaiDefinitions.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Net/UnrealNetwork.h"
#include "Misc/FileHelper.h"
#include "Http.h"
#include "ConvaiUtils.h"
#include "Containers/UnrealString.h"
#include "Kismet/GameplayStatics.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Engine/GameEngine.h"
#include "Sound/SoundWave.h"
#include "AudioDevice.h"
#include "AudioMixerDevice.h"
#include "UObject/ConstructorHelpers.h"


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
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
		bool Found = AudioDevice != nullptr;
#else
		bool Found = AudioDevice != nullptr && AudioDevice->IsAudioMixerEnabled();
#endif

		if (!Found)
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
	//SetIsReplicated(true);
	bAutoActivate = true;
	PlayerName = "Guest";

	IsInit = false;
	VoiceCaptureRingBuffer.Init(ConvaiConstants::VoiceCaptureRingBufferCapacity);
	VoiceCaptureBuffer.Empty(ConvaiConstants::VoiceCaptureBufferSize);
	UsePixelStreamingMicInput = true;

	const FString FoundSubmixPath = "/ConvAI/Submixes/AudioInput.AudioInput";
	static ConstructorHelpers::FObjectFinder<USoundSubmixBase> SoundSubmixFinder(*FoundSubmixPath);
	if (SoundSubmixFinder.Succeeded())
	{
		_FoundSubmix = SoundSubmixFinder.Object;
	}
}

void UConvaiPlayerComponent::OnComponentCreated()
{
	Super::OnComponentCreated();

	AudioCaptureComponent = NewObject<UConvaiAudioCaptureComponent>(this, UConvaiAudioCaptureComponent::StaticClass(), TEXT("ConvaiAudioCapture"));
	if (AudioCaptureComponent)
	{
		AudioCaptureComponent->RegisterComponent();
	}
	//AudioCaptureComponent->SetupAttachment(this);

	if (_FoundSubmix != nullptr) {
		AudioCaptureComponent->SoundSubmix = _FoundSubmix;
		UE_LOG(ConvaiPlayerLog, Log, TEXT("UConvaiPlayerComponent: Found submix \"AudioInput\""));
	}
	else
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("UConvaiPlayerComponent: Audio Submix was not found, please ensure an audio submix exists at this directory: \"/ConvAI/Submixes/AudioInput\" then restart Unreal Engine"));
	}
}

void UConvaiPlayerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UConvaiPlayerComponent, PlayerName);
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
	if (!IsValid(AudioCaptureComponent))
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("Init: AudioCaptureComponent is not valid"));
		return false;
	}

	IsInit = true;
	Token = 0;
	return true;
}

void UConvaiPlayerComponent::SetPlayerName(FString NewPlayerName)
{
	PlayerName = NewPlayerName;

	if (GetIsReplicated())
	{
		SetPlayerNameServer(PlayerName);
	}
}

void UConvaiPlayerComponent::SetPlayerNameServer_Implementation(const FString& NewPlayerName)
{
	PlayerName = NewPlayerName;
}

bool UConvaiPlayerComponent::GetDefaultCaptureDeviceInfo(FCaptureDeviceInfoBP& OutInfo)
{
	if (!IsValid(AudioCaptureComponent))
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("GetDefaultCaptureDeviceInfo: AudioCaptureComponent is not valid"));
		return false;
	}

	return false;
}

bool UConvaiPlayerComponent::GetCaptureDeviceInfo(FCaptureDeviceInfoBP& OutInfo, int DeviceIndex)
{
	if (!IsValid(AudioCaptureComponent))
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

TArray<FCaptureDeviceInfoBP> UConvaiPlayerComponent::GetAvailableCaptureDeviceDetails()
{
	TArray<FCaptureDeviceInfoBP> FCaptureDevicesInfoBP;
	if (!IsValid(AudioCaptureComponent))
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("GetAvailableCaptureDeviceDetails: AudioCaptureComponent is not valid"));
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
		FCaptureDevicesInfoBP.Add(CaptureDeviceInfoBP);
	}
	return FCaptureDevicesInfoBP;
}

TArray<FString> UConvaiPlayerComponent::GetAvailableCaptureDeviceNames()
{
	TArray<FString> AvailableDeviceNames;
	if (!IsValid(AudioCaptureComponent))
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("GetAvailableCaptureDeviceNames: AudioCaptureComponent is not valid"));
		return AvailableDeviceNames;
	}

	for (auto CaptureDeviceInfo : GetAvailableCaptureDeviceDetails())
	{
		AvailableDeviceNames.Add(CaptureDeviceInfo.DeviceName);
	}

	return AvailableDeviceNames;
}

void UConvaiPlayerComponent::GetActiveCaptureDevice(FCaptureDeviceInfoBP& OutInfo)
{
	if (!IsValid(AudioCaptureComponent))
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

bool UConvaiPlayerComponent::SetCaptureDeviceByIndex(int DeviceIndex)
{
	if (!IsValid(AudioCaptureComponent))
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("SetCaptureDeviceByIndex: AudioCaptureComponent is not valid"));
		return false;
	}

	if (DeviceIndex >= GetAvailableCaptureDeviceDetails().Num())
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("SetCaptureDeviceByIndex: Invalid Device Index: %d - Max possible index: %d."), DeviceIndex, GetAvailableCaptureDeviceDetails().Num() - 1);
		return false;
	}

	return AudioCaptureComponent->SetCaptureDevice(DeviceIndex);
}

bool UConvaiPlayerComponent::SetCaptureDeviceByName(FString DeviceName)
{
	if (!IsValid(AudioCaptureComponent))
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("SetCaptureDeviceByName: AudioCaptureComponent is not valid"));
		return false;
	}

	bool bDeviceFound = false;
	int DeviceIndex = -1;
	for (auto CaptureDeviceInfo : GetAvailableCaptureDeviceDetails())
	{
		if (CaptureDeviceInfo.DeviceName == DeviceName)
		{
			bDeviceFound = true;
			DeviceIndex = CaptureDeviceInfo.DeviceIndex;
		}
	}

	TArray<FString> AvailableDeviceNames;

	if (!bDeviceFound)
	{
		AvailableDeviceNames = GetAvailableCaptureDeviceNames();
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("SetCaptureDeviceByName: Could not find Device name: %s - Available Device names are: [%s]."), *DeviceName, *FString::Join(AvailableDeviceNames, *FString(" - ")));
		return false;
	}

	if (!SetCaptureDeviceByIndex(DeviceIndex))
	{
		AvailableDeviceNames = GetAvailableCaptureDeviceNames();
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("SetCaptureDeviceByName: SetCaptureDeviceByIndex failed for index: %d and device name: %s - Available Device names are: [%s]."), DeviceIndex, *DeviceName, *FString::Join(AvailableDeviceNames, *FString(" - ")));
		return false;
	}
	return true;
}

void UConvaiPlayerComponent::SetMicrophoneVolumeMultiplier(float InVolumeMultiplier, bool& Success)
{
	Success = false;
	if (IsValid(AudioCaptureComponent))
	{
		AudioCaptureComponent->SetVolumeMultiplier(InVolumeMultiplier);
		Success = true;
	}

	if (PixelStreamingAudioComponent.IsValid())
	{
		AudioCaptureComponent->SetVolumeMultiplier(InVolumeMultiplier*2); // multiply by 2 because Pixel Streaming mic is usually very low
		Success = true;
	}

	if (!IsValid(AudioCaptureComponent) && !PixelStreamingAudioComponent.IsValid())
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("SetMicrophoneVolumeMultiplier: AudioCaptureComponent and PixelStreamingAudioComponent are not valid"));
	}
}

void UConvaiPlayerComponent::GetMicrophoneVolumeMultiplier(float& OutVolumeMultiplier, bool& Success)
{
	Success = false;
	if (!IsValid(AudioCaptureComponent))
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("SetMicrophoneVolumeMultiplier: AudioCaptureComponent is not valid"));
		return;
	}
	auto InternalAudioComponent = AudioCaptureComponent->GetAudioComponent();
	if (!InternalAudioComponent)
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("GetMicrophoneVolumeMultiplier: InternalAudioComponent is not valid"));
	}
	OutVolumeMultiplier = InternalAudioComponent->VolumeMultiplier;
	Success = true;
}

// void UConvaiPlayerComponent::GetIfHardwareFeatureIsSupported(EHardwareInputFeatureBP FeatureType, bool& Success)
// {
// 	Success = false;
// 	if (!IsValid(AudioCaptureComponent))
// 	{
// 		UE_LOG(ConvaiPlayerLog, Warning, TEXT("GetIfHardwareFeatureIsSupported: AudioCaptureComponent is not valid"));
// 		return;
// 	}

// 	auto CaptureSynth = AudioCaptureComponent->GetCaptureSynth();
// 	if (CaptureSynth == nullptr)
// 	{
// 		UE_LOG(ConvaiPlayerLog, Warning, TEXT("GetIfHardwareFeatureIsSupported: CaptureSynth is not valid"));
// 		return;
// 	}

// 	auto AudioCapture = CaptureSynth->GetAudioCapture();
// 	if (AudioCapture == nullptr)
// 	{
// 		UE_LOG(ConvaiPlayerLog, Warning, TEXT("GetIfHardwareFeatureIsSupported: AudioCapture is not valid"));
// 		return;
// 	}
// 	Success = AudioCapture->GetIfHardwareFeatureIsSupported((Audio::EHardwareInputFeature)FeatureType);
// }

// void UConvaiPlayerComponent::SetHardwareFeatureEnabled(EHardwareInputFeatureBP FeatureType, bool bIsEnabled, bool& Success)
// {
// 	bool HardwareFeatureIsSupported;
// 	GetIfHardwareFeatureIsSupported(FeatureType, HardwareFeatureIsSupported);
// 	if (!HardwareFeatureIsSupported)
// 	{
// 		if (!bIsEnabled)
// 		{
// 			Success = true;
// 			return;
// 		}
// 		else
// 		{
// 			Success = false;
// 			return;
// 		}
// 	}

// 	auto AudioCapture = AudioCaptureComponent->GetCaptureSynth()->GetAudioCapture();
// 	AudioCapture->SetHardwareFeatureEnabled((Audio::EHardwareInputFeature)FeatureType, bIsEnabled);
// 	Success = true;
// }

void UConvaiPlayerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsInit || !IsValid(AudioCaptureComponent))
	{
		return;
	}

	UpdateVoiceCapture(DeltaTime);
}

void UConvaiPlayerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsRecording)
		FinishRecording();

	if (IsStreaming)
		FinishTalking();

	Super::EndPlay(EndPlayReason);
}

bool UConvaiPlayerComponent::IsPixelStreamingEnabledAndAllowed()
{
	return UsePixelStreamingMicInput && PixelStreamingAudioComponent.IsValid();
}

void UConvaiPlayerComponent::UpdateVoiceCapture(float DeltaTime)
{
	if (IsRecording || IsStreaming) {
		RemainingTimeUntilNextUpdate -= DeltaTime;
		if (RemainingTimeUntilNextUpdate <= 0)
		{
			float ExpectedRecordingTime = DeltaTime > TIME_BETWEEN_VOICE_UPDATES_SECS ? DeltaTime : TIME_BETWEEN_VOICE_UPDATES_SECS;
			FAudioThread::RunCommandOnAudioThread([this, ExpectedRecordingTime]()
				{
					StopVoiceChunkCapture();
					StartVoiceChunkCapture(ExpectedRecordingTime);
				});
			RemainingTimeUntilNextUpdate = TIME_BETWEEN_VOICE_UPDATES_SECS;
		}
	}
	else
	{
		RemainingTimeUntilNextUpdate = 0;
	}
}

void UConvaiPlayerComponent::StartVoiceChunkCapture(float ExpectedRecordingTime)
{
	//GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, FString::Printf(TEXT("StartVoiceChunkCapture() in VoiceCaptureComp.cpp")));
	//UE_LOG(LogTemp, Warning, TEXT("StartVoiceChunkCapture() in VoiceCaptureComp.cpp"));
	UAudioMixerBlueprintLibrary::StartRecordingOutput(this, ExpectedRecordingTime, Cast<USoundSubmix>(AudioCaptureComponent->SoundSubmix));
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

void UConvaiPlayerComponent::StartAudioCaptureComponent()
{
	if (IsPixelStreamingEnabledAndAllowed())
	{
		PixelStreamingAudioComponent->Start();
	}
	else
	{
		AudioCaptureComponent->Start();
	}
}

void UConvaiPlayerComponent::StopAudioCaptureComponent()
{
	if (IsPixelStreamingEnabledAndAllowed())
	{
		PixelStreamingAudioComponent->Stop();
	}
	AudioCaptureComponent->Stop();
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

	if (IsRecording)
	{
		VoiceCaptureBuffer.Append((uint8*)OutConverted.GetData(), OutConverted.Num() * sizeof(int16));
		return;
	}

	if (!ReplicateVoiceToNetwork)
	{
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
	StartAudioCaptureComponent();    //Start the AudioCaptureComponent

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
	StopAudioCaptureComponent();  //stop the AudioCaptureComponent
	if (IsValid(OutSoundWave))
		UE_LOG(ConvaiPlayerLog, Log, TEXT("OutSoundWave->GetDuration(): %f seconds "), OutSoundWave->GetDuration());
	IsRecording = false;
	return OutSoundWave;
}

void UConvaiPlayerComponent::StartTalking(
	UConvaiChatbotComponent* ConvaiChatbotComponent,
	UConvaiEnvironment* Environment,
	bool GenerateActions,
	bool VoiceResponse,
	bool RunOnServer,
	bool StreamPlayerMic,
	bool UseServerAPI_Key)
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

	StartAudioCaptureComponent();    //Start the AudioCaptureComponent

	// reset audio buffers
	StartVoiceChunkCapture();
	StopVoiceChunkCapture();

	IsStreaming = true;
	VoiceCaptureRingBuffer.Empty();

	ReplicateVoiceToNetwork = RunOnServer;

	TPair<FString, FString> AuthHeaderAndKey = UConvaiUtils::GetAuthHeaderAndKey();
	FString AuthKey = AuthHeaderAndKey.Value;
	FString AuthHeader = AuthHeaderAndKey.Key;

	FString ClientAuthKey = UseServerAPI_Key ? FString("") : AuthKey;

	if (RunOnServer)
	{
		if (IsValid(Environment))
			StartTalkingServer(ConvaiChatbotComponent, true, Environment->Actions, Environment->Objects, Environment->Characters, Environment->MainCharacter, GenerateActions, VoiceResponse, StreamPlayerMic, UseServerAPI_Key, ClientAuthKey, AuthHeader);
		else
			StartTalkingServer(ConvaiChatbotComponent, false, TArray<FString>(), TArray<FConvaiObjectEntry>(), TArray<FConvaiObjectEntry>(), FConvaiObjectEntry(), GenerateActions, VoiceResponse, StreamPlayerMic, UseServerAPI_Key, ClientAuthKey, AuthHeader);
	}
	else
	{
		bool UseOverrideAuthKey = false;
		ConvaiChatbotComponent->StartGetResponseStream(this, FString(""), Environment, GenerateActions, VoiceResponse, false, UseOverrideAuthKey, FString(""), FString(""), Token);
		CurrentConvaiChatbotComponent = ConvaiChatbotComponent;
	}
}

void UConvaiPlayerComponent::FinishTalking()
{
	if (!IsStreaming)
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("FinishTalking did not start talking"));
		return;
	}

	StopVoiceChunkCapture();
	StopAudioCaptureComponent();  //stop the AudioCaptureComponent
	IsStreaming = false;

	if (ReplicateVoiceToNetwork)
	{
		FinishTalkingServer();
	}
	else
	{
		// Invalidate the token by generating a new one
		GenerateNewToken();

		if (IsValid(CurrentConvaiChatbotComponent))
		{
			UE_LOG(ConvaiPlayerLog, Log, TEXT("FinishTalking calling FinishGetResponseStream"));
			CurrentConvaiChatbotComponent->FinishGetResponseStream(this);
			CurrentConvaiChatbotComponent = nullptr;
		}
		else
		{
			UE_LOG(ConvaiPlayerLog, Warning, TEXT("FinishTalking failed to call FinishGetResponseStream"));
		}
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
	bool StreamPlayerMic,
	bool UseServerAPI_Key,
	const FString& ClientAuthKey, 
	const FString& AuthHeader)
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
		bool UseOverrideAuthKey = !UseServerAPI_Key;
		ConvaiChatbotComponent->StartGetResponseStream(this, FString(""), Environment, GenerateActions, VoiceResponse, true, UseOverrideAuthKey, ClientAuthKey, AuthHeader, Token);
		CurrentConvaiChatbotComponent = ConvaiChatbotComponent;
	}
}

void UConvaiPlayerComponent::FinishTalkingServer_Implementation()
{
	// Invalidate the token by generating a new one
	GenerateNewToken();

	if (IsValid(CurrentConvaiChatbotComponent))
	{
		UE_LOG(ConvaiPlayerLog, Log, TEXT("FinishTalking calling FinishGetResponseStream"));
		CurrentConvaiChatbotComponent->FinishGetResponseStream(this);
		CurrentConvaiChatbotComponent = nullptr;
	}
	else
	{
		UE_LOG(ConvaiPlayerLog, Warning, TEXT("FinishTalking failed to call FinishGetResponseStream"));
	}
}

void UConvaiPlayerComponent::SendText(UConvaiChatbotComponent* ConvaiChatbotComponent, FString Text, UConvaiEnvironment* Environment, bool GenerateActions, bool VoiceResponse, bool RunOnServer, bool UseServerAPI_Key)
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

	TPair<FString, FString> AuthHeaderAndKey = UConvaiUtils::GetAuthHeaderAndKey();
	FString AuthKey = AuthHeaderAndKey.Value;
	FString AuthHeader = AuthHeaderAndKey.Key;

	FString ClientAuthKey = UseServerAPI_Key ? FString("") : AuthKey;

	if (RunOnServer)
	{
		if (IsValid(Environment))
			SendTextServer(ConvaiChatbotComponent, Text, true, Environment->Actions, Environment->Objects, Environment->Characters, Environment->MainCharacter, GenerateActions, VoiceResponse, UseServerAPI_Key, ClientAuthKey, AuthHeader);
		else
			SendTextServer(ConvaiChatbotComponent, Text, false, TArray<FString>(), TArray<FConvaiObjectEntry>(), TArray<FConvaiObjectEntry>(), FConvaiObjectEntry(), GenerateActions, VoiceResponse, UseServerAPI_Key, ClientAuthKey, AuthHeader);
	}
	else
	{
		bool UseOverrideAuthKey = false;
		ConvaiChatbotComponent->StartGetResponseStream(this, Text, Environment, GenerateActions, VoiceResponse, false, UseOverrideAuthKey, FString(""), FString(""), Token);

		// Invalidate the token by generating a new one
		GenerateNewToken();

		CurrentConvaiChatbotComponent = ConvaiChatbotComponent;
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
	bool VoiceResponse,
	bool UseServerAPI_Key,
	const FString& ClientAuthKey, 
	const FString& AuthHeader)
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

	bool UseOverrideAuthKey = !UseServerAPI_Key;
	ConvaiChatbotComponent->StartGetResponseStream(this, Text, Environment, GenerateActions, VoiceResponse, true, UseOverrideAuthKey, ClientAuthKey, AuthHeader, Token);

	// Invalidate the token by generating a new one
	GenerateNewToken();

	CurrentConvaiChatbotComponent = ConvaiChatbotComponent;
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

	if (IsValid(AudioCaptureComponent))
	{
		AudioCaptureComponent->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);
	}
	else
	{
		UE_LOG(ConvaiPlayerLog, Error, TEXT("Could not attach AudioCaptureComponent"));
	}

	if (!IsInit)
	{
		if (!Init())
		{
			UE_LOG(ConvaiPlayerLog, Warning, TEXT("Could not initialize Audio Decoder"));
			return;
		}
	}
}

bool UConvaiPlayerComponent::ConsumeStreamingBuffer(TArray<uint8>& Buffer)
{
	int Datalength = VoiceCaptureRingBuffer.RingDataUsage();
	if (Datalength <= 0)
		return false;

	Buffer.SetNumUninitialized(Datalength, false);
	VoiceCaptureRingBuffer.Dequeue(Buffer.GetData(), Datalength);

	return true;
}

void UConvaiPlayerComponent::SetIsStreamingServer_Implementation(bool value)
{
	IsStreaming = value;
	if (IsStreaming == false)
		onDataReceived_Delegate.ExecuteIfBound(); // In case a consumer was waiting on this delegate
}
