// Copyright 2022 Convai Inc. All Rights Reserved.


#include "ConvaiAudioCaptureComponent.h"
#include "ConvaiDefinitions.h"

DEFINE_LOG_CATEGORY(ConvaiAudioLog);


namespace Audio
{
	FConvaiAudioCaptureSynth::FConvaiAudioCaptureSynth()
		: NumSamplesEnqueued(0)
		, bInitialized(false)
		, bIsCapturing(false)
	{
	}

	FConvaiAudioCaptureSynth::~FConvaiAudioCaptureSynth()
	{
	}

	TArray<FCaptureDeviceInfo> FConvaiAudioCaptureSynth::GetCaptureDevicesAvailable()
	{
		TArray<FCaptureDeviceInfo> CaptureDevicesInfo;
		AudioCapture.GetCaptureDevicesAvailable(CaptureDevicesInfo);
		for (int i = CaptureDevicesInfo.Num()-1; i>=0; i--)
		{
			if (CaptureDevicesInfo[i].InputChannels <= 0)
				CaptureDevicesInfo.RemoveAt(i);
		}
		return CaptureDevicesInfo;
	}

	int32 FConvaiAudioCaptureSynth::MapInputDeviceIDToDeviceID(int32 InputDeviceID)
	{
		uint32 NumInputDevices = 0;
		TArray<FCaptureDeviceInfo> CaptureDevicesInfo;
		AudioCapture.GetCaptureDevicesAvailable(CaptureDevicesInfo);
		uint32 NumDevices = CaptureDevicesInfo.Num();

		Audio::FCaptureDeviceInfo DeviceInfo;
		for (uint32 DeviceIndex = 0; AudioCapture.GetCaptureDeviceInfo(DeviceInfo, DeviceIndex); DeviceIndex++)
		{
			//UE_LOG(ConvaiAudioLog, Log, TEXT("DeviceIndex: %d, InputDeviceID: %d - name: %s - channels:%d - NumDevices:%d"), DeviceIndex, InputDeviceID, *DeviceInfo.DeviceName, DeviceInfo.InputChannels, NumDevices);
			bool IsInput = DeviceInfo.InputChannels > 0;
			if (!IsInput)
			{
				continue;
			}
			//UE_LOG(ConvaiAudioLog, Log, TEXT("DeviceIndex: %d, is an input - name: %s"), DeviceIndex, *DeviceInfo.DeviceName);
			if (NumInputDevices == InputDeviceID)
			{
				return DeviceIndex;
			}
			NumInputDevices++;
		}
		return -1;
	}

	bool FConvaiAudioCaptureSynth::GetDefaultCaptureDeviceInfo(FCaptureDeviceInfo& OutInfo)
	{
		return AudioCapture.GetCaptureDeviceInfo(OutInfo);
	}

	bool FConvaiAudioCaptureSynth::GetCaptureDeviceInfo(FCaptureDeviceInfo& OutInfo, int32 DeviceIndex)
	{
		DeviceIndex = MapInputDeviceIDToDeviceID(DeviceIndex);
		return AudioCapture.GetCaptureDeviceInfo(OutInfo, DeviceIndex);
	}

	bool FConvaiAudioCaptureSynth::OpenDefaultStream()
	{
		bool bSuccess = true;
		if (!AudioCapture.IsStreamOpen())
		{
			FOnCaptureFunction OnCapture = [this](const float* AudioData, int32 NumFrames, int32 NumChannels, int32 SampleRate, double StreamTime, bool bOverFlow)
			{
				int32 NumSamples = NumChannels * NumFrames;

				FScopeLock Lock(&CaptureCriticalSection);

				if (bIsCapturing)
				{
					// Append the audio memory to the capture data buffer
					int32 Index = AudioCaptureData.AddUninitialized(NumSamples);
					float* AudioCaptureDataPtr = AudioCaptureData.GetData();
					FMemory::Memcpy(&AudioCaptureDataPtr[Index], AudioData, NumSamples * sizeof(float));
				}
			};

			// Prepare the audio buffer memory for 2 seconds of stereo audio at 48k SR to reduce chance for allocation in callbacks
			AudioCaptureData.Reserve(2 * 2 * 48000);

			FAudioCaptureDeviceParams Params = FAudioCaptureDeviceParams();

			// Start the stream here to avoid hitching the audio render thread. 
			if (AudioCapture.OpenCaptureStream(Params, MoveTemp(OnCapture), 1024))
			{
				AudioCapture.StartStream();
			}
			else
			{
				bSuccess = false;
			}
		}
		return bSuccess;
	}

