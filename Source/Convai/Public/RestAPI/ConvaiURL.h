#pragma once

#include "CoreMinimal.h"
#include "ConvaiURL.generated.h"

UENUM(BlueprintType)
enum class EConvaiEndpoint : uint8
{
    NewSpeaker UMETA(DisplayName = "New Speaker"),
    SpeakerIDList UMETA(DisplayName = "Speaker ID List"),
    DeleteSpeakerID UMETA(DisplayName = "Delete Speaker ID"),
    ReferralSourceStatus UMETA(DisplayName = "Referral Source Status"),
    UpdateReferralSource UMETA(DisplayName = "Update Referral Source"),
    UserAPIUsage UMETA(DisplayName = "User API Usage"),
    CharacterUpdate UMETA(DisplayName = "Character Update"),
    CharacterGet UMETA(DisplayName = "Character Get"),
    ListCharacterSections UMETA(DisplayName = "List Character Sections"),
    ListCharacterTriggers UMETA(DisplayName = "List Character Triggers")
};

UCLASS()
class CONVAI_API UConvaiURL : public UObject
{
    GENERATED_BODY()

public:
    static FString GetEndpoint(EConvaiEndpoint Endpoint);

    /** Get the base URL for a specific environment (beta or prod) */
    static FString GetBaseURL(bool bUseBeta = false);
    
    /** Get the full URL for a specific API endpoint */
    static FString GetFullURL(const FString& ApiPath, bool bUseBeta = false);

    static FString GetFormattedBaseURL(const FString& Subdomain);
    
    /** Initialize URL configuration from command line or settings */
    static void InitializeURLConfig();

private:
    static const TCHAR BETA_SUBDOMAIN[];
    static const TCHAR PROD_SUBDOMAIN[];
    static const TCHAR BASE_URL[];
    static const TCHAR BASE_URL_FORMAT[];
    
    static const TCHAR LTM_SUBDOMAIN[];
    static const TCHAR USER_SUBDOMAIN[];
    static const TCHAR CHARACTER_SUBDOMAIN[];
    static const TCHAR NARRATIVE_DESIGN_SUBDOMAIN[];

    static TArray<EConvaiEndpoint> BetaEndpoints;

    static FString CustomBetaBaseURL;
    static FString CustomProdBaseURL;
    static bool bURLConfigInitialized;
};
