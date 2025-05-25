#include "RestAPI/ConvaiURL.h"
#include "Misc/CommandLine.h"

// Define static members
const TCHAR UConvaiURL::BETA_SUBDOMAIN[] = TEXT("beta");
const TCHAR UConvaiURL::PROD_SUBDOMAIN[] = TEXT("api");
const TCHAR UConvaiURL::BASE_URL[] = TEXT("https://%s.convai.com/");

const TCHAR UConvaiURL::LTM_SUBDOMAIN[] = TEXT("user/speaker/");
const TCHAR UConvaiURL::USER_SUBDOMAIN[] = TEXT("user/");
const TCHAR UConvaiURL::CHARACTER_SUBDOMAIN[] = TEXT("character/");
const TCHAR UConvaiURL::NARRATIVE_DESIGN_SUBDOMAIN[] = TEXT("character/narrative/");

TArray<EConvaiEndpoint> UConvaiURL::BetaEndpoints;

FString UConvaiURL::CustomBetaBaseURL = TEXT("");
FString UConvaiURL::CustomProdBaseURL = TEXT("");
bool UConvaiURL::bURLConfigInitialized = false;

void UConvaiURL::InitializeURLConfig()
{
    if (bURLConfigInitialized)
    {
        return;
    }

    // Check command line parameters
    FString BetaURL;
    if (FParse::Value(FCommandLine::Get(), TEXT("ConvaiBetaURL="), BetaURL))
    {
        CustomBetaBaseURL = BetaURL;
        UE_LOG(LogTemp, Log, TEXT("Using custom beta URL: %s"), *CustomBetaBaseURL);
    }

    FString ProdURL;
    if (FParse::Value(FCommandLine::Get(), TEXT("ConvaiProdURL="), ProdURL))
    {
        CustomProdBaseURL = ProdURL;
        UE_LOG(LogTemp, Log, TEXT("Using custom prod URL: %s"), *CustomProdBaseURL);
    }

    bURLConfigInitialized = true;
}

FString UConvaiURL::GetBaseURL(bool bUseBeta)
{
    InitializeURLConfig();
    
    if (bUseBeta)
    {
        if (!CustomBetaBaseURL.IsEmpty())
        {
            return CustomBetaBaseURL;
        }
        return TEXT("https://beta.convai.com");
    }
    else
    {
        if (!CustomProdBaseURL.IsEmpty())
        {
            return CustomProdBaseURL;
        }
        return TEXT("https://api.convai.com");
    }
}

FString UConvaiURL::GetFullURL(const FString& ApiPath, bool bUseBeta)
{
    FString BaseURL = GetBaseURL(bUseBeta);
    
    // Ensure the base URL ends with a slash and the API path doesn't start with one
    if (!BaseURL.EndsWith(TEXT("/")))
    {
        BaseURL += TEXT("/");
    }
    
    FString Path = ApiPath;
    if (Path.StartsWith(TEXT("/")))
    {
        Path = Path.RightChop(1);
    }
    
    return BaseURL + Path;
}

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