	bool FConvaiAudioCaptureSynth::OpenStream(int32 DeviceIndex)
	{
		bool bSuccess = true;
		if (!AudioCapture.IsStreamOpen())
		{
			FOnCaptureFunction OnCapture = [this](const float* AudioData, int32 NumFrames, int32 NumChannels, int32 SampleRate, double StreamTime, bool bOverFlow)
			{
				int32 NumSamples = NumChannels * NumFrames;

				FScopeLock Lock(&CaptureCriticalSection);

				if (bIsCapturing)
				{
					// Append the audio memory to the capture data buffer
					int32 Index = AudioCaptureData.AddUninitialized(NumSamples);
					float* AudioCaptureDataPtr = AudioCaptureData.GetData();
					FMemory::Memcpy(&AudioCaptureDataPtr[Index], AudioData, NumSamples * sizeof(float));
				}
			};

			// Prepare the audio buffer memory for 2 seconds of stereo audio at 48k SR to reduce chance for allocation in callbacks
			AudioCaptureData.Reserve(2 * 2 * 48000);

			FAudioCaptureDeviceParams Params = FAudioCaptureDeviceParams();
			Params.DeviceIndex = DeviceIndex;
			// Start the stream here to avoid hitching the audio render thread. 
			if (AudioCapture.OpenCaptureStream(Params, MoveTemp(OnCapture), 1024))
			{
				AudioCapture.StartStream();
			}
			else
			{
				bSuccess = false;
			}
		}
		return bSuccess;
	}

	void FConvaiAudioCaptureSynth::CloseStream()
	{
		if (AudioCapture.IsStreamOpen())
		{
			AudioCapture.CloseStream();
		}
	}

	bool FConvaiAudioCaptureSynth::StartCapturing()
	{
		FScopeLock Lock(&CaptureCriticalSection);

		AudioCaptureData.Reset();

		check(AudioCapture.IsStreamOpen());

		bIsCapturing = true;
		return true;
	}

	void FConvaiAudioCaptureSynth::StopCapturing()
	{
		check(AudioCapture.IsStreamOpen());
		check(AudioCapture.IsCapturing());
		FScopeLock Lock(&CaptureCriticalSection);
		bIsCapturing = false;
	}

	void FConvaiAudioCaptureSynth::AbortCapturing()
	{
		AudioCapture.AbortStream();
		AudioCapture.CloseStream();
	}

	bool FConvaiAudioCaptureSynth::IsStreamOpen() const
	{
		return AudioCapture.IsStreamOpen();
	}

	bool FConvaiAudioCaptureSynth::IsCapturing() const
	{
		return bIsCapturing;
	}

	int32 FConvaiAudioCaptureSynth::GetNumSamplesEnqueued()
	{
		FScopeLock Lock(&CaptureCriticalSection);
		return AudioCaptureData.Num();
	}

	FAudioCapture* FConvaiAudioCaptureSynth::GetAudioCapture()
	{
		return &AudioCapture;
	}

	bool FConvaiAudioCaptureSynth::GetAudioData(TArray<float>& OutAudioData)
	{
		FScopeLock Lock(&CaptureCriticalSection);

		int32 CaptureDataSamples = AudioCaptureData.Num();
		if (CaptureDataSamples > 0)
		{
			// Append the capture audio to the output buffer
			int32 OutIndex = OutAudioData.AddUninitialized(CaptureDataSamples);
			float* OutDataPtr = OutAudioData.GetData();
			FMemory::Memcpy(&OutDataPtr[OutIndex], AudioCaptureData.GetData(), CaptureDataSamples * sizeof(float));

			// Reset the capture data buffer since we copied the audio out
			AudioCaptureData.Reset();
			return true;
		}
		return false;
	}
};

