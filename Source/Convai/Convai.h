// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Convai.generated.h"

UCLASS(config = Engine, defaultconfig)
class CONVAI_API UConvaiSettings : public UObject
{
	GENERATED_BODY()

public:
	UConvaiSettings(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer)
	{
		API_Key = "";
		EnableNewActionSystem = false;
	}
	/* API Key Issued from the website */
	UPROPERTY(Config, EditAnywhere, Category = "Convai API")
	FString API_Key;

	/* Enable new actions system */
	UPROPERTY(Config, EditAnywhere, Category = "Convai API", meta = (DisplayName = "Enable New Action System (Experimental)"))
	bool EnableNewActionSystem;

	/* Authentication token used for Convai Connect */
	UPROPERTY(Config, EditAnywhere, AdvancedDisplay, Category = "Convai API")
	FString AuthToken;

	/* Custom Server URL (Used for debugging) */
	UPROPERTY(Config, EditAnywhere, AdvancedDisplay, Category = "Convai API")
	FString CustomURL;

	/* Test Character ID (Used for debugging) */
	UPROPERTY(Config, EditAnywhere, AdvancedDisplay, Category = "Convai API")
	FString TestCharacterID;

	UPROPERTY(Config, EditAnywhere, AdvancedDisplay, Category = "Convai API")
	bool AllowInsecureConnection;

	/* Extra Parameters (Used for debugging) */
	UPROPERTY(Config, EditAnywhere, AdvancedDisplay, Category = "Convai API")
	FString ExtraParams;
};


class CONVAI_API Convai : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	void StartupModule();
	void ShutdownModule();


	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline Convai& Get()
	{
		return FModuleManager::LoadModuleChecked<Convai>("Convai");
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("Convai");
	}

	virtual bool IsGameModule() const override
	{
		return true;
	}

	/** Getter for internal settings object to support runtime configuration changes */
	UConvaiSettings* GetConvaiSettings() const;


protected:
	/** Module settings */
	UConvaiSettings* ConvaiSettings;
};
