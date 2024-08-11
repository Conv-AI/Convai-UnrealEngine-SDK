// Copyright 2022 Convai Inc. All Rights Reserved.


#include "ConvaiActionUtils.h"
#include "ConvaiUtils.h"
#include "Internationalization/Regex.h"
#include "ConvaiDefinitions.h"

DEFINE_LOG_CATEGORY(ConvaiActionUtilsLog);

namespace
{
	bool FindCharAfterIndex(const FString& SearchString, const char Search, int32& OutIndex, const int32& AfterIndex)
	{
		return SearchString.Left(AfterIndex).FindChar(Search, OutIndex);
	}

	FString RemoveQuotedWords(const FString& Input)
	{

		int32 QuoteStartIndex = -1;
		if (!Input.FindChar('"', QuoteStartIndex))
		{
			return Input;
		}

		FString Result = Input;

		while (Result.FindChar('"', QuoteStartIndex))
		{
			int32 QuoteEndIndex = -1;

			// Start searching for the end quote after the start quote
			if (Result.Left(QuoteStartIndex + 1).FindChar('"', QuoteEndIndex) && QuoteEndIndex > QuoteStartIndex)
			{
				// Remove the quoted string, including the quotes
				Result.RemoveAt(QuoteStartIndex, QuoteEndIndex - QuoteStartIndex + 1);
			}
			else
			{
				// No closing quote found after this point, break the loop
				break;
			}
		}

		return Result;
	}

	int32 CountWords(const FString& str)
	{
		if (str.Len() == 0) return 0;

		TArray<FString> words;
		str.ParseIntoArray(words, TEXT(" "), true);

		return words.Num();
	}

	FString KeepNWords(const FString& StringToBeParsed, const FString& CandidateString)
	{
		TArray<FString> words;
		StringToBeParsed.ParseIntoArray(words, TEXT(" "), true);

		int32 N = CountWords(CandidateString);

		FString result;
		for (int32 i = 0; i < FMath::Min(N, words.Num()); i++)
		{
			result += words[i] + " ";
		}

		// Trim trailing space
		result = result.LeftChop(1);

		return result;
	}

	FString FindClosestString(FString Input, const TArray<FString>& StringArray)
	{
		FString ClosestString;
		int32 MinDistance = MAX_int32;

		for (auto& String : StringArray)
		{
			int32 Distance = UConvaiUtils::LevenshteinDistance(Input, String);
			if (Distance < MinDistance)
			{
				MinDistance = Distance;
				ClosestString = String;
			}
		}

		return ClosestString;
	}

	// Helper function to check if a substring is found outside of quotes.
	bool FindSubstringOutsideQuotes(const FString& SearchString, const FString& SubstringToFind)
	{
		bool bInsideQuotes = false;
		int32 SubstringLength = SubstringToFind.Len();
		int32 SearchStringLength = SearchString.Len();

		if (SubstringLength > SearchStringLength || SubstringLength == 0)
		{
			return false;
		}

		for (int32 i = 0; i <= SearchStringLength - SubstringLength; ++i)
		{
			// Toggle the inside quotes flag if a quote is found
			if (SearchString[i] == '"')
			{
				bInsideQuotes = !bInsideQuotes;
				continue;
			}

			// If we are not inside quotes, perform the search
			if (!bInsideQuotes && SearchString.Mid(i, SubstringLength).Equals(SubstringToFind, ESearchCase::IgnoreCase))
			{
				// Make sure the match is not part of a larger string
				if ((i == 0 || !FChar::IsAlnum(SearchString[i - 1])) &&
					(i + SubstringLength == SearchStringLength || !FChar::IsAlnum(SearchString[i + SubstringLength])))
				{
					return true; // Substring found outside of quotes
				}
			}
		}

		return false; // Substring not found or is within quotes
	}

	bool FindClosePhraseOutsideQuotes(const FString& SearchString, const FString& PhraseToFind, int& OutBestDistance, int NumWordsToSkip = 0)
	{
		if (PhraseToFind.IsEmpty())
		{
			return false; // Can't find an empty phrase.
		}

		int32 MaxLevenshteinDistance = FMath::Clamp(PhraseToFind.Len() / 2, 2, 4);
		TArray<FString> Words;
		FString CurrentWord;
		bool bInsideQuotes = false;
		OutBestDistance = MaxLevenshteinDistance + 1; // Initialize with a value above the limit

		// Collect words outside quotes
		for (TCHAR Char : SearchString)
		{
			if (Char == '"')
			{
				bInsideQuotes = !bInsideQuotes;
				if (!CurrentWord.IsEmpty())
				{
					Words.Add(CurrentWord);
					CurrentWord.Empty();
				}
				continue;
			}

			if (!bInsideQuotes && (Char == ' ' || Char == '\t'))
			{
				if (!CurrentWord.IsEmpty())
				{
					Words.Add(CurrentWord);
					CurrentWord.Empty();
				}
			}
			else if (!bInsideQuotes)
			{
				CurrentWord.AppendChar(Char);
			}
		}

		// Add the last word if there is one
		if (!CurrentWord.IsEmpty())
		{
			Words.Add(CurrentWord);
		}

		// Number of words in the phrase to find
		TArray<FString> PhraseWords;
		PhraseToFind.ParseIntoArray(PhraseWords, TEXT(" "), true);
		int32 NumWordsInPhrase = PhraseWords.Num();

		// Sliding window of words to check for the phrase
		for (int32 i = NumWordsToSkip; i <= Words.Num() - NumWordsInPhrase; ++i)
		{
			FString WindowString;
			for (int32 j = 0; j < NumWordsInPhrase; ++j)
			{
				WindowString += (j > 0 ? TEXT(" ") : TEXT("")) + Words[i + j];
			}

			if (WindowString.Len() - PhraseToFind.Len() >= MaxLevenshteinDistance || PhraseToFind.Len() - WindowString.Len() >= MaxLevenshteinDistance)
			{
				continue;
			}

			int32 Distance = UConvaiUtils::LevenshteinDistance(WindowString, PhraseToFind);
			if (Distance <= MaxLevenshteinDistance && Distance < OutBestDistance)
			{
				OutBestDistance = Distance;
			}
		}

		// The function returns true if we found a match within the acceptable Levenshtein distance
		return OutBestDistance <= MaxLevenshteinDistance;
	}

