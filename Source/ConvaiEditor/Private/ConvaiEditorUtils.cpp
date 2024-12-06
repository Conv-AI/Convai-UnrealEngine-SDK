// Fill out your copyright notice in the Description page of Project Settings.


#include "ConvaiEditorUtils.h"
#include "../Convai.h"
#include "ISettingsModule.h"

void UConvaiEditorUtils::ConvaiAddSpeakerID(const FConvaiSpeakerInfo& Speaker)
{
    UConvaiSettings* Settings = GetMutableDefault<UConvaiSettings>();
    if (!Settings)
    {
        UE_LOG(LogTemp, Warning, TEXT("ConvaiSettings not found."));
        return;
    }

    int32 Index = Settings->SpeakerIDs.IndexOfByPredicate([&](const FConvaiSpeakerInfo& Info)
        {
            return Info.SpeakerID == Speaker.SpeakerID;
        });

    if (Index == INDEX_NONE)
    {
        Settings->SpeakerIDs.Add(Speaker);
        //UE_LOG(LogTemp, Log, TEXT("Added Speaker: ID=%s, Name=%s"), *Speaker.SpeakerID, *Speaker.Name);
    }

    Settings->SaveConfig(CPF_Config, *Settings->GetDefaultConfigFilename());

    RefreshConvaiSettings();
}

void UConvaiEditorUtils::ConvaiRemoveSpeakerID(const FString& SpeakerID)
{
    UConvaiSettings* Settings = GetMutableDefault<UConvaiSettings>();
    if (!Settings)
    {
        UE_LOG(LogTemp, Warning, TEXT("ConvaiSettings not found."));
        return;
    }

    int32 Index = Settings->SpeakerIDs.IndexOfByPredicate([&](const FConvaiSpeakerInfo& Info)
        {
            return Info.SpeakerID == SpeakerID;
        });

    if (Index != INDEX_NONE)
    {
        Settings->SpeakerIDs.RemoveAt(Index);
        //UE_LOG(LogTemp, Log, TEXT("Removed Speaker: ID=%s"), *SpeakerID);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Speaker ID not found: %s"), *SpeakerID);
    }

    Settings->SaveConfig(CPF_Config, *Settings->GetDefaultConfigFilename());

    RefreshConvaiSettings();
}

#define LOCTEXT_NAMESPACE "Convai"
void UConvaiEditorUtils::RefreshConvaiSettings()
{
    if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
    {
        // Unregister the settings
        SettingsModule->UnregisterSettings("Project", "Plugins", "Convai");

        // Re-register the settings
        UConvaiSettings* Settings = GetMutableDefault<UConvaiSettings>();
        SettingsModule->RegisterSettings("Project", "Plugins", "Convai",
            LOCTEXT("RuntimeSettingsName", "Convai"),
            LOCTEXT("RuntimeSettingsDescription", "Configure Convai settings"),
            Settings);
    }
}
#undef LOCTEXT_NAMESPACE