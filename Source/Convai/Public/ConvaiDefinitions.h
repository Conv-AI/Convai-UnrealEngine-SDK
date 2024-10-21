// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CoreGlobals.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Engine/GameEngine.h"
#include "Runtime/Launch/Resources/Version.h"
#include "ConvaiDefinitions.generated.h"

USTRUCT(BlueprintType)
struct CONVAI_API FNarrativeDecision
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, category = "Convai|Narrative Design")
	FString criteria;

	UPROPERTY(BlueprintReadOnly, category = "Convai|Narrative Design")
	FString next_section_id;

	UPROPERTY(BlueprintReadOnly, category = "Convai|Narrative Design")
	int32 priority;

    FNarrativeDecision()
        : criteria(TEXT("")), // Initialize with default empty string
		next_section_id(TEXT("")), // Initialize with default empty string
		priority(0) // Initialize with default priority
    {
    }
};

USTRUCT(BlueprintType)
struct CONVAI_API FNarrativeTrigger
{
	GENERATED_BODY()

	UPROPERTY()
	FString character_id;

	UPROPERTY(BlueprintReadOnly, category = "Convai|Narrative Design")
	FString destination_section;

	UPROPERTY(BlueprintReadOnly, category = "Convai|Narrative Design")
	FString trigger_id;

	UPROPERTY(BlueprintReadOnly, category = "Convai|Narrative Design")
	FString trigger_message;

	UPROPERTY(BlueprintReadOnly, category = "Convai|Narrative Design")
	FString trigger_name;

	FNarrativeTrigger()
		: destination_section(TEXT("")),
		trigger_id(TEXT("")),
		trigger_message(TEXT("")),
		trigger_name(TEXT(""))
	{
	}
};

USTRUCT(BlueprintType)
struct CONVAI_API FNarrativeSection
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, category = "Convai|Narrative Design")
	FString behavior_tree_code;

	UPROPERTY(BlueprintReadOnly, category = "Convai|Narrative Design")
	FString bt_constants;

	UPROPERTY(BlueprintReadOnly, category = "Convai|Narrative Design")
	FString character_id;

	UPROPERTY(BlueprintReadOnly, category = "Convai|Narrative Design")
	TArray<FNarrativeDecision> decisions;

	UPROPERTY(BlueprintReadOnly, category = "Convai|Narrative Design")
	FString objective;

	UPROPERTY(BlueprintReadOnly, category = "Convai|Narrative Design")
	TArray<FString> parents;

	UPROPERTY(BlueprintReadOnly, category = "Convai|Narrative Design")
	FString section_id;

	UPROPERTY(BlueprintReadOnly, category = "Convai|Narrative Design")
	FString section_name;

	UPROPERTY(BlueprintReadOnly, category = "Convai|Narrative Design")
	TMap<FString, FString> updated_character_data;

    FNarrativeSection()
        : behavior_tree_code(TEXT("")),
		bt_constants(TEXT("")),
		character_id(TEXT("")),
		objective(TEXT("")),
		section_id(TEXT("")),
		section_name(TEXT(""))
    {
    }
};

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
	Joy          UMETA(DisplayName = "Happy"),
	Trust        UMETA(DisplayName = "Calm"),
	Fear         UMETA(DisplayName = "Afraid"),
	Surprise     UMETA(DisplayName = "Surprise"),
	Sadness      UMETA(DisplayName = "Sad"),
	Disgust      UMETA(DisplayName = "Bored"),
	Anger        UMETA(DisplayName = "Angry"),
	Anticipation UMETA(DisplayName = "Anticipation", Hidden), // No longer used
	None         UMETA(DisplayName = "None", Hidden) // To handle cases when the emotion is not found
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

	UPROPERTY()
	int32 FrameRate = 0;

	// Serialize this struct to a JSON string
	FString ToJson() const
	{
		TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);

		// Convert AnimationFrames to a JSON array
		TArray<TSharedPtr<FJsonValue>> JsonFrames;
		for (const FAnimationFrame& Frame : AnimationFrames)
		{
			TSharedPtr<FJsonObject> JsonFrameObject = MakeShareable(new FJsonObject);
			JsonFrameObject->SetNumberField(TEXT("FrameIndex"), Frame.FrameIndex);

			// Convert BlendShapes to a JSON object
			TSharedPtr<FJsonObject> JsonBlendShapes = MakeShareable(new FJsonObject);
			for (const auto& Elem : Frame.BlendShapes)
			{
				JsonBlendShapes->SetNumberField(Elem.Key.ToString(), Elem.Value);
			}
			JsonFrameObject->SetObjectField(TEXT("BlendShapes"), JsonBlendShapes);

			JsonFrames.Add(MakeShareable(new FJsonValueObject(JsonFrameObject)));
		}
		JsonObject->SetArrayField(TEXT("AnimationFrames"), JsonFrames);

		// Set the rest of the properties
		JsonObject->SetNumberField(TEXT("Duration"), Duration);
		JsonObject->SetNumberField(TEXT("FrameRate"), FrameRate);

		FString OutputString;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
		FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

		return OutputString;
	}

	// Deserialize this struct from a JSON string
	bool FromJson(const FString& JsonString)
	{
		AnimationFrames.Empty();
		Duration = 0;
		FrameRate = 0;
		TSharedPtr<FJsonObject> JsonObject;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
		if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
		{
			const TArray<TSharedPtr<FJsonValue>>* JsonFrames;
			if (JsonObject->TryGetArrayField(TEXT("AnimationFrames"), JsonFrames))
			{
				for (const TSharedPtr<FJsonValue>& JsonValue : *JsonFrames)
				{
					TSharedPtr<FJsonObject> JsonFrameObject = JsonValue->AsObject();
					if (JsonFrameObject.IsValid())
					{
						FAnimationFrame Frame;
						Frame.FrameIndex = JsonFrameObject->GetIntegerField(TEXT("FrameIndex"));

						TSharedPtr<FJsonObject> JsonBlendShapes = JsonFrameObject->GetObjectField(TEXT("BlendShapes"));
						for (const auto& Elem : JsonBlendShapes->Values)
						{
							Frame.BlendShapes.Add(FName(*Elem.Key), Elem.Value->AsNumber());
						}

						AnimationFrames.Add(Frame);
					}
				}
			}

#if ENGINE_MAJOR_VERSION < 5
			double tempDuration;
			JsonObject->TryGetNumberField(TEXT("Duration"), tempDuration);
			Duration = (float)tempDuration;
#else
			JsonObject->TryGetNumberField(TEXT("Duration"), Duration);
#endif
			JsonObject->TryGetNumberField(TEXT("FrameRate"), FrameRate);

			return true;
		}
		return false;
	}
};

USTRUCT()
struct FAnimationSequenceBP
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FAnimationSequence AnimationSequence = FAnimationSequence();
};

USTRUCT()
struct FConvaiEmotionState
{
	GENERATED_BODY()

public:
	
	// Deprecated
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

	// Deprecated
	void SetEmotionData(const FString& EmotionRespponse, float EmotionOffset)
	{
		TArray<FString> OutputEmotionsArray;
		// Separate the string into an array based on the space delimiter
		EmotionRespponse.ParseIntoArray(OutputEmotionsArray, TEXT(" "), true);
		SetEmotionData(OutputEmotionsArray, EmotionOffset);
	}

	// Deprecated
	void SetEmotionData(const TArray<FString>& EmotionArray, float EmotionOffset)
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
				Score = *ScoreMultiplier * (FMath::Exp(float(-i) / float(EmotionArray.Num())) + EmotionOffset);
				Score = Score > 1 ? 1 : Score;
				Score = Score < 0 ? 0 : Score;
			}
			else
			{
				Score = 0;
			}

			EmotionsScore.Add(BasicEmotion, Score);
			i++;
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

	void GetTTSEmotion(const FString& Emotion, EBasicEmotions& BasicEmotion)
	{
		// Static dictionaries of emotions
		static const TMap<FString, EBasicEmotions> BasicEmotions = {
			{"Joy", EBasicEmotions::Joy},
			{"Calm", EBasicEmotions::Trust},
			{"Fear", EBasicEmotions::Fear},
			{"Surprise", EBasicEmotions::Surprise},
			{"Sadness", EBasicEmotions::Sadness},
			{"Bored", EBasicEmotions::Disgust},
			{"Anger", EBasicEmotions::Anger},
			{"Neutral", EBasicEmotions::None}
		};

		// Initialize the output parameters
		BasicEmotion = EBasicEmotions::None;

		// Look up the emotion
		if (BasicEmotions.Contains(Emotion))
		{
			BasicEmotion = BasicEmotions[Emotion];
		}
	}


	void SetEmotionDataSingleEmotion(const FString& EmotionRespponse, float EmotionOffset)
	{
		FString EmotionString;
		float Scale;
		float MaxScale = 3;
		ParseStringToStringAndFloat(EmotionRespponse, EmotionString, Scale);


		Scale /= MaxScale;

		// Increase the scale a bit
		Scale += EmotionOffset;
		Scale = Scale > 1? 1 : Scale;
		Scale = Scale < 0? 0 : Scale;

		EBasicEmotions Emotion;
		GetTTSEmotion(EmotionString, Emotion);

		ResetEmotionScores();
		EmotionsScore.Add(Emotion, Scale);
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

	bool ParseStringToStringAndFloat(const FString& Input, FString& OutString, float& OutFloat)
	{
		// Split the input string into two parts based on the space character
		TArray<FString> Parsed;
		Input.ParseIntoArray(Parsed, TEXT(" "), true);

		// Check if the parsing was successful
		if (Parsed.Num() == 2)
		{
			// Assign the first part to OutString
			OutString = Parsed[0];

			// Convert the second part to a float and assign it to OutFloat
			OutFloat = FCString::Atof(*Parsed[1]);
			return true;
		}
		else
		{
			// Handle the error case
			OutString = TEXT("");
			OutFloat = 0.0f;
			return false;
		}
	}
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

	UPROPERTY()
		FConvaiObjectEntry AttentionObject;
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
			AttentionObject = InEnvironment->AttentionObject;
			OnEnvironmentChanged.ExecuteIfBound();
		}
	}

	void SetFromEnvironment(FConvaiEnvironmentDetails InEnvironment)
	{
		Objects = InEnvironment.Objects;
		Characters = InEnvironment.Characters;
		Actions = InEnvironment.Actions;
		MainCharacter = InEnvironment.MainCharacter;
		AttentionObject = InEnvironment.AttentionObject;
		OnEnvironmentChanged.ExecuteIfBound();
	}

	FConvaiEnvironmentDetails ToEnvironmentStruct()
	{
		FConvaiEnvironmentDetails OutStruct;
		OutStruct.Objects = Objects;
		OutStruct.Characters = Characters;
		OutStruct.Actions = Actions;
		OutStruct.MainCharacter = MainCharacter;
		OutStruct.AttentionObject = AttentionObject;
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

	FConvaiObjectEntry* FindObject(FString ObjectName)
	{
		for (FConvaiObjectEntry& o : Objects)
		{
			if (ObjectName == o.Name)
			{
				return &o;
			}
		}
		return nullptr;
	}

	UFUNCTION(BlueprintCallable, category = "Convai|Action API")
		void AddObject(FConvaiObjectEntry Object)
	{
		// Replace old object that has the same name with the new object
		if (FConvaiObjectEntry* ExistingObject = FindObject(Object.Name))
		{
			ExistingObject->Description = Object.Description;
			ExistingObject->OptionalPositionVector = Object.OptionalPositionVector;
			ExistingObject->Ref = Object.Ref;
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

	FConvaiObjectEntry* FindCharacter(FString CharacterName)
	{
		for (FConvaiObjectEntry& c : Characters)
		{
			if (CharacterName == c.Name)
			{
				return &c;
			}
		}
		return nullptr;
	}

	UFUNCTION(BlueprintCallable, category = "Convai|Action API")
		void AddCharacter(FConvaiObjectEntry Character)
	{
		// Replace old character that has the same name with the new character
		if (FConvaiObjectEntry* ExistingCharacter = FindCharacter(Character.Name))
		{
			ExistingCharacter->Description = Character.Description;
			ExistingCharacter->OptionalPositionVector = Character.OptionalPositionVector;
			ExistingCharacter->Ref = Character.Ref;
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
		void SetAttentionObject(FConvaiObjectEntry InAttentionObject)
	{
		AttentionObject = InAttentionObject;
		AddObject(AttentionObject);
		OnEnvironmentChanged.ExecuteIfBound();
	}

	UFUNCTION(BlueprintCallable, category = "Convai|Action API")
		void ClearMainCharacter()
	{
		MainCharacter = FConvaiObjectEntry();
	}

	UFUNCTION(BlueprintCallable, category = "Convai|Action API")
		void ClearAttentionObject()
	{
		AttentionObject = FConvaiObjectEntry();
	}

	UPROPERTY(BlueprintReadOnly, category = "Convai|Action API")
		TArray<FString> Actions;

	UPROPERTY(BlueprintReadOnly, category = "Convai|Action API")
		TArray<FConvaiObjectEntry> Objects;

	UPROPERTY(BlueprintReadOnly, category = "Convai|Action API")
		TArray<FConvaiObjectEntry> Characters;

	UPROPERTY(BlueprintReadOnly, category = "Convai|Action API")
		FConvaiObjectEntry MainCharacter;

	UPROPERTY(BlueprintReadOnly, category = "Convai|Action API")
		FConvaiObjectEntry AttentionObject;
};

UCLASS(Blueprintable)
class UConvaiActionContext : public UConvaiEnvironment
{
	GENERATED_BODY()
public:
	UFUNCTION(Meta = (DeprecatedFunction, DeprecationMessage = "Use \"Create Convai Environment\" Instead"), BlueprintCallable, Category = "Convai|Action API|DEPRECATED")
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

	const TArray<FString> BlendShapesNames = { "browDownLeft", "browDownRight", "browInnerUp", "browOuterUpLeft", "browOuterUpRight", "cheekPuff", "cheekSquintLeft", "cheekSquintRight", "eyeBlinkLeft", "eyeBlinkRight", "eyeLookDownLeft", "eyeLookDownRight", "eyeLookInLeft", "eyeLookInRight", "eyeLookOutLeft", "eyeLookOutRight", "eyeLookUpLeft", "eyeLookUpRight", "eyeSquintLeft", "eyeSquintRight", "eyeWideLeft", "eyeWideRight", "jawForward", "jawLeft", "jawOpen", "jawRight", "mouthClose", "mouthDimpleLeft", "mouthDimpleRight", "mouthFrownLeft", "mouthFrownRight", "mouthFunnel", "mouthLeft", "mouthLowerDownLeft", "mouthLowerDownRight", "mouthPressLeft", "mouthPressRight", "mouthPucker", "mouthRight", "mouthRollLower", "mouthRollUpper", "mouthShrugLower", "mouthShrugUpper", "mouthSmileLeft", "mouthSmileRight", "mouthStretchLeft", "mouthStretchRight", "mouthUpperUpLeft", "mouthUpperUpRight", "noseSneerLeft", "noseSneerRight", "tongueOut", /*"headRoll", "headPitch", "headYaw"*/ };
	const TArray<FString> VisemeNames = { "sil", "PP", "FF", "TH", "DD", "kk", "CH", "SS", "nn", "RR", "aa", "E", "ih", "oh", "ou" };

	const FString API_Key_Header = "CONVAI-API-KEY";
	const FString Auth_Token_Header = "API-AUTH-TOKEN";

	const FString ExperienceUpdateEndpoint = "https://api.convai.com/xp/experiences/update";
	const FString ExperienceSessionDetailEndpoint = "https://api.convai.com/xp/sessions/detail";

	//char* AvatarDefaultImage2 = "https://convai.com/_next/static/images/placeholder-3d-cab6463359f6ccedb4cda311c4056788.jpg";
};


template<typename DelegateType>
class FThreadSafeDelegateWrapper
{
public:
	// Bind a delegate
	void Bind(const DelegateType& InDelegate)
	{
		FScopeLock Lock(&Mutex);
		MyDelegate = InDelegate;
	}

	// Mirror of BindUObject function for non-const UserClass
	template <typename UserClass, typename... VarTypes>
	void BindUObject(UserClass* InUserObject, void(UserClass::* InFunc)(VarTypes...))
	{
		FScopeLock Lock(&Mutex);
		MyDelegate.BindUObject(InUserObject, InFunc);
	}

	// Unbind the delegate
	void Unbind()
	{
		FScopeLock Lock(&Mutex);
		MyDelegate.Unbind();
	}

	// Check if the delegate is bound
	bool IsBound() const
	{
		return MyDelegate.IsBound();
	}

	// Execute the delegate if it is bound
	// Use perfect forwarding to forward arguments to the delegate
	template<typename... ArgTypes>
	void ExecuteIfBound(ArgTypes&&... Args) const
	{
		FScopeLock Lock(&Mutex);
		if (MyDelegate.IsBound() && !IsEngineExitRequested())
		{
			MyDelegate.ExecuteIfBound(Forward<ArgTypes>(Args)...);
		}
	}

private:
	mutable FCriticalSection Mutex;
	DelegateType MyDelegate;
};

// Enum for Voice Types
UENUM(BlueprintType)
enum class EVoiceType : uint8
{
	AzureVoices           UMETA(DisplayName = "Azure Voices"),
	ElevenLabsVoices      UMETA(DisplayName = "ElevenLabs Voices"),
	GCPVoices             UMETA(DisplayName = "GCP Voices"),
	ConvaiVoices          UMETA(DisplayName = "Convai Voices"),
	OpenAIVoices          UMETA(DisplayName = "OpenAI Voices"),
	ConvaiVoicesNew       UMETA(DisplayName = "Convai Voices (New)"),
	ConvaiVoicesExperimental UMETA(DisplayName = "Convai Voices (Experimental)")
};

// Enum for Languages
UENUM(BlueprintType)
enum class ELanguageType : uint8
{
	Arabic               UMETA(DisplayName = "Arabic"),
	ChineseCantonese     UMETA(DisplayName = "Chinese (Cantonese)"),
	ChineseMandarin      UMETA(DisplayName = "Chinese (Mandarin)"),
	Dutch                UMETA(DisplayName = "Dutch"),
	DutchBelgium         UMETA(DisplayName = "Dutch (Belgium)"),
	English              UMETA(DisplayName = "English"),
	Finnish              UMETA(DisplayName = "Finnish"),
	French               UMETA(DisplayName = "French"),
	German               UMETA(DisplayName = "German"),
	Hindi                UMETA(DisplayName = "Hindi"),
	Italian              UMETA(DisplayName = "Italian"),
	Japanese             UMETA(DisplayName = "Japanese"),
	Korean               UMETA(DisplayName = "Korean"),
	Polish               UMETA(DisplayName = "Polish"),
	PortugueseBrazil     UMETA(DisplayName = "Portuguese (Brazil)"),
	PortuguesePortugal   UMETA(DisplayName = "Portuguese (Portugal)"),
	Russian              UMETA(DisplayName = "Russian"),
	Spanish              UMETA(DisplayName = "Spanish"),
	SpanishMexico        UMETA(DisplayName = "Spanish (Mexico)"),
	SpanishUS            UMETA(DisplayName = "Spanish (US)"),
	Swedish              UMETA(DisplayName = "Swedish"),
	Turkish              UMETA(DisplayName = "Turkish"),
	Vietnamese           UMETA(DisplayName = "Vietnamese")
};

// Enum for Gender
UENUM(BlueprintType)
enum class EGenderType : uint8
{
	Male    UMETA(DisplayName = "Male"),
	Female  UMETA(DisplayName = "Female")
};


USTRUCT(BlueprintType)
struct FVoiceLanguageStruct
{
	GENERATED_BODY()
	
	//UPROPERTY(BlueprintReadOnly)
	FString VoiceType;

	//UPROPERTY(BlueprintReadOnly)
	FString VoiceName;

	UPROPERTY(BlueprintReadOnly, category = "Convai|Language")
	FString VoiceValue;

	UPROPERTY(BlueprintReadOnly, category = "Convai|Language")
	TArray<FString> LangCodes;

	//UPROPERTY(BlueprintReadOnly)
	FString Gender;

	FVoiceLanguageStruct() {}
};
