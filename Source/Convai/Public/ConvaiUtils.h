// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ConvaiDefinitions.h"
#include "Utility/Log/ConvaiLogger.h"
#include "ConvaiUtils.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(ConvaiUtilsLog, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(ConvaiFormValidationLog, Log, All);

class USoundWave;
class APlayerController;
class UObject;
class UConvaiSubsystem;
struct FAnimationFrame;

UCLASS()
class CONVAI_API UConvaiUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	static UConvaiSubsystem* GetConvaiSubsystem(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Convai|Utilities")
	static void StereoToMono(TArray<uint8> stereoWavBytes, TArray<uint8>& monoWavBytes);

	UFUNCTION(BlueprintCallable, Category = "Convai|Utilities")
	static bool ReadFileAsByteArray(const FString FilePath, TArray<uint8>& Bytes);

	UFUNCTION(BlueprintCallable, Category = "Convai|Utilities")
	static bool SaveByteArrayAsFile(FString FilePath, TArray<uint8> Bytes);

	UFUNCTION(BlueprintCallable, Category = "Convai|Utilities")
	static FString ByteArrayToString(TArray<uint8> Bytes);

	// Writes a string to a file
	UFUNCTION(BlueprintCallable, Category = "Convai|Utilities")
	static bool WriteStringToFile(const FString& StringToWrite, const FString& FilePath);

	// Reads a string from a file
	UFUNCTION(BlueprintCallable, Category = "Convai|Utilities")
	static bool ReadStringFromFile(FString& OutString, const FString& FilePath);

	static float CalculateAudioDuration(uint32 AudioSize, uint8 Channels, uint32 SampleRate, uint8 SampleSize = 2);

	UFUNCTION(BlueprintPure, BlueprintCallable, Category = "Convai|Utilities", meta = (WorldContext = "WorldContextObject", AutoCreateRefTerm = "IncludedCharacters, ExcludedCharacters"))
	static void ConvaiGetLookedAtCharacter(UObject* WorldContextObject, APlayerController* PlayerController, float Radius, bool PlaneView, TArray<UObject*> IncludedCharacters, TArray<UObject*> ExcludedCharacters, UConvaiChatbotComponent*& ConvaiCharacter, bool& Found);
	
	UFUNCTION(BlueprintPure, BlueprintCallable, Category = "Convai|Utilities", meta = (WorldContext = "WorldContextObject"))
	static void ConvaiGetLookedAtObjectOrCharacter(UObject* WorldContextObject, APlayerController* PlayerController, float Radius, bool PlaneView, TArray<FConvaiObjectEntry> ListToSearchIn, FConvaiObjectEntry& FoundObjectOrCharacter, bool& Found);

	UFUNCTION(BlueprintPure, BlueprintCallable, Category = "Convai|Utilities", meta = (WorldContext = "WorldContextObject"))
	static void ConvaiGetAllPlayerComponents(UObject* WorldContextObject, TArray<class UConvaiPlayerComponent*>& ConvaiPlayerComponents);

	UFUNCTION(BlueprintPure, BlueprintCallable, Category = "Convai|Utilities", meta = (WorldContext = "WorldContextObject"))
	static void ConvaiGetAllChatbotComponents(UObject* WorldContextObject, TArray<class UConvaiChatbotComponent*>& ConvaiChatbotComponents);

	UFUNCTION(BlueprintCallable, Category = "Convai|Settings")
	static void SetAPI_Key(FString API_Key);	
	
	UFUNCTION(BlueprintPure, Category = "Convai|Settings")
	static FString GetAPI_Key();

	UFUNCTION(BlueprintCallable, Category = "Convai|Settings")
	static void SetAuthToken(FString AuthToken);

	UFUNCTION(BlueprintPure, Category = "Convai|Settings")
	static FString GetAuthToken();

	static TPair<FString, FString> GetAuthHeaderAndKey();

	UFUNCTION(BlueprintPure, Category = "Convai|Settings")
	static FString GetTestCharacterID();

	UFUNCTION(BlueprintPure, Category = "Convai|Settings")
	static bool IsNewActionSystemEnabled();

	UFUNCTION(BlueprintPure, Category = "Convai|Utilities")
	static void GetPluginInfo(FString PluginName, bool& Found, FString& VersionName, FString& EngineVersion, FString& FriendlyName);

	UFUNCTION(BlueprintPure, Category = "Convai|Utilities")
	static void GetPlatformInfo(FString& EngineVersion, FString& PlatformName);
	
	UFUNCTION(BlueprintPure, Category = "Convai|Utilities")
	static FString GetDeviceUniqueIdentifier();

	/**
	 * Maps and transforms blendshapes from one naming convention/rig to another.
	 * Supports per-blendshape multipliers, offsets, clamping, and override values.
	 *
	 * This is useful for:
	 * - Converting ARKit blendshapes to MetaHuman blendshapes
	 * - Remapping custom character rigs
	 * - Applying global scaling and offset to all blendshapes
	 * - Overriding specific blendshape values
	 * - Clamping blendshape values to valid ranges
	 *
	 * @param InputBlendshapes The source blendshape map (e.g., from ARKit or AI system)
	 * @param BlendshapeMap Mapping configuration for each blendshape (multipliers, offsets, target names, etc.)
	 * @param GlobalMultiplier Global multiplier applied to all blendshapes (unless IgnoreGlobalModifiers is set)
	 * @param GlobalOffset Global offset added to all blendshapes (unless IgnoreGlobalModifiers is set)
	 * @return Transformed blendshape map ready for the target character rig
	 *
	 * Example:
	 * - Input: {"jawOpen": 0.5}
	 * - BlendshapeMap: {"jawOpen" -> TargetNames: ["CTRL_expressions_mouthOpen"], Multiplier: 2.0, Offset: 0.1}
	 * - GlobalMultiplier: 1.0, GlobalOffset: 0.0
	 * - Output: {"CTRL_expressions_mouthOpen": 1.1}  // (0.5 * 2.0 * 1.0) + 0.1 + 0.0
	 */
	UFUNCTION(BlueprintPure, Category = "Convai|Blendshapes")
	static TMap<FName, float> MapBlendshapes(const TMap<FName,float>& InputBlendshapes, const TMap<FName, FConvaiBlendshapeParameters>& BlendshapeMap, float GlobalMultiplier, float GlobalOffset);


	/**
	 * Splits a blendshape map into two maps based on a list of keys.
	 * Keys found in SplitKeys are moved to OutSplitMap and removed from InOutOriginalMap.
	 *
	 * This is useful for:
	 * - Separating blendshapes that need different blend modes (additive vs replace)
	 * - Isolating specific facial regions (eyes, mouth, etc.) for independent control
	 * - Creating layered animation systems
	 *
	 * Performance: O(n) where n is the number of blendshapes in the original map
	 * Uses TSet internally for O(1) key lookups
	 *
	 * @param InOutOriginalMap The original map. Keys matching SplitKeys will be removed from this map.
	 * @param SplitKeys Array of blendshape names to extract from the original map.
	 * @param OutSplitMap Output map containing only the key-value pairs whose keys are in SplitKeys.
	 *
	 * Example:
	 * - InOutOriginalMap: {"eyeBlinkL": 0.5, "jawOpen": 0.3, "eyeBlinkR": 0.5}
	 * - SplitKeys: ["eyeBlinkL", "eyeBlinkR"]
	 * - After execution:
	 *   - InOutOriginalMap: {"jawOpen": 0.3}
	 *   - OutSplitMap: {"eyeBlinkL": 0.5, "eyeBlinkR": 0.5}
	 */
	UFUNCTION(BlueprintCallable, Category = "Convai|Blendshapes", meta = (DisplayName = "Split Blendshape Map by Keys"))
	static void SplitBlendshapeMapByKeys(
		UPARAM(ref) TMap<FName, float>& InOutOriginalMap,
		const TArray<FName>& SplitKeys,
		TMap<FName, float>& OutSplitMap
	);

	/**
	 * Merges two blendshape maps together.
	 * If a key exists in both maps, the value from OverrideMap takes precedence.
	 *
	 * This is useful for:
	 * - Combining blendshapes from multiple sources (AI + manual animation)
	 * - Applying corrective blendshapes on top of base animation
	 * - Layering different animation systems
	 *
	 * @param BaseMap The base map to merge into.
	 * @param OverrideMap The map whose values will override the base map.
	 * @return A new map containing all key-value pairs from both maps.
	 *
	 * Example:
	 * - BaseMap: {"eyeBlinkL": 0.3, "jawOpen": 0.5}
	 * - OverrideMap: {"eyeBlinkL": 0.8, "browUp": 0.2}
	 * - Result: {"eyeBlinkL": 0.8, "jawOpen": 0.5, "browUp": 0.2}
	 */
	UFUNCTION(BlueprintPure, Category = "Convai|Blendshapes", meta = (DisplayName = "Merge Blendshape Maps"))
	static TMap<FName, float> MergeBlendshapeMaps(
		const TMap<FName, float>& BaseMap,
		const TMap<FName, float>& OverrideMap
	);

	static TArray<uint8> ExtractPCMDataFromSoundWave(USoundWave* SoundWave, int32& OutSampleRate, int32& OutNumChannels);

	static void PCMDataToWav(TArray<uint8> InPCMBytes, TArray<uint8>& OutWaveFileData, int NumChannels, int SampleRate);

	static USoundWave* PCMDataToSoundWav(TArray<uint8> InPCMBytes, int NumChannels, int SampleRate);

	static USoundWave* WavDataToSoundWave(TArray<uint8> InWavData);

	// Writes a USoundWave to a .wav file on disk
	UFUNCTION(BlueprintCallable, Category = "Convai|Utilities")
	static bool WriteSoundWaveToWavFile(USoundWave* SoundWave, const FString& FilePath);

	// Reads a .wav file from disk and creates a USoundWave
	UFUNCTION(BlueprintPure, Category = "Convai|Utilities")
	static USoundWave* ReadWavFileAsSoundWave(const FString& FilePath);

	static void ResampleAudio(float currentSampleRate, float targetSampleRate, int numChannels, bool reduceToMono, int16* currentPcmData, int numSamplesToConvert, TArray<int16>& outResampledPcmData);

	static void ResampleAudio(float currentSampleRate, float targetSampleRate, int numChannels, bool reduceToMono, const TArray<int16>& currentPcmData, int numSamplesToConvert, TArray<int16>& outResampledPcmData);

	static FString FUTF8ToFString(const char* StringToConvert);

	static int LevenshteinDistance(const FString& s, const FString& t);

	static TArray<FAnimationFrame> ParseJsonToBlendShapeData(const FString& JsonString);

	static bool ParseVisemeValuesToAnimationFrame(const FString& VisemeValuesString, FAnimationFrame& AnimationFrame);

	// UFUNCTION(BlueprintCallable, Category = "ActorFuncions", meta = (WorldContext = WorldContextObject))
	static AActor* ConvaiCloneActor(AActor* InputActor);

	// UFUNCTION(BlueprintPure, Category = "Convai|Utilities|AnimationSequence")
	static FString ConvaiAnimationSequenceToJson(const FAnimationSequenceBP& AnimationSequenceBP);

	// UFUNCTION(BlueprintPure, Category = "Convai|Utilities|AnimationSequence")
	static void ConvaiAnimationSequenceFromJson(const FString& JsonString, FAnimationSequenceBP& AnimationSequenceBP);
};

