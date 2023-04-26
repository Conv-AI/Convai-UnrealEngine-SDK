// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once


#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ConvaiAndroid.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(ConvaiAndroidLog, Log, All);


UCLASS()
class UConvaiAndroid : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
		
public:

		UFUNCTION(BlueprintCallable, Category = "Convai|Android|Permissions")
		static void ConvaiAndroidAskMicrophonePermission();

		UFUNCTION(BlueprintCallable, Category = "Convai|Android|Permissions")
		static bool ConvaiAndroidHasMicrophonePermission();
};