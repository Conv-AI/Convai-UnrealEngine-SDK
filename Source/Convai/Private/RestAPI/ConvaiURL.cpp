#include "RestAPI/ConvaiURL.h"
#include "Misc/CommandLine.h"
#include "../Convai.h"
#include "Utility/Log/ConvaiLogger.h"

// Define static members
const TCHAR UConvaiURL::BETA_SUBDOMAIN[] = TEXT("beta");
const TCHAR UConvaiURL::PROD_SUBDOMAIN[] = TEXT("api");
const TCHAR UConvaiURL::BASE_URL[] = TEXT("https://%s.convai.com/");
const TCHAR UConvaiURL::BASE_URL_FORMAT[] = TEXT("https://{0}.convai.com/");

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
#if UE_BUILD_SHIPPING
    if (bURLConfigInitialized)
    {
        return;
    }
#endif

    // First check settings
    FString SettingsBetaURL = Convai::Get().GetConvaiSettings()->CustomBetaURL;
    SettingsBetaURL.TrimEndInline();
    SettingsBetaURL.TrimStartInline();
    if (!SettingsBetaURL.IsEmpty())
    {
        CustomBetaBaseURL = SettingsBetaURL;
        CONVAI_LOG(LogTemp, Log, TEXT("Using beta URL from settings: %s"), *CustomBetaBaseURL);
    }

    FString SettingsProdURL = Convai::Get().GetConvaiSettings()->CustomProdURL;
    SettingsProdURL.TrimEndInline();
    SettingsProdURL.TrimStartInline();
    if (!SettingsProdURL.IsEmpty())
    {
        CustomProdBaseURL = SettingsProdURL;
        CONVAI_LOG(LogTemp, Log, TEXT("Using prod URL from settings: %s"), *CustomProdBaseURL);
    }

    // Then check command line parameters (these will override settings if present)
    FString BetaURL;
    if (FParse::Value(FCommandLine::Get(), TEXT("ConvaiBetaURL="), BetaURL))
    {
        CustomBetaBaseURL = BetaURL;
        CONVAI_LOG(LogTemp, Log, TEXT("Using custom beta URL from command line: %s"), *CustomBetaBaseURL);
    }

    FString ProdURL;
    if (FParse::Value(FCommandLine::Get(), TEXT("ConvaiProdURL="), ProdURL))
    {
        CustomProdBaseURL = ProdURL;
        CONVAI_LOG(LogTemp, Log, TEXT("Using custom prod URL from command line: %s"), *CustomProdBaseURL);
    }

    bURLConfigInitialized = true;
}

FString UConvaiURL::GetBaseURL(const bool bUseBeta)
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

FString UConvaiURL::GetFullURL(const FString& ApiPath, const bool bUseBeta)
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

FString UConvaiURL::GetFormattedBaseURL(const FString& Subdomain)
{
    return FString::Format(BASE_URL_FORMAT, { Subdomain });
}

FString UConvaiURL::GetEndpoint(const EConvaiEndpoint Endpoint)
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
        CONVAI_LOG(LogTemp, Warning, TEXT("Invalid endpoint!"));
        return FString();
    }

    const bool bOnProd = !BetaEndpoints.Contains(Endpoint);
    const FString Subdomain = bOnProd ? FString(PROD_SUBDOMAIN) : FString(BETA_SUBDOMAIN);

    return GetFormattedBaseURL(Subdomain) + Api;
}
