// Copyright 2022 Convai Inc. All Rights Reserved.

#include "ConvaiEditor.h"
#include "EditorUtilitySubsystem.h"
#include "Widgets/Input/SButton.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "../Convai.h"
#include "EditorUtilityWidgetBlueprint.h"
#include "Utility/Log/ConvaiLogger.h"

#define LOCTEXT_NAMESPACE "FConvaiEditorModule"

void FConvaiEditorModule::StartupModule()
{
    // Register settings customization
    FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
    PropertyEditor.RegisterCustomClassLayout(
        UConvaiSettings::StaticClass()->GetFName(),
        FOnGetDetailCustomizationInstance::CreateStatic(&FConvaiEditorSettingsCustomization::MakeInstance)
    );

    // Notify customization module
    PropertyEditor.NotifyCustomizationModuleChanged();
}

void FConvaiEditorModule::ShutdownModule()
{
	
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FConvaiEditorModule, ConvaiEditor)




TSharedRef<IDetailCustomization> FConvaiEditorSettingsCustomization::MakeInstance()
{
    return MakeShareable(new FConvaiEditorSettingsCustomization);
}

void FConvaiEditorSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
    IDetailCategoryBuilder& ParentCategory = DetailBuilder.EditCategory(TEXT("Convai API"), FText::FromString("Convai API"));

    ParentCategory.AddCustomRow(FText::FromString(""))
        .WholeRowWidget
        [
            SNew(SHorizontalBox)
        ];

    IDetailCategoryBuilder& SubCategory = DetailBuilder.EditCategory(TEXT("Long Term Memory"), FText::FromString("Long Term Memory"));

    // Add a compact button to the category
    SubCategory.AddCustomRow(FText::FromString("Spawn Tab"))
        .WholeRowWidget
        [
            SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .HAlign(HAlign_Left) 
                .VAlign(VAlign_Center) 
                .AutoWidth() 
                [
                    SNew(SButton)
                        .Text(FText::FromString("Manage Speaker ID"))
                        .HAlign(HAlign_Center) 
                        .VAlign(VAlign_Center)
                        .ContentPadding(FMargin(8.0f, 2.0f)) 
                        .OnClicked(this, &FConvaiEditorSettingsCustomization::OnSpawnTabClicked)
                ]
        ];
}

FReply FConvaiEditorSettingsCustomization::OnSpawnTabClicked()
{
    const FString WidgetPath = TEXT("/ConvAI/Editor/EUW_LTM.EUW_LTM");

    UEditorUtilityWidgetBlueprint* WidgetBlueprint = LoadObject<UEditorUtilityWidgetBlueprint>(nullptr, *WidgetPath);

    if (WidgetBlueprint)
    {
        if (UEditorUtilitySubsystem* Subsystem = GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>())
        {
            Subsystem->SpawnAndRegisterTab(WidgetBlueprint);
            CONVAI_LOG(LogTemp, Log, TEXT("Successfully spawned the Editor Utility Widget: %s"), *WidgetPath);
        }
        else
        {
            CONVAI_LOG(LogTemp, Warning, TEXT("Failed to get Editor Utility Subsystem."));
        }
    }
    else
    {
        CONVAI_LOG(LogTemp, Error, TEXT("Failed to load Editor Utility Widget Blueprint at path: %s"), *WidgetPath);
    }

    return FReply::Handled();
}


