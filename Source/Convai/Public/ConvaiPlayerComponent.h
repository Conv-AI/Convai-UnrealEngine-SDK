// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/AudioComponent.h"
#include "RingBuffer.h"
#include "ConvaiAudioStreamer.h"
#include "Net/OnlineBlueprintCallProxyBase.h"
#include "DSP/BufferVectorOperations.h"
#include "ConvaiPlayerComponent.generated.h"

#define TIME_BETWEEN_VOICE_UPDATES_SECS 0.01

DECLARE_LOG_CATEGORY_EXTERN(ConvaiPlayerLog, Log, All);

DECLARE_DELEGATE(FonDataReceived_Delegate);

// class IVoiceCapture;
class UConvaiAudioCaptureComponent;

// UENUM(BlueprintType)
// enum class EHardwareInputFeatureBP : uint8
// {
// 	EchoCancellation,
// 	NoiseSuppression,
// 	AutomaticGainControl
// };

// TODO (Mohamed): Ensure both Chatbot and Player components have the same ReplicateVoiceToNetwork value
// TODO (Mohamed): Send Text should also be handled in this class (UConvaiPlayerComponent) like we did with voice

USTRUCT(BlueprintType)
struct FCaptureDeviceInfoBP
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Convai|Microphone")
	FString DeviceName = "";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Convai|Microphone")
	int DeviceIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Convai|Microphone")
	FString LongDeviceId = "";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Convai|Microphone")
	int InputChannels = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Convai|Microphone")
	int PreferredSampleRate = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Convai|Microphone")
	bool bSupportsHardwareAEC = 0;
};


UCLASS(Blueprintable, BlueprintType, meta = (BlueprintSpawnableComponent), DisplayName = "Convai Player")
class CONVAI_API UConvaiPlayerComponent : public UConvaiAudioStreamer
{
	GENERATED_BODY()

	UConvaiPlayerComponent();

	virtual void OnComponentCreated() override;

	void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	bool Init();

	//~ Begin ActorComponent Interface.
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End ActorComponent Interface.

public:

	UPROPERTY(EditAnywhere, Category = "Convai", Replicated, BlueprintSetter = SetPlayerName)
	FString PlayerName;

	/**
	 *    Sets a new player name
	 */
	UFUNCTION(BlueprintCallable, BlueprintInternalUseOnly, Category = "Convai")
	void SetPlayerName(FString NewPlayerName);

	UFUNCTION(Server, Reliable, Category = "Convai|Network")
	void SetPlayerNameServer(const FString& NewPlayerName);

	UFUNCTION(BlueprintCallable, Category = "Convai|Microphone")
	bool GetDefaultCaptureDeviceInfo(FCaptureDeviceInfoBP& OutInfo);

	UFUNCTION(BlueprintCallable, Category = "Convai|Microphone")
	bool GetCaptureDeviceInfo(FCaptureDeviceInfoBP& OutInfo, int DeviceIndex);

	UFUNCTION(BlueprintCallable, Category = "Convai|Microphone")
	TArray<FCaptureDeviceInfoBP> GetAvailableCaptureDeviceDetails();

	UFUNCTION(BlueprintCallable, Category = "Convai|Microphone")
	TArray<FString> GetAvailableCaptureDeviceNames();

	UFUNCTION(BlueprintCallable, Category = "Convai|Microphone")
	void GetActiveCaptureDevice(FCaptureDeviceInfoBP& OutInfo);

	UFUNCTION(BlueprintCallable, Category = "Convai|Microphone")
	bool SetCaptureDeviceByIndex(int DeviceIndex);

	UFUNCTION(BlueprintCallable, Category = "Convai|Microphone")
	bool SetCaptureDeviceByName(FString DeviceName);

	UFUNCTION(BlueprintCallable, Category = "Convai|Microphone")
	void SetMicrophoneVolumeMultiplier(float InVolumeMultiplier, bool& Success);

	UFUNCTION(BlueprintPure, BlueprintCallable, Category = "Convai|Microphone")
	void GetMicrophoneVolumeMultiplier(float& OutVolumeMultiplier, bool& Success);

	// UFUNCTION(BlueprintCallable, Category = "Convai|Microphone")
	// void GetIfHardwareFeatureIsSupported(EHardwareInputFeatureBP FeatureType, bool& Success);

	// UFUNCTION(BlueprintCallable, Category = "Convai|Microphone")
	// void SetHardwareFeatureEnabled(EHardwareInputFeatureBP FeatureType, bool bIsEnabled, bool& Success);

	/**
	 *    Start recording audio from the microphone, use "Finish Recording" function afterwards
	 */
	UFUNCTION(BlueprintCallable, Category = "Convai|Microphone")
	void StartRecording();

	/**
	 *    Stops recording from the microphone and outputs the recorded audio as a Sound Wave
	 */
	UFUNCTION(BlueprintCallable, Category = "Convai|Microphone")
	USoundWave* FinishRecording();