UCLASS()
class CONVAI_API UConvaiSettingsUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static bool GetParamValueAsString(const FString& paramName, FString& outValue);

	static bool GetParamValueAsFloat(const FString& paramName, float& outValue);

	static bool GetParamValueAsInt(const FString & paramName, int32 & outValue);
};

UCLASS()
class CONVAI_API UConvaiFormValidation : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	static bool ValidateAuthKey(FString API_Key)
	{
		if ((API_Key.Len()))
		{
			return true;
		}
		else
		{
			CONVAI_LOG(LogTemp, Warning, TEXT("Empty API Key, please add it in Edit->Project Settings->Convai"));
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
			CONVAI_LOG(ConvaiFormValidationLog, Warning, TEXT("Empty Session ID"));
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
			CONVAI_LOG(ConvaiFormValidationLog, Warning, TEXT("Empty Character ID"));
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
			CONVAI_LOG(LogTemp, Warning, TEXT("Empty Input Text"));
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
			CONVAI_LOG(ConvaiFormValidationLog, Warning, TEXT("Invalid Voice Type"));
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
			CONVAI_LOG(ConvaiFormValidationLog, Warning, TEXT("Empty Backstory"));
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
			CONVAI_LOG(ConvaiFormValidationLog, Warning, TEXT("Empty Character Name"));
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
			CONVAI_LOG(ConvaiFormValidationLog, Warning, TEXT("Input Voice is too short (less than 44 bytes)"));
			return false;
		}
	}
};

UCLASS()
class CONVAI_API UCommandLineUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	// Function to check if a flag is present in the command line - Has issue it always returns false
	//UFUNCTION(BlueprintCallable, Category = "CommandLine")
	static bool IsCommandLineFlagPresent(const FString& Flag);

	// Function to get the value of a command line flag as an integer
	UFUNCTION(BlueprintCallable, Category = "CommandLine")
		static int32 GetCommandLineFlagValueAsInt(const FString& Flag, int32 DefaultValue = 0);

	// Function to get the value of a command line flag as a string
	UFUNCTION(BlueprintCallable, Category = "CommandLine")
		static FString GetCommandLineFlagValueAsString(const FString& Flag, const FString& DefaultValue = TEXT(""));

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "CommandLine")
	static FString GetCommandLineFlagValueAsStringNoDefault(const FString& Flag);
};