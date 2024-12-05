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

private:
    static const TCHAR BETA_SUBDOMAIN[];
    static const TCHAR PROD_SUBDOMAIN[];
    static const TCHAR BASE_URL[];

    static const TCHAR LTM_SUBDOMAIN[];
    static const TCHAR USER_SUBDOMAIN[];
    static const TCHAR CHARACTER_SUBDOMAIN[];
    static const TCHAR NARRATIVE_DESIGN_SUBDOMAIN[];

    static TArray<EConvaiEndpoint> BetaEndpoints;
};
