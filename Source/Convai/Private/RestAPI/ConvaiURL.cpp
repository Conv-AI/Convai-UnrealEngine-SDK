
#include "RestAPI/ConvaiURL.h"

// Define static members
const TCHAR UConvaiURL::BETA_SUBDOMAIN[] = TEXT("beta");
const TCHAR UConvaiURL::PROD_SUBDOMAIN[] = TEXT("api");
const TCHAR UConvaiURL::BASE_URL[] = TEXT("https://%s.convai.com/");

const TCHAR UConvaiURL::LTM_SUBDOMAIN[] = TEXT("user/speaker/");
const TCHAR UConvaiURL::USER_SUBDOMAIN[] = TEXT("user/");
const TCHAR UConvaiURL::CHARACTER_SUBDOMAIN[] = TEXT("character/");
const TCHAR UConvaiURL::NARRATIVE_DESIGN_SUBDOMAIN[] = TEXT("character/narrative/");

TArray<EConvaiEndpoint> UConvaiURL::BetaEndpoints;

FString UConvaiURL::GetEndpoint(EConvaiEndpoint Endpoint)
{
    FString Api;
    switch (Endpoint)
    {
    case EConvaiEndpoint::NewSpeaker:
        Api = FString(LTM_SUBDOMAIN) + TEXT("new");
        break;
    case EConvaiEndpoint::SpeakerIDList:
        Api = FString(LTM_SUBDOMAIN) + TEXT("list");
        break;
    case EConvaiEndpoint::DeleteSpeakerID:
        Api = FString(LTM_SUBDOMAIN) + TEXT("delete");
        break;
    case EConvaiEndpoint::ReferralSourceStatus:
        Api = FString(USER_SUBDOMAIN) + TEXT("referral-source-status");
        break;
    case EConvaiEndpoint::UpdateReferralSource:
        Api = FString(USER_SUBDOMAIN) + TEXT("update-source");
        break;
    case EConvaiEndpoint::UserAPIUsage:
        Api = FString(USER_SUBDOMAIN) + TEXT("user-api-usage");
        break;
    case EConvaiEndpoint::CharacterUpdate:
        Api = FString(CHARACTER_SUBDOMAIN) + TEXT("update");
        break;
    case EConvaiEndpoint::CharacterGet:
        Api = FString(CHARACTER_SUBDOMAIN) + TEXT("get");
        break;
    case EConvaiEndpoint::ListCharacterSections:
        Api = FString(NARRATIVE_DESIGN_SUBDOMAIN) + TEXT("list-sections");
        break;
    case EConvaiEndpoint::ListCharacterTriggers:
        Api = FString(NARRATIVE_DESIGN_SUBDOMAIN) + TEXT("list-triggers");
        break;
    default:
        UE_LOG(LogTemp, Warning, TEXT("Invalid endpoint!"));
        return FString();
    }

    bool bOnProd = !BetaEndpoints.Contains(Endpoint);
    FString Subdomain = bOnProd ? FString(PROD_SUBDOMAIN) : FString(BETA_SUBDOMAIN);

    return FString::Printf(BASE_URL, *Subdomain) + Api;
}
