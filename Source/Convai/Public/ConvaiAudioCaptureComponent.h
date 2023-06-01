// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AudioCapture.h"
#include "Components/SynthComponent.h"
#include "AudioCaptureCore.h"
#include "AudioCaptureDeviceInterface.h"
#include "ConvaiAudioCaptureComponent.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(ConvaiAudioLog, Log, All);

namespace Audio
{
	/** Class which contains an FAudioCapture object and performs analysis on the audio stream, only outputing audio if it matches a detection criteria. */
	class FConvaiAudioCaptureSynth
	{
	public:
		FConvaiAudioCaptureSynth();
		virtual ~FConvaiAudioCaptureSynth();

		TArray<FCaptureDeviceInfo> GetCaptureDevicesAvailable();

		int32 MapInputDeviceIDToDeviceID(int32 InputDeviceID);

		// Gets the default capture device info
		bool GetDefaultCaptureDeviceInfo(FCaptureDeviceInfo& OutInfo);

		bool GetCaptureDeviceInfo(FCaptureDeviceInfo& OutInfo, int32 DeviceIndex);

		// Opens up a stream to the default capture device
		bool OpenDefaultStream();

		bool OpenStream(int32 DeviceIndex);

		void CloseStream();

		// Starts capturing audio
		bool StartCapturing();

		// Stops capturing audio
		void StopCapturing();

		// Immediately stop capturing audio
		void AbortCapturing();

		// Returned if the capture synth is closed
		bool IsStreamOpen() const;

		// Returns true if the capture synth is capturing audio
		bool IsCapturing() const;

		// Retrieves audio data from the capture synth.
		// This returns audio only if there was non-zero audio since this function was last called.
		bool GetAudioData(TArray<float>& OutAudioData);

		// Returns the number of samples enqueued in the capture synth
		int32 GetNumSamplesEnqueued();

		FAudioCapture* GetAudioCapture();

	private:

		// Number of samples enqueued
		int32 NumSamplesEnqueued;

		// Information about the default capture device we're going to use
		FCaptureDeviceInfo CaptureInfo;

		// Audio capture object dealing with getting audio callbacks
		FAudioCapture AudioCapture;

		// Critical section to prevent reading and writing from the captured buffer at the same time
		FCriticalSection CaptureCriticalSection;

		// Buffer of audio capture data, yet to be copied to the output 
		TArray<float> AudioCaptureData;

		// If the object has been initialized
		bool bInitialized;

		// If we're capturing data
		bool bIsCapturing;
	};

};


UCLASS(ClassGroup = Synth, meta = (BlueprintSpawnableComponent))
class UConvaiAudioCaptureComponent : public USynthComponent
{
	GENERATED_BODY()

protected:

	UConvaiAudioCaptureComponent(const FObjectInitializer& ObjectInitializer);

	//~ Begin USynthComponent interface
	virtual bool Init(int32& SampleRate) override;
	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;
	virtual void OnBeginGenerate() override;
	virtual void OnEndGenerate() override;
	//~ End USynthComponent interface

	//~ Begin UObject interface
	virtual void BeginDestroy();
	virtual bool IsReadyForFinishDestroy() override;
	virtual void FinishDestroy() override;
	//~ End UObject interface

public:
	/**
	*   Induced latency in audio frames to use to account for jitter between mic capture hardware and audio render hardware.
	 *	Increasing this number will increase latency but reduce potential for underruns.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Latency", meta = (ClampMin = "0", ClampMax = "1024"))
		int32 JitterLatencyFrames;

	bool GetDefaultCaptureDeviceInfo(Audio::FCaptureDeviceInfo& OutInfo);

	bool GetCaptureDeviceInfo(Audio::FCaptureDeviceInfo& OutInfo, int32 DeviceIndex);

	TArray<Audio::FCaptureDeviceInfo> GetCaptureDevicesAvailable();

	int32 GetActiveCaptureDevice(Audio::FCaptureDeviceInfo& OutInfo);

	bool SetCaptureDevice(int32 DeviceIndex);

	Audio::FConvaiAudioCaptureSynth* GetCaptureSynth();

private:
	int32 SelectedDeviceIndex;

	Audio::FConvaiAudioCaptureSynth CaptureSynth;
	TArray<float> CaptureAudioData;
	int32 CapturedAudioDataSamples;

	bool bSuccessfullyInitialized;
	bool bIsCapturing;
	bool bIsStreamOpen;
	int32 CaptureChannels;
	int32 FramesSinceStarting;
	int32 ReadSampleIndex;
	FThreadSafeBool bIsDestroying;
	FThreadSafeBool bIsNotReadyForForFinishDestroy;
};
