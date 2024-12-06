// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ConvaiDefinitions.h"

#include "ConvaiEditorUtils.generated.h"



/**
 * 
 */
UCLASS()
class CONVAIEDITOR_API UConvaiEditorUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Convai|LTM")
	static void ConvaiAddSpeakerID(const FConvaiSpeakerInfo& Speaker);

	UFUNCTION(BlueprintCallable, Category = "Convai|LTM")
	static void ConvaiRemoveSpeakerID(const FString& SpeakerID);

	//UFUNCTION(BlueprintCallable, Category = "Convai|LTM")
	static void RefreshConvaiSettings();
};
 