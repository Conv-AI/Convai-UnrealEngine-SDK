// Copyright 2022 Convai Inc. All Rights Reserved.

#include "ConvaiAndroid.h"

//#define PLATFORM_ANDROID 1

#if PLATFORM_ANDROID
#include "AndroidPermissionFunctionLibrary.h"
#endif

DEFINE_LOG_CATEGORY(ConvaiAndroidLog);


void UConvaiAndroid::ConvaiAndroidAskMicrophonePermission()
{
#if PLATFORM_ANDROID
	TArray<FString> Permissions;
	Permissions.Add((FString("android.permission.RECORD_AUDIO")));
	UAndroidPermissionFunctionLibrary::AcquirePermissions(Permissions);
#endif
}

bool UConvaiAndroid::ConvaiAndroidHasMicrophonePermission()
{
#if PLATFORM_ANDROID
	return UAndroidPermissionFunctionLibrary::CheckPermission("android.permission.RECORD_AUDIO");
#else
	return true;
#endif
	
}