	 /**
	  *   Starts streaming microphone audio to the character. Use "Finish Talking" afterward to let the character know that you are done talking
	  *	  @param ConvaiChatbotComponent					The character to talk to
	  *	  @param Environment							Holds all relevant objects and characters in the scene including the (Player), and also all the actions doable by the character. Use "CreateConvaiEnvironment" function to create and then use functions like (AddAction, AddCharacter, AddMainCharacter) to fill it up
	  *	  @param GenerateActions						Whether or not to generate actions (Environment has to be given and valid)
	  *	  @param VoiceResponse							If true it will generate a voice response, otherwise, it will only generate a text response.
	  *	  @param RunOnServer							If true it will run this function on the server, this can be used in multiplayer sessions to allow other players to hear the character's voice response.
	  *	  @param StreamPlayerMic						If true it will stream the player's voice to other players in the multiplayer session, this is the same effect as voice chat.
	 */
	UFUNCTION(BlueprintCallable, Category = "Convai|Microphone")
	void StartTalking(
		UConvaiChatbotComponent* ConvaiChatbotComponent,
		UConvaiEnvironment* Environment,
		bool GenerateActions,
		bool VoiceResponse,
		bool RunOnServer,
		bool StreamPlayerMic,
		bool UseServerAPI_Key);
	
	/**
	* Stops streaming microphone audio to the character.
	*/
	UFUNCTION(BlueprintCallable, Category = "Convai|Microphone")
	void FinishTalking();

	UFUNCTION(Server, Reliable, Category = "Convai|Network")
	void StartTalkingServer(
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
		const FString& AuthHeader);

	UFUNCTION(Server, Reliable, Category = "Convai|Network")
	void FinishTalkingServer();

	/**
	*	Sends text to the character.
	*	@param ConvaiChatbotComponent				The character to talk to
	*	@param Text									Text to be sent to the character.
	*	@param Environment							Holds all relevant objects and characters in the scene including the (Player), and also all the actions doable by the character. Use "CreateConvaiEnvironment" function to create and then use functions like (AddAction, AddCharacter, AddMainCharacter) to fill it up
	*	@param GenerateActions						Whether or not to generate actions (Environment has to be given and valid)
	*	@param VoiceResponse						If true it will generate a voice response, otherwise, it will only generate a text response.
	*	@param RunOnServer							If true it will run this function on the server, this can be used in multiplayer sessions to allow other players to hear the character's voice response.
	*/
	UFUNCTION(BlueprintCallable, Category = "Convai|Microphone")
		void SendText(
			UConvaiChatbotComponent* ConvaiChatbotComponent,
			FString Text,
			UConvaiEnvironment* Environment,
			bool GenerateActions,
			bool VoiceResponse,
			bool RunOnServer,
			bool UseServerAPI_Key);

	UFUNCTION(Server, Reliable, Category = "Convai|Network")
	void SendTextServer(
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
		const FString& AuthHeader);

	virtual bool ShouldMuteLocal() override;

	virtual bool ShouldMuteGlobal() override;

	virtual void OnServerAudioReceived(uint8* VoiceData, uint32 VoiceDataSize, bool ContainsHeaderData = true, uint32 SampleRate = 21000, uint32 NumChannels = 1) override;

	// UActorComponent interface
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	bool ConsumeStreamingBuffer(TArray<uint8>& Buffer);

	UFUNCTION(Server, Reliable, Category = "Convai|Network")
	void SetIsStreamingServer(bool value);

	// Returns true if microphone audio is being streamed, false otherwise.
	UFUNCTION(BlueprintPure, BlueprintCallable, Category = "Convai|Microphone", meta = (DisplayName = "Is Talking"))
	bool GetIsStreaming()
	{
		return(IsStreaming);
	}

	// Returns true if microphone audio is being recorded, false otherwise.
	UFUNCTION(BlueprintPure, BlueprintCallable, Category = "Convai|Microphone", meta = (DisplayName = "Is Recording"))
	bool GetIsRecording()
	{
		return(IsRecording);
	}

	FonDataReceived_Delegate& getOnDataReceivedDelegate()
	{
		return onDataReceived_Delegate;
	}

	bool CheckTokenValidty(uint32 TokenToCheck)
	{
		return Token == TokenToCheck;
	}

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Convai|PixelStreaming")
	TWeakObjectPtr<USynthComponent> PixelStreamingAudioComponent;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Convai|PixelStreaming")
	bool UsePixelStreamingMicInput;

	bool IsPixelStreamingEnabledAndAllowed();

private:

	uint32 GenerateNewToken()
	{
		Token += 1;
		return Token;
	}

	UPROPERTY()
	USoundWaveProcedural* VoiceCaptureSoundWaveProcedural;

	//TSharedPtr<IVoiceCapture> VoiceCapture;

	// Buffer used with recording
	TArray<uint8> VoiceCaptureBuffer;

	// Buffer used with streaming
	// TODO (Mohamed): use TCircularAudioBuffer instead
	TRingBuffer<uint8> VoiceCaptureRingBuffer;

	UPROPERTY()
	UConvaiAudioCaptureComponent* AudioCaptureComponent = nullptr;

	UPROPERTY()
	UConvaiChatbotComponent* CurrentConvaiChatbotComponent = nullptr;

	void UpdateVoiceCapture(float DeltaTime);
	void StartVoiceChunkCapture(float ExpectedRecordingTime = 0.01);
	void StopVoiceChunkCapture();
	void ReadRecordedBuffer(Audio::AlignedFloatBuffer& RecordedBuffer, float& OutNumChannels, float& OutSampleRate);

	void StartAudioCaptureComponent();
	void StopAudioCaptureComponent();


	FonDataReceived_Delegate onDataReceived_Delegate;

	bool IsRecording = false;
	bool IsStreaming = false;
	bool IsInit = false;
	bool bShouldMuteGlobal;

	float RemainingTimeUntilNextUpdate = 0;

	// Used by a consumer (e.g. chatbot component) to validate if this player component is still streaming audio data to it
	uint32 Token;

	USoundSubmixBase* _FoundSubmix;
};