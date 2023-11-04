// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ConvaiDefinitions.generated.h"


UENUM(BlueprintType)
enum class ETTS_Voice_Type : uint8
{
	MALE,
	FEMALE,
	WUKMale_1,
	WUKFemale_1,
	SUKMale_1,
	WAFemale_1,
	WAMale_1,
	SIFemale_1,
	SIMale_1,
	SUFemale_1,
	SUMale_1,
	WUFemale_1,
	WUMale_1,
	Trixie,
	Twilight_Sparkle,
	Celestia,
	Spike,
	Applejack,
};

UENUM(BlueprintType)
enum class EEmotionIntensity : uint8
{
	Basic        UMETA(DisplayName = "Basic"),
	LessIntense  UMETA(DisplayName = "Less Intense"),
	MoreIntense  UMETA(DisplayName = "More Intense"),
	None         UMETA(DisplayName = "None", BlueprintHidden) // To handle cases when the emotion is not found
};

UENUM(BlueprintType)
enum class EBasicEmotions : uint8
{
	Joy          UMETA(DisplayName = "Joy"),
	Trust        UMETA(DisplayName = "Trust"),
	Fear         UMETA(DisplayName = "Fear"),
	Surprise     UMETA(DisplayName = "Surprise"),
	Sadness      UMETA(DisplayName = "Sadness"),
	Disgust      UMETA(DisplayName = "Disgust"),
	Anger        UMETA(DisplayName = "Anger"),
	Anticipation UMETA(DisplayName = "Anticipation"),
	None         UMETA(DisplayName = "None", BlueprintHidden) // To handle cases when the emotion is not found
};