UConvaiAudioCaptureComponent::UConvaiAudioCaptureComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSuccessfullyInitialized = false;
	bIsCapturing = false;
	CapturedAudioDataSamples = 0;
	ReadSampleIndex = 0;
	bIsDestroying = false;
	bIsNotReadyForForFinishDestroy = false;
	bIsStreamOpen = false;
	CaptureAudioData.Reserve(2 * 48000 * 5);
	SelectedDeviceIndex = -1;
}

bool UConvaiAudioCaptureComponent::Init(int32& SampleRate)
{

	Audio::FCaptureDeviceInfo DeviceInfo;
	bool FoundDevice = false;
	if (SelectedDeviceIndex == -1)
	{
		FoundDevice = CaptureSynth.GetDefaultCaptureDeviceInfo(DeviceInfo);
	}
	else
	{
		FoundDevice = CaptureSynth.GetCaptureDeviceInfo(DeviceInfo, SelectedDeviceIndex);
		if (!FoundDevice)
		{
			FoundDevice = CaptureSynth.GetDefaultCaptureDeviceInfo(DeviceInfo);
		}
	}

	UE_LOG(ConvaiAudioLog, Log, TEXT("Using %s as Audio capture device with NumChannels:%d and SampleRate:%d"), *DeviceInfo.DeviceName, DeviceInfo.InputChannels, DeviceInfo.PreferredSampleRate);

	if (FoundDevice)
	{
		SampleRate = DeviceInfo.PreferredSampleRate;
		NumChannels = DeviceInfo.InputChannels;

		// Only support mono and stereo mic inputs for now...
		if (NumChannels == 1 || NumChannels == 2)
		{
			// This may fail if capture synths aren't supported on a given platform or if something went wrong with the capture device
			bIsStreamOpen = CaptureSynth.OpenStream(SelectedDeviceIndex);
			if (!bIsStreamOpen)
			{
				UE_LOG(ConvaiAudioLog, Warning, TEXT("OpenStream returned false."));
			}
			return true;
		}
		else
		{
			UE_LOG(ConvaiAudioLog, Warning, TEXT("Audio capture components only support mono and stereo mic input - Audio might be mangeled."));
			return true;
		}

	}
	return false;
}

void UConvaiAudioCaptureComponent::BeginDestroy()
{
	Super::BeginDestroy();

	// Flag that we're beginning to be destroyed
	// This is so that if a mic capture is open, we shut it down on the render thread
	bIsDestroying = true;

	// Make sure stop is kicked off
	Stop();
}

bool UConvaiAudioCaptureComponent::IsReadyForFinishDestroy()
{
	return !bIsNotReadyForForFinishDestroy;
}

void UConvaiAudioCaptureComponent::FinishDestroy()
{
	if (CaptureSynth.IsStreamOpen())
	{
		CaptureSynth.AbortCapturing();
	}

	check(!CaptureSynth.IsStreamOpen());

	Super::FinishDestroy();
	bSuccessfullyInitialized = false;
	bIsCapturing = false;
	bIsDestroying = false;
	bIsStreamOpen = false;
}

bool UConvaiAudioCaptureComponent::GetDefaultCaptureDeviceInfo(Audio::FCaptureDeviceInfo& OutInfo)
{
	return CaptureSynth.GetDefaultCaptureDeviceInfo(OutInfo);
}

bool UConvaiAudioCaptureComponent::GetCaptureDeviceInfo(Audio::FCaptureDeviceInfo& OutInfo, int32 DeviceIndex)
{
	return CaptureSynth.GetCaptureDeviceInfo(OutInfo, DeviceIndex);
}

TArray<Audio::FCaptureDeviceInfo> UConvaiAudioCaptureComponent::GetCaptureDevicesAvailable()
{
	return CaptureSynth.GetCaptureDevicesAvailable();
}

int32 UConvaiAudioCaptureComponent::GetActiveCaptureDevice(Audio::FCaptureDeviceInfo& OutInfo)
{
	if (SelectedDeviceIndex == -1)
		GetDefaultCaptureDeviceInfo(OutInfo);
	else
		GetCaptureDeviceInfo(OutInfo, SelectedDeviceIndex);
	return SelectedDeviceIndex;
}

bool UConvaiAudioCaptureComponent::SetCaptureDevice(int32 DeviceIndex)
{
	Audio::FCaptureDeviceInfo OutInfo;
	if (GetCaptureDeviceInfo(OutInfo, DeviceIndex))
	{
		CaptureSynth.CloseStream();
		SelectedDeviceIndex = DeviceIndex;
		return true;
	}
	else
	{
		return false;
	}
}

Audio::FConvaiAudioCaptureSynth* UConvaiAudioCaptureComponent::GetCaptureSynth()
{
	return &CaptureSynth;
}

void UConvaiAudioCaptureComponent::OnBeginGenerate()
{
	CapturedAudioDataSamples = 0;
	ReadSampleIndex = 0;
	CaptureAudioData.Reset();

	if (!bIsStreamOpen)
	{
		bIsStreamOpen = CaptureSynth.OpenDefaultStream();
	}

	if (bIsStreamOpen)
	{
		CaptureSynth.StartCapturing();
		check(CaptureSynth.IsCapturing());

		// Don't allow this component to be destroyed until the stream is closed again
		bIsNotReadyForForFinishDestroy = true;
		FramesSinceStarting = 0;
		ReadSampleIndex = 0;
	}

}

void UConvaiAudioCaptureComponent::OnEndGenerate()
{
	if (bIsStreamOpen)
	{
		check(CaptureSynth.IsStreamOpen());
		CaptureSynth.StopCapturing();
		bIsStreamOpen = false;

		bIsNotReadyForForFinishDestroy = false;
	}
}

int32 UConvaiAudioCaptureComponent::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
	// Don't do anything if the stream isn't open
	if (!bIsStreamOpen || !CaptureSynth.IsStreamOpen() || !CaptureSynth.IsCapturing())
	{
		// Just return NumSamples, which uses zero'd buffer
		return NumSamples;
	}

	int32 OutputSamplesGenerated = 0;

	if (CapturedAudioDataSamples > 0 || CaptureSynth.GetNumSamplesEnqueued() > 1024)
	{
		// Check if we need to read more audio data from capture synth
		if (ReadSampleIndex + NumSamples > CaptureAudioData.Num())
		{
			// but before we do, copy off the remainder of the capture audio data buffer if there's data in it
			int32 SamplesLeft = FMath::Max(0, CaptureAudioData.Num() - ReadSampleIndex);
			if (SamplesLeft > 0)
			{
				float* CaptureDataPtr = CaptureAudioData.GetData();
				FMemory::Memcpy(OutAudio, &CaptureDataPtr[ReadSampleIndex], SamplesLeft * sizeof(float));

				// Track samples generated
				OutputSamplesGenerated += SamplesLeft;
			}

			// Get another block of audio from the capture synth
			CaptureAudioData.Reset();
			CaptureSynth.GetAudioData(CaptureAudioData);

			// Reset the read sample index since we got a new buffer of audio data
			ReadSampleIndex = 0;
		}

		// note it's possible we didn't get any more audio in our last attempt to get it
		if (CaptureAudioData.Num() > 0)
		{
			// Compute samples to copy
			int32 NumSamplesToCopy = FMath::Min(NumSamples - OutputSamplesGenerated, CaptureAudioData.Num() - ReadSampleIndex);

			float* CaptureDataPtr = CaptureAudioData.GetData();
			FMemory::Memcpy(&OutAudio[OutputSamplesGenerated], &CaptureDataPtr[ReadSampleIndex], NumSamplesToCopy * sizeof(float));
			ReadSampleIndex += NumSamplesToCopy;
			OutputSamplesGenerated += NumSamplesToCopy;
		}

		CapturedAudioDataSamples += OutputSamplesGenerated;
	}
	else
	{
		// Say we generated the full samples, this will result in silence
		OutputSamplesGenerated = NumSamples;
	}

	return OutputSamplesGenerated;
}

