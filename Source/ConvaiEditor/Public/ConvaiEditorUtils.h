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

	// ---------------------------------CCPack---------------------------------------------------------------------

	/**
	 * Begins a transaction in the editor and retrieves the currently selected assets.
	 *
	 * @param Context A string describing the context of the transaction.
	 * @param Description A text description of the transaction.
	 * @return An array of selected assets as UObject references.
	 */
	UFUNCTION(BlueprintCallable, Category = "Convai|Editor")
	static TArray<UObject*> BeginTransactionAndGetSelectedAssets(const FString& Context, const FText& Description);

	/**
   * Saves the loaded assets and ends the current transaction.
   * @param LoadedAssets An array of assets to save.
   */
	UFUNCTION(BlueprintCallable, Category = "Convai|Editor")
	static void SaveLoadedAssetAndEndTransaction(const TArray<UObject*>& LoadedAssets);

	// ---------------------------------END CCPack---------------------------------------------------------------------
};
 