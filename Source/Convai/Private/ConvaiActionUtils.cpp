// Copyright 2022 Convai Inc. All Rights Reserved.


#include "ConvaiActionUtils.h"
#include "Internationalization/Regex.h"
#include "ConviDefinitions.h"

DEFINE_LOG_CATEGORY(ConvaiActionUtilsLog);

FString UConvaiActions::ExtractText(FString Action, FString ActionResult)
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

float UConvaiActions::ExtractNumber(FString ActionResult)
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

FString UConvaiActions::RemoveDesc(FString str)
{
	FRegexPattern DescPattern(TEXT("<.*>"));
	FRegexMatcher DescMatcher = FRegexMatcher(DescPattern, str);
	int i = 0;
	while (DescMatcher.FindNext())
	{
		int start = DescMatcher.GetMatchBeginning();
		str.ReplaceInline(*DescMatcher.GetCaptureGroup(i++), *FString(""));
		str.TrimEndInline();
		str.TrimStartInline();
	}
	//UE_LOG(ConvaiActionUtilsLog, Warning, TEXT("str:%s"), *str);
	return str;
}

bool UConvaiActions::ParseAction(UConvaiEnvironment* Environment, FString ActionToBeParsed, FConvaiResultAction& ConvaiResultAction)
{
	FString ActionToAdd;
	FConvaiObjectEntry RelatedObjOrChar;
	ConvaiResultAction.ActionString = ActionToBeParsed;

	// find actions
	for (auto a : Environment->Actions)
	{
		a = RemoveDesc(a);
		if (ActionToBeParsed.Find(a) >= 0)
		{
			ActionToAdd = a;
		}
	}

	// find characters
	for (auto c : Environment->Characters)
	{
		FString charName = RemoveDesc(c.Name);
		if (ActionToBeParsed.Find(charName) >= 0)
		{
			RelatedObjOrChar = c;
		}
	}

	// find objects
	for (auto o : Environment->Objects)
	{
		FString objName = RemoveDesc(o.Name);
		if (ActionToBeParsed.Find(objName) >= 0)
		{
			RelatedObjOrChar = o;
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

bool UConvaiActions::ValidateEnvironment(UConvaiEnvironment* Environment, FString& Error)
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