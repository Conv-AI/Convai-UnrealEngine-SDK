// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Internationalization/Regex.h"
#include "ConviDefinitions.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ConvaiActionUtils.generated.h"

UCLASS()
class UConvaiActions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	// Extract text from an action result (e.g. Says "I love AI" -> I love AI)
	static FString ExtractText(FString Action, FString ActionResult)
	{
		FString ExtraText = "";
		FRegexPattern TextPattern(TEXT("\".*\""));
		FRegexMatcher StringMatcher = FRegexMatcher(TextPattern, ActionResult);
		if (StringMatcher.FindNext())
		{
			//UE_LOG(ConvaiUtilsLog, Warning, TEXT("*StringMatcher.GetCaptureGroup(0):%s"), *StringMatcher.GetCaptureGroup(0));
			ExtraText = *StringMatcher.GetCaptureGroup(0).LeftChop(1).RightChop(1);
		}
		else
		{
			int index = ActionResult.Find(Action);
			index += Action.Len();
			ExtraText = ActionResult.RightChop(index + 1);
			//ExtraText = ActionResult;
		}
		return ExtraText;
	}

	// Extract number from an action result (e.g. Waits for 5 seconds -> 5)
	static float ExtractNumber(FString ActionResult)
	{
		float ExtraNumber = 0;
		FRegexPattern NumericPattern(TEXT("\\d+"));
		FRegexMatcher NumberMatcher = FRegexMatcher(NumericPattern, ActionResult);
		if (NumberMatcher.FindNext())
		{
			//UE_LOG(ConvaiGetActionHttpLog, Warning, TEXT("*NumberMatcher.GetCaptureGroup(0):%s"), *NumberMatcher.GetCaptureGroup(0));
			ExtraNumber = FCString::Atof(*NumberMatcher.GetCaptureGroup(0));
		}
		return ExtraNumber;
	}

	static bool ParseAction(UConvaiEnvironment* Environment, FString ActionToBeParsed, FConvaiResultAction& ConvaiResultAction)
	{
		FString ActionToAdd;
		FConvaiObjectEntry RelatedObjOrChar;

		// find actions
		for (auto a : Environment->Actions)
		{
			if (ActionToBeParsed.Find(a) >= 0)
			{
				ActionToAdd = a;
			}
		}

		// find characters
		for (auto c : Environment->Characters)
		{
			if (ActionToBeParsed.Find(c.Name) >= 0)
			{
				RelatedObjOrChar = c;
			}
		}

		// find objects
		for (auto c : Environment->Objects)
		{
			if (ActionToBeParsed.Find(c.Name) >= 0)
			{
				RelatedObjOrChar = c;
			}
		}

		// Find extra numeric param
		float ExtraNumber = ExtractNumber(ActionToBeParsed);


		// Find extra textual param
		FString ExtraText = ExtractText(ActionToAdd, ActionToBeParsed);


		// Add to result action sequence array
		if (ActionToAdd != "")
		{
			ConvaiResultAction.Action = ActionToAdd;
			ConvaiResultAction.RelatedObjectOrCharacter = RelatedObjOrChar;
			ConvaiResultAction.ConvaiExtraParams.Number = ExtraNumber;
			ConvaiResultAction.ConvaiExtraParams.Text = ExtraText;
			return true;
		}
		return false;
	}

	static bool ValidateEnvironment(UConvaiEnvironment* Environment, FString& Error)
	{
		if (!IsValid(Environment))
		{
			Error = "ConvaiActions: Environment object is invalid or not set! You can create it using \"Create Environemnt Object\" function";
			return false;
		}

		if (!Environment->MainCharacter.Ref.IsValid())
		{
			Error = "ConvaiActions: Main Character in the Environment object is invalid or not set! Please set it using \"Set Main Character\" function and use the player pawn as input";
			return false;
		}

		if (Environment->Actions.Num() == 0)
		{
			Error = "ConvaiActions: The Environment object does not have any actions added, please add some actions using \"Add Action\" or \"Add Actions\" function";
			return false;
		}
		return true;
	}
};
