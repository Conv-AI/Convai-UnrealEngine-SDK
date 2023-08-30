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

UCLASS(Blueprintable)
class UConvaiEnvironment : public UObject
{
	GENERATED_BODY()
public:
	// Creates a Convai Environment object.
	UFUNCTION(BlueprintCallable, Category = "Convai|Action API")
	static UConvaiEnvironment* CreateConvaiEnvironment()
	{
		UConvaiEnvironment* ContextObject = NewObject<UConvaiEnvironment>();
		return ContextObject;
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
	}

	UFUNCTION(BlueprintCallable, category = "Convai|Action API")
		void ClearAllActions()
	{
		Actions.Empty();
	}

	UFUNCTION(BlueprintCallable, category = "Convai|Action API")
		void AddObject(FConvaiObjectEntry Object)
	{
		// Replace old object that has the same name with the new object
		RemoveObject(Object.Name);
		Objects.AddUnique(Object);
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
	}

	UFUNCTION(BlueprintCallable, category = "Convai|Action API")
		void ClearObjects()
	{
		Objects.Empty();
	}

	UFUNCTION(BlueprintCallable, category = "Convai|Action API")
		void AddCharacter(FConvaiObjectEntry Character)
	{
		// Replace old character that has the same name with the new character
		RemoveCharacter(Character.Name);
		Characters.AddUnique(Character);
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
	}

	// Assigns the main character initiating the conversation, typically the player character, unless the dialogue involves non-player characters talking to each other.
	UFUNCTION(BlueprintCallable, category = "Convai|Action API")
		void SetMainCharacter(FConvaiObjectEntry InMainCharacter)
	{
		MainCharacter = InMainCharacter;
		AddCharacter(MainCharacter);
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
		VoiceCaptureSampleRate = 16000,
		VoiceCaptureChunk = 2084,
		VoiceStreamMaxChunk = 4096,
		PlayerTimeOut = 2500 /* 2500 ms*/,
		ChatbotTimeOut = 6000 /* 6000 ms*/
	};

	//char* AvatarDefaultImage2 = "https://convai.com/_next/static/images/placeholder-3d-cab6463359f6ccedb4cda311c4056788.jpg";
};