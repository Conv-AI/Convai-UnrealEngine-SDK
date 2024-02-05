// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ConvaiDefinitions.h"
#include "ConvaiActionUtils.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(ConvaiActionUtilsLog, Log, All);

class UConvaiEnvironment;
struct FConvaiResultAction;
struct FConvaiObjectEntry;


UCLASS()
class UConvaiActions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	// Function to split a string based on commas, but ignoring commas within quotes
	static TArray<FString> SmartSplit(const FString& SequenceString);

	// Extract text from an action result (e.g. Says "I love AI" -> I love AI)
	static FString ExtractText(FString Action, FString ActionResult);

	// Extract number from an action result (e.g. Waits for 5 seconds -> 5)
	static float ExtractNumber(FString ActionResult);

	static FString FindAction(FString ActionToBeParsed, TArray<FString> Actions);

	// Removes inner descriptions from a string e.g. (Waits for <time in seconds> becomes Waits for)
	static FString RemoveDesc(FString str);

	static bool ParseAction(UConvaiEnvironment* Environment, FString ActionToBeParsed, FConvaiResultAction& ConvaiResultAction);

	static bool ValidateEnvironment(UConvaiEnvironment* Environment, FString& Error);
};