USTRUCT(BlueprintType)
struct FConvaiObjectEntry
{
	GENERATED_USTRUCT_BODY()

public:
	/** A refrence of a character or object*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Convai|Action API")
		TWeakObjectPtr<AActor> Ref;

	/** A related position vector*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Convai|Action API")
		FVector OptionalPositionVector;

	/** The Name of the character or object*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Convai|Action API")
		FString Name;


	/** The bio/description for the chracter/object*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Convai|Action API")
	FString Description;

	friend bool operator==(const FConvaiObjectEntry& lhs, const FConvaiObjectEntry& rhs)
	{
		return lhs.Name == rhs.Name;
	}

	FConvaiObjectEntry()
		: Ref(nullptr)
		, OptionalPositionVector(FVector(0, 0, 0))
		, Name(FString(""))
		, Description(FString(""))
	{
	}
};

USTRUCT(BlueprintType)
struct FConvaiExtraParams
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Convai|Action API")
		float Number;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Convai|Action API")
		FString Text;

	FConvaiExtraParams()
		:Number(0),
		Text("")
	{

	}
};

USTRUCT(BlueprintType)
struct FConvaiResultAction
{
	GENERATED_BODY()

	/** The action to be made*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Convai|Action API")
	FString Action;

	/** The object or character whom the action is to be made on*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Convai|Action API")
	FConvaiObjectEntry RelatedObjectOrCharacter;

	/** The actual string of the action without any preprocessing*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Convai|Action API")
	FString ActionString;

	/** Has extra parameters*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Convai|Action API")
	FConvaiExtraParams ConvaiExtraParams;
};

USTRUCT(BlueprintType)
struct FConvaiBlendshapeParameters
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Convai|LipSync")
	TArray<FName> TargetNames;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Convai|LipSync")
	float Multiplyer = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Convai|LipSync")
	float Offset = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Convai|LipSync")
	bool UseOverrideValue = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Convai|LipSync")
	bool IgnoreGlobalModifiers = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Convai|LipSync")
	float OverrideValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Convai|LipSync")
	float ClampMinValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Convai|LipSync")
	float ClampMaxValue = 1;
};

USTRUCT()
struct FAnimationFrame
{
	GENERATED_BODY()

	UPROPERTY()
	int32 FrameIndex = 0;

	UPROPERTY()
	TMap<FName, float> BlendShapes;

	FString ToString()
	{
		FString Result;

		// iterate over all elements in the TMap
		for (const auto& Elem : BlendShapes)
		{
			// Append the key-value pair to the result string
			Result += Elem.Key.ToString() + TEXT(": ") + FString::SanitizeFloat(Elem.Value) + TEXT(", ");
		}

		// Remove the trailing comma and space for cleanliness, if present
		if (Result.Len() > 0)
		{
			Result.RemoveAt(Result.Len() - 2);
		}

		return Result;
	}
};

USTRUCT()
struct FAnimationSequence
{
	GENERATED_BODY()
public:
	UPROPERTY()
	TArray<FAnimationFrame> AnimationFrames;

	UPROPERTY()
	float Duration = 0;
};

USTRUCT()
struct FConvaiEmotionState
{
	GENERATED_BODY()

public:
	void GetEmotionDetails(const FString& Emotion, EEmotionIntensity& Intensity, EBasicEmotions& BasicEmotion)
	{
		// Static dictionaries of emotions
		static const TMap<FString, EBasicEmotions> BasicEmotions = {
			{"Joy", EBasicEmotions::Joy},
			{"Trust", EBasicEmotions::Trust},
			{"Fear", EBasicEmotions::Fear},
			{"Surprise", EBasicEmotions::Surprise},
			{"Sadness", EBasicEmotions::Sadness},
			{"Disgust", EBasicEmotions::Disgust},
			{"Anger", EBasicEmotions::Anger},
			{"Anticipation", EBasicEmotions::Anticipation}
		};

		static const TMap<FString, EBasicEmotions> LessIntenseEmotions = {
			{"Serenity", EBasicEmotions::Joy},
			{"Acceptance", EBasicEmotions::Trust},
			{"Apprehension", EBasicEmotions::Fear},
			{"Distraction", EBasicEmotions::Surprise},
			{"Pensiveness", EBasicEmotions::Sadness},
			{"Boredom", EBasicEmotions::Disgust},
			{"Annoyance", EBasicEmotions::Anger},
			{"Interest", EBasicEmotions::Anticipation}
		};

		static const TMap<FString, EBasicEmotions> MoreIntenseEmotions = {
			{"Ecstasy", EBasicEmotions::Joy},
			{"Admiration", EBasicEmotions::Trust},
			{"Terror", EBasicEmotions::Fear},
			{"Amazement", EBasicEmotions::Surprise},
			{"Grief", EBasicEmotions::Sadness},
			{"Loathing", EBasicEmotions::Disgust},
			{"Rage", EBasicEmotions::Anger},
			{"Vigilance", EBasicEmotions::Anticipation}
		};

		// Initialize the output parameters
		Intensity = EEmotionIntensity::None;
		BasicEmotion = EBasicEmotions::None;

		// Look up the emotion
		if (BasicEmotions.Contains(Emotion))
		{
			Intensity = EEmotionIntensity::Basic;
			BasicEmotion = BasicEmotions[Emotion];
		}
		else if (LessIntenseEmotions.Contains(Emotion))
		{
			Intensity = EEmotionIntensity::LessIntense;
			BasicEmotion = LessIntenseEmotions[Emotion];
		}
		else if (MoreIntenseEmotions.Contains(Emotion))
		{
			Intensity = EEmotionIntensity::MoreIntense;
			BasicEmotion = MoreIntenseEmotions[Emotion];
		}
	}

	void ForceSetEmotion(const EBasicEmotions& BasicEmotion, const EEmotionIntensity& Intensity, const bool& ResetOtherEmotions)
	{
		if (ResetOtherEmotions)
		{
			ResetEmotionScores();
		}

		float Score = 0;
		if (const float* ScoreMultiplier = ScoreMultipliers.Find(Intensity))
		{
			Score = *ScoreMultiplier;
		}
		else
		{
			Score = 0;
		}

		EmotionsScore.Add(BasicEmotion, Score);
	}

	void SetEmotionData(const FString& EmotionRespponse)
	{
		TArray<FString> OutputEmotionsArray;
		// Separate the string into an array based on the space delimiter
		EmotionRespponse.ParseIntoArray(OutputEmotionsArray, TEXT(" "), true);
		SetEmotionData(OutputEmotionsArray);
	}

	void SetEmotionData(const TArray<FString>& EmotionArray)
	{
		ResetEmotionScores();
		EEmotionIntensity Intensity = EEmotionIntensity::None;
		EBasicEmotions BasicEmotion = EBasicEmotions::None;
		float Score = 0;

		int i = 0;
		for (FString Emotion : EmotionArray)
		{
			GetEmotionDetails(Emotion, Intensity, BasicEmotion);
			if (Intensity == EEmotionIntensity::None || BasicEmotion == EBasicEmotions::None)
				continue;

			if (const float* ScoreMultiplier = ScoreMultipliers.Find(Intensity))
			{
				Score = *ScoreMultiplier  * FMath::Exp(float(-i) / float(EmotionArray.Num()));
			}
			else
			{
				Score = 0;
			}

			EmotionsScore.Add(BasicEmotion, Score);
			i++;
		}

	}

	float GetEmotionScore(const EBasicEmotions& Emotion)
	{
		float Score = 0;
		if (const float* ScorePointer = EmotionsScore.Find(Emotion))
		{
			Score = *ScorePointer;
		}
		return Score;
	}

	void ResetEmotionScores()
	{
		EmotionsScore.Empty();
		
		for (int32 i = 0; i <= static_cast<int32>(EBasicEmotions::Anticipation); ++i)
		{
			EBasicEmotions EnumValue = static_cast<EBasicEmotions>(i);
			EmotionsScore.Add(EnumValue, 0);
		}
	}

private:
	TMap<EBasicEmotions, float> EmotionsScore;

	static const TMap<EEmotionIntensity, float> ScoreMultipliers;
};

USTRUCT()
struct FConvaiEnvironmentDetails
{
	GENERATED_BODY()

public:

	UPROPERTY()
	TArray<FString> Actions;

	UPROPERTY()
	TArray<FConvaiObjectEntry> Objects;

	UPROPERTY()
	TArray<FConvaiObjectEntry> Characters;

	UPROPERTY()
	FConvaiObjectEntry MainCharacter;
};

// TODO: OnEnvironmentChanged event should be called in an optimizied way for any change in the environment

UCLASS(Blueprintable)
class UConvaiEnvironment : public UObject
{
	GENERATED_BODY()
public:

	DECLARE_DELEGATE(FEnvironmentChangedEventSignature);

	FEnvironmentChangedEventSignature OnEnvironmentChanged;

	// Creates a Convai Environment object.
	UFUNCTION(BlueprintCallable, Category = "Convai|Action API")
	static UConvaiEnvironment* CreateConvaiEnvironment()
	{
		UConvaiEnvironment* ContextObject = NewObject<UConvaiEnvironment>();
		return ContextObject;
	}

	void SetFromEnvironment(UConvaiEnvironment* InEnvironment)
	{
		if (IsValid(InEnvironment))
		{
			Objects = InEnvironment->Objects;
			Characters = InEnvironment->Characters;
			Actions = InEnvironment->Actions;
			MainCharacter = InEnvironment->MainCharacter;
			OnEnvironmentChanged.ExecuteIfBound();
		}
	}

	void SetFromEnvironment(FConvaiEnvironmentDetails InEnvironment)
	{
		Objects = InEnvironment.Objects;
		Characters = InEnvironment.Characters;
		Actions = InEnvironment.Actions;
		MainCharacter = InEnvironment.MainCharacter;
		OnEnvironmentChanged.ExecuteIfBound();
	}

	FConvaiEnvironmentDetails ToEnvironmentStruct()
	{
		FConvaiEnvironmentDetails OutStruct;
		OutStruct.Objects = Objects;
		OutStruct.Characters = Characters;
		OutStruct.Actions = Actions;
		OutStruct.MainCharacter = MainCharacter;
		return OutStruct;
	}

	UFUNCTION(BlueprintCallable, category = "Convai|Action API")
		void AddAction(FString Action)
	{
		Actions.AddUnique(Action);
	}

	UFUNCTION(BlueprintCallable, category = "Convai|Action API")
		void AddActions(TArray<FString> ActionsToAdd)
	{
		for (auto a : ActionsToAdd)
			Actions.AddUnique(a);
		OnEnvironmentChanged.ExecuteIfBound();
	}

	UFUNCTION(BlueprintCallable, category = "Convai|Action API")
		void RemoveAction(FString Action)
	{
		Actions.Remove(Action);
	}

	UFUNCTION(BlueprintCallable, category = "Convai|Action API")
		void RemoveActions(TArray<FString> ActionsToRemove)
	{
		for (auto a : ActionsToRemove)
			Actions.Remove(a);
		OnEnvironmentChanged.ExecuteIfBound();
	}

	UFUNCTION(BlueprintCallable, category = "Convai|Action API")
		void ClearAllActions()
	{
		Actions.Empty();
		OnEnvironmentChanged.ExecuteIfBound();
	}

	bool FindObject(FString ObjectName, FConvaiObjectEntry& OutObject)
	{
		for (FConvaiObjectEntry& o : Objects)
		{
			if (ObjectName == o.Name)
			{
				OutObject = o;
				return true;
			}
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, category = "Convai|Action API")
	void AddObject(FConvaiObjectEntry Object)
	{
		// Replace old object that has the same name with the new object
		FConvaiObjectEntry ExistingObject;
		if (FindObject(Object.Name, ExistingObject))
		{
			ExistingObject.Description = Object.Description;
			ExistingObject.OptionalPositionVector = Object.OptionalPositionVector;
			ExistingObject.Ref = Object.Ref;
		}
		else
		{
			Objects.AddUnique(Object);
		}
	}

	/**
	 *    Adds a list of objects to the Environment object
	 *    @param Objects			A map of objects following this format [UObject refrence : Description]
	 */
	UFUNCTION(BlueprintCallable, category = "Convai|Action API")
		void AddObjects(TArray<FConvaiObjectEntry> ObjectsToAdd)
	{
		for (auto o : ObjectsToAdd)
			AddObject(o);
		OnEnvironmentChanged.ExecuteIfBound();
	}

	UFUNCTION(BlueprintCallable, category = "Convai|Action API")
		void RemoveObject(FString ObjectName)
	{
		for (auto o : Objects)
			if (ObjectName == o.Name)
				Objects.Remove(o);
	}

	UFUNCTION(BlueprintCallable, category = "Convai|Action API")
		void RemoveObjects(TArray<FString> ObjectNamesToRemove)
	{
		for (auto n : ObjectNamesToRemove)
			RemoveObject(n);
		OnEnvironmentChanged.ExecuteIfBound();
	}

	UFUNCTION(BlueprintCallable, category = "Convai|Action API")
		void ClearObjects()
	{
		Objects.Empty();
		OnEnvironmentChanged.ExecuteIfBound();
	}

	bool FindCharacter(FString CharacterName, FConvaiObjectEntry& OutCharacter)
	{
		for (FConvaiObjectEntry& c : Characters)
		{
			if (CharacterName == c.Name)
			{
				OutCharacter = c;
				return true;
			}
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, category = "Convai|Action API")
		void AddCharacter(FConvaiObjectEntry Character)
	{
		// Replace old character that has the same name with the new character
		FConvaiObjectEntry ExistingCharacter;
		if (FindCharacter(Character.Name, ExistingCharacter))
		{
			ExistingCharacter.Description = Character.Description;
			ExistingCharacter.OptionalPositionVector = Character.OptionalPositionVector;
			ExistingCharacter.Ref = Character.Ref;
		}
		else
		{
			Characters.AddUnique(Character);
		}
	}


	/**
		*    Adds a list of characters to the Environment object
		*    @param Characters			A map of Characters following this format [Name : Bio]
		*/
	UFUNCTION(BlueprintCallable, category = "Convai|Action API")
		void AddCharacters(TArray<FConvaiObjectEntry> CharactersToAdd)
	{
		for (auto c : CharactersToAdd)
			AddCharacter(c);
		OnEnvironmentChanged.ExecuteIfBound();
	}

	UFUNCTION(BlueprintCallable, category = "Convai|Action API")
		void RemoveCharacter(FString CharacterName)
	{
		for (auto c : Characters)
			if (CharacterName == c.Name)
				Characters.Remove(c);
	}

	UFUNCTION(BlueprintCallable, category = "Convai|Action API")
		void RemoveCharacters(TArray<FString> CharacterNamesToRemove)
	{
		for (auto n : CharacterNamesToRemove)
			RemoveCharacter(n);
	}

	UFUNCTION(BlueprintCallable, category = "Convai|Action API")
		void ClearCharacters()
	{
		Characters.Empty();
		OnEnvironmentChanged.ExecuteIfBound();
	}

	// Assigns the main character initiating the conversation, typically the player character, unless the dialogue involves non-player characters talking to each other.
	UFUNCTION(BlueprintCallable, category = "Convai|Action API")
	void SetMainCharacter(FConvaiObjectEntry InMainCharacter)
	{
		MainCharacter = InMainCharacter;
		AddCharacter(MainCharacter);
		OnEnvironmentChanged.ExecuteIfBound();
	}

	UFUNCTION(BlueprintCallable, category = "Convai|Action API")
	void ClearMainCharacter()
	{
		MainCharacter = FConvaiObjectEntry();
	}


	UPROPERTY(BlueprintReadOnly, category = "Convai|Action API")
		TArray<FString> Actions;

	UPROPERTY(BlueprintReadOnly, category = "Convai|Action API")
		TArray<FConvaiObjectEntry> Objects;

	UPROPERTY(BlueprintReadOnly, category = "Convai|Action API")
		TArray<FConvaiObjectEntry> Characters;

	UPROPERTY(BlueprintReadOnly, category = "Convai|Action API")
	FConvaiObjectEntry MainCharacter;
};

UCLASS(Blueprintable)
class UConvaiActionContext : public UConvaiEnvironment
{
	GENERATED_BODY()
public:
	UFUNCTION(Meta=(DeprecatedFunction, DeprecationMessage = "Use \"Create Convai Environment\" Instead"), BlueprintCallable, Category = "Convai|Action API|DEPRECATED")
		static UConvaiActionContext* CreateConvaiActionContext()
	{
		UConvaiActionContext* ContextObject = NewObject<UConvaiActionContext>();
		return ContextObject;
	}
};

namespace ConvaiConstants
{
	enum
	{
		// Buffer sizes
		VoiceCaptureRingBufferCapacity = 1024 * 1024,
		VoiceCaptureBufferSize = 1024 * 1024,
		LipSyncBufferSize = 1024 * 100, // aproximately 1024 seconds for OVR
		VoiceCaptureSampleRate = 16000,
		VoiceCaptureChunk = 2084,
		VoiceStreamMaxChunk = 4096,
		PlayerTimeOut = 2500 /* 2500 ms*/,
		ChatbotTimeOut = 6000 /* 6000 ms*/
	};

	const TArray<FString> BlendShapesNames = { "EyeBlinkLeft", "EyeLookDownLeft", "EyeLookInLeft", "EyeLookOutLeft", "EyeLookUpLeft", "EyeSquintLeft", "EyeWideLeft", "EyeBlinkRight", "EyeLookDownRight", "EyeLookInRight", "EyeLookOutRight", "EyeLookUpRight", "EyeSquintRight", "EyeWideRight", "JawForward", "JawLeft", "JawRight", "JawOpen", "MouthClose", "MouthFunnel", "MouthPucker", "MouthLeft", "MouthRight", "MouthSmileLeft", "MouthSmileRight", "MouthFrownLeft", "MouthFrownRight", "MouthDimpleLeft", "MouthDimpleRight", "MouthStretchLeft", "MouthStretchRight", "MouthRollLower", "MouthRollUpper", "MouthShrugLower", "MouthShrugUpper", "MouthPressLeft", "MouthPressRight", "MouthLowerDownLeft", "MouthLowerDownRight", "MouthUpperUpLeft", "MouthUpperUpRight", "BrowDownLeft", "BrowDownRight", "BrowInnerUp", "BrowOuterUpLeft", "BrowOuterUpRight", "CheekPuff", "CheekSquintLeft", "CheekSquintRight", "NoseSneerLeft", "NoseSneerRight", "TongueOut", "HeadRoll", "HeadPitch", "HeadYaw" };
	const TArray<FString> VisemeNames = { "sil", "PP", "FF", "TH", "DD", "kk", "CH", "SS", "nn", "RR", "aa", "E", "ih", "oh", "ou" };

	//char* AvatarDefaultImage2 = "https://convai.com/_next/static/images/placeholder-3d-cab6463359f6ccedb4cda311c4056788.jpg";
};