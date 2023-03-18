// Copyright 2022 Convai Inc. All Rights Reserved.

#include "Convai.h"
#include "Developer/Settings/Public/ISettingsModule.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"

IMPLEMENT_MODULE(Convai, Convai);

#define LOCTEXT_NAMESPACE "Convai"

void Convai::StartupModule()
{
	ConvaiSettings = NewObject<UConvaiSettings>(GetTransientPackage(), "ConvaiSettings", RF_Standalone);
	ConvaiSettings->AddToRoot();

	// Register settings
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->RegisterSettings("Project", "Plugins", "Convai",
			LOCTEXT("RuntimeSettingsName", "Convai"),
			LOCTEXT("RuntimeSettingsDescription", "Configure Convai settings"),
			ConvaiSettings);
	}
}

void Convai::ShutdownModule()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "Convai");
	}

	if (!GExitPurge)
	{
		ConvaiSettings->RemoveFromRoot();
	}
	else
	{
		ConvaiSettings = nullptr;
	}
}

UConvaiSettings* Convai::GetConvaiSettings() const
{
	check(ConvaiSettings);
	return ConvaiSettings;
}

#undef LOCTEXT_NAMESPACE