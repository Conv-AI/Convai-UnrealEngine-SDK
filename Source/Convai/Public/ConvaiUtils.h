// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ConvaiUtils.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(ConvaiUtilsLog, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(ConvaiFormValidationLog, Log, All);

class USoundWave;
class APlayerController;
class UObject;
class UConvaiSubsystem;

/**
 *
 */
UCLASS()
class UConvaiUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	static UConvaiSubsystem* GetConvaiSubsystem(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Convai|Utils")
	static void StereoToMono(TArray<uint8> stereoWavBytes, TArray<uint8>& monoWavBytes);

	UFUNCTION(BlueprintCallable, Category = "Convai|Utils")
	static bool ReadFileAsByteArray(FString FilePath, TArray<uint8>& Bytes);

	UFUNCTION(BlueprintCallable, Category = "Convai|Utils")
	static bool SaveByteArrayAsFile(FString FilePath, TArray<uint8> Bytes);

	UFUNCTION(BlueprintCallable, Category = "Convai|Utils")
	static FString ByteArrayToString(TArray<uint8> Bytes);

	UFUNCTION(BlueprintPure, BlueprintCallable, Category = "Convai", meta = (WorldContext = "WorldContextObject", AutoCreateRefTerm = "IncludedCharacters, ExcludedCharacters"))
	static void ConvaiGetLookedAtCharacter(UObject* WorldContextObject, APlayerController* PlayerController, float Radius, bool PlaneView, TArray<UObject*> IncludedCharacters, TArray<UObject*> ExcludedCharacters, UConvaiChatbotComponent*& ConvaiCharacter, bool& Found);
	
	UFUNCTION(BlueprintPure, BlueprintCallable, Category = "Convai", meta = (WorldContext = "WorldContextObject"))
	static void ConvaiGetAllPlayerComponents(UObject* WorldContextObject, TArray<class UConvaiPlayerComponent*>& ConvaiPlayerComponents);

	UFUNCTION(BlueprintPure, BlueprintCallable, Category = "Convai", meta = (WorldContext = "WorldContextObject"))
	static void ConvaiGetAllChatbotComponents(UObject* WorldContextObject, TArray<class UConvaiChatbotComponent*>& ConvaiChatbotComponents);

	UFUNCTION(BlueprintCallable, Category = "Convai|Settings")
	static void SetAPI_Key(FString API_Key);	
	
	UFUNCTION(BlueprintPure, Category = "Convai|Settings")
	static FString GetAPI_Key();

	UFUNCTION(BlueprintPure, Category = "Convai|Settings")
	static bool IsNewActionSystemEnabled();

	UFUNCTION(BlueprintPure, Category = "Convai")
	static void GetPluginInfo(FString PluginName, bool& Found, FString& VersionName, FString& EngineVersion, FString& FriendlyName);

	UFUNCTION(BlueprintPure, Category = "Convai")
	static void GetPlatformInfo(FString& EngineVersion, FString& PlatformName);

	static TArray<uint8> ExtractPCMDataFromSoundWave(USoundWave* SoundWave, int32& OutSampleRate);

	static void PCMDataToWav(TArray<uint8> InPCMBytes, TArray<uint8>& OutWaveFileData, int NumChannels, int SampleRate);

	static USoundWave* PCMDataToSoundWav(TArray<uint8> InPCMBytes, int NumChannels, int SampleRate);

	static USoundWave* WavDataToSoundWave(TArray<uint8> InWavData);

	static void ResampleAudio(float currentSampleRate, float targetSampleRate, int numChannels, bool reduceToMono, int16* currentPcmData, int numSamplesToConvert, TArray<int16>& outResampledPcmData);

	static void ResampleAudio(float currentSampleRate, float targetSampleRate, int numChannels, bool reduceToMono, const TArray<int16>& currentPcmData, int numSamplesToConvert, TArray<int16>& outResampledPcmData);

	static FString FUTF8ToFString(const char* StringToConvert);

	static int LevenshteinDistance(const FString& s, const FString& t);

};


UCLASS()
class UConvaiFormValidation : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	static bool ValidateAPIKey(FString API_Key)
	{
		if ((API_Key.Len()))
		{
			return true;
		}
		else
		{
			UE_LOG(ConvaiFormValidationLog, Warning, TEXT("Empty API Key, please add it in Edit->Project Settings->Convai"));
			return false;
		}
	}

	static bool ValidateSessionID(FString SessionID)
	{
		if ((SessionID.Len()))
		{
			return true;
		}
		else
		{
			UE_LOG(ConvaiFormValidationLog, Warning, TEXT("Empty Session ID"));
			return false;
		}
	}

	static bool ValidateCharacterID(FString CharacterID)
	{
		if ((CharacterID.Len()))
		{
			return true;
		}
		else
		{
			UE_LOG(ConvaiFormValidationLog, Warning, TEXT("Empty Character ID"));
			return false;
		}
	}

	static bool ValidateInputText(FString InputText)
	{
		if ((InputText.Len()))
		{
			return true;
		}
		else
		{
			UE_LOG(ConvaiFormValidationLog, Warning, TEXT("Empty Input Text"));
			return false;
		}
	}

	static bool ValidateVoiceType(FString VoiceType)
	{
		if ((VoiceType.Len()))
		{
			return true;
		}
		else
		{
			UE_LOG(ConvaiFormValidationLog, Warning, TEXT("Invalid Voice Type"));
			return false;
		}
	}

	static bool ValidateBackstory(FString Backstory)
	{
		if ((Backstory.Len()))
		{
			return true;
		}
		else
		{
			UE_LOG(ConvaiFormValidationLog, Warning, TEXT("Empty Backstory"));
			return false;
		}
	}

	static bool ValidateCharacterName(FString CharacterName)
	{
		if ((CharacterName.Len()))
		{
			return true;
		}
		else
		{
			UE_LOG(ConvaiFormValidationLog, Warning, TEXT("Empty Character Name"));
			return false;
		}
	}

	static bool ValidateInputVoice(TArray<uint8> InputVoiceData)
	{
		if ((InputVoiceData.Num() > 44))
		{
			return true;
		}
		else
		{
			UE_LOG(ConvaiFormValidationLog, Warning, TEXT("Input Voice is too short (less than 44 bytes)"));
			return false;
		}
	}
};