	static bool FindNearestObjectByName(FString SearchString, TArray<FConvaiObjectEntry> Objects, FConvaiObjectEntry& ObjectMatch)
	{
		FString SearchStringLower = SearchString.ToLower();
		bool Found = false;
		int BestDistance = 100;
		for (auto o : Objects)
		{
			FString ObjectNameLower = o.Name.ToLower();
			int Distance = 0;
			if (FindClosePhraseOutsideQuotes(SearchStringLower, ObjectNameLower, Distance))
			{
				if (Distance < BestDistance)
				{
					ObjectMatch = o;
					BestDistance = Distance;
					Found = true;
				}
			}
		}

		// Try to search in objects using each word in the SearchString
		if (!Found)
		{
			TArray<FString> words;
			FString SearchStringWithoutQuotes = RemoveQuotedWords(SearchStringLower);
			SearchStringWithoutQuotes.ParseIntoArray(words, TEXT(" "), true);
			bool breakFromLoop = false;
			for (auto o : Objects)
			{
				if (breakFromLoop)
					break;
				if (CountWords(o.Name) <= 1)
					continue; // consider only names consisting of 1+ words

				FString ObjectNameLower = o.Name.ToLower();
				for (auto w : words)
				{
					if (w.Len() <= 3)
						continue; // consider only words greater than 3 letters

					int Distance = 0;
					if (FindClosePhraseOutsideQuotes(ObjectNameLower, w, Distance))
					{
						ObjectMatch = o;
						BestDistance = Distance;
						Found = true;
						breakFromLoop = true;
						break;
					}
				}
			}
		}

		return Found;
	}

	TArray<FConvaiObjectEntry> BubbleSortEntriesByNumberOfWords(TArray<FConvaiObjectEntry> Entries) 
	{
		bool swapped;
		int n = Entries.Num();
		for (int i = 0; i < n - 1; i++) {
			swapped = false;
			for (int j = 0; j < n - i - 1; j++) {
				if (CountWords(Entries[j].Name) > CountWords(Entries[j+1].Name)) {
					Entries.Swap(j, j + 1);
					swapped = true;
				}
			}
			// If no two elements were swapped by inner loop, then break
			if (!swapped) {
				break;
			}
		}
		return Entries;
	}
};

TArray<FString> UConvaiActions::SmartSplit(const FString& SequenceString)
{
	TArray<FString> Result;
	FString CurrentString = "";
	bool InQuotes = false;

	for (int i = 0; i < SequenceString.Len(); ++i)
	{
		TCHAR CurrentChar = SequenceString[i];

		if (CurrentChar == '\"')
		{
			InQuotes = !InQuotes;
		}

		if (CurrentChar == ',' && !InQuotes)
		{
			Result.Add(CurrentString.TrimStartAndEnd());
			CurrentString = "";
		}
		else
		{
			CurrentString += CurrentChar;
		}
	}

	if (!CurrentString.IsEmpty())
	{
		Result.Add(CurrentString.TrimStartAndEnd());
	}

	return Result;
}

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

FString UConvaiActions::FindAction(FString ActionToBeParsed, TArray<FString> Actions)
{
	FString ClosestAction = "None";
	int32 MinDistance = 4;
	for (auto a : Actions)
	{
		FString TrimmedAction = ActionToBeParsed;
		a = RemoveDesc(a);
		TrimmedAction = KeepNWords(ActionToBeParsed, a);
		int32 Distance = UConvaiUtils::LevenshteinDistance(TrimmedAction, a);
		if (Distance < MinDistance)
		{
			MinDistance = Distance;
			ClosestAction = a;
		}
	}
	return ClosestAction;
}

bool UConvaiActions::ParseAction(UConvaiEnvironment* Environment, FString ActionToBeParsed, FConvaiResultAction& ConvaiResultAction)
{
	FString ActionToAdd;
	FConvaiObjectEntry RelatedObjOrChar;
	ConvaiResultAction.ActionString = ActionToBeParsed;

	// find actions
	ActionToAdd = FindAction(ActionToBeParsed, Environment->Actions);

	TArray<FConvaiObjectEntry> AllCharsAndObjs;
	AllCharsAndObjs.Append(Environment->Characters);
	AllCharsAndObjs.Append(Environment->Objects);

	// find objects or characters
	FindNearestObjectByName(ActionToBeParsed, AllCharsAndObjs, RelatedObjOrChar);
	
	// find objects
	//FindNearestObjectByName(ActionToBeParsed, Environment->Objects, RelatedObjOrChar);

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