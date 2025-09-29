
#include "RestAPI/ConvaiAPIBase.h"
#include "ConvaiUtils.h"

DEFINE_LOG_CATEGORY(ConvaiBaseHttpLogs);


void UConvaiAPIBaseProxy::Activate()
{
    TSharedRef<CONVAI_HTTP_REQUEST_INTERFACE> Request = CONVAI_HTTP_MODULE::Get().CreateRequest(); 

    if (!ConfigureRequest(Request, TEXT("")))
    {
        HandleFailure();
        return;
    }

    Request->OnProcessRequestComplete().BindUObject(this, &ThisClass::OnHttpRequestComplete);

    if (Request->ProcessRequest())
    {
        AddToRoot();
    }
}

bool UConvaiAPIBaseProxy::ConfigureRequest(TSharedRef<CONVAI_HTTP_REQUEST_INTERFACE> Request, const TCHAR* Verb)
{
    if (!UConvaiFormValidation::ValidateInputText(URL) || !UConvaiFormValidation::ValidateInputText(Verb))
    {
        HandleFailure();
        return false;
    }

    TPair<FString, FString> AuthHeaderAndKey = UConvaiUtils::GetAuthHeaderAndKey();
    FString AuthKey = AuthHeaderAndKey.Value;
    FString AuthHeader = AuthHeaderAndKey.Key;

    if (!UConvaiFormValidation::ValidateAuthKey(AuthKey))
    {
        HandleFailure();
        return false;
    }

    Request->SetURL(URL);
    Request->SetVerb(Verb);
    Request->SetHeader(AuthHeader, AuthKey);

    // Child classes can add filed to this object
    TSharedPtr<FJsonObject> ObjectToSend = MakeShareable(new FJsonObject);
    if (AddContentToRequestAsString(ObjectToSend))
    {
        Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

        FString Content;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Content);
        FJsonSerializer::Serialize(ObjectToSend.ToSharedRef(), Writer);

        Request->SetContentAsString(Content);

        return true;
    }

    CONVAI_HTTP_PAYLOAD_ARRAY_TYPE DataToSend;
    FString Boundary = TEXT("ConvaiPluginFormBoundary") + FString::FromInt(FDateTime::Now().GetTicks());
    if (AddContentToRequest(DataToSend, Boundary))
    {
        if (FString(Verb).Equals(ConvaiHttpConstants::PUT))
        {
            Request->SetHeader(TEXT("Content-Type"), TEXT("application/octet-stream"));
            Request->SetContent(DataToSend);
            return true;
        }
        
        Request->SetHeader(TEXT("Content-Type"), FString::Printf(TEXT("multipart/form-data; boundary=----%s"), *Boundary));

        // Add closing boundary
        FString ClosingBoundary = FString::Printf(TEXT("\r\n------%s--\r\n"), *Boundary);
        DataToSend.Append((uint8*)TCHAR_TO_UTF8(*ClosingBoundary), ClosingBoundary.Len());

        // Set the request content and content length
        Request->SetHeader(TEXT("Content-Length"), FString::FromInt(DataToSend.Num()));
        Request->SetContent(DataToSend);

        return true;
    }

    return true;
}

void UConvaiAPIBaseProxy::OnHttpRequestComplete(CONVAI_HTTP_REQUEST_PTR Request, CONVAI_HTTP_RESPONSE_PTR Response, bool bWasSuccessful)
{
    if (!Response)
    {
        if (bWasSuccessful)
        {
            CONVAI_LOG(ConvaiBaseHttpLogs, Warning, TEXT("HTTP request succeded - But response pointer is invalid"));
        }
        else
        {
            CONVAI_LOG(ConvaiBaseHttpLogs, Warning, TEXT("HTTP request failed - Response pointer is invalid"));
        }

        HandleFailure();
        return;
    }
    if (!bWasSuccessful || Response->GetResponseCode() < 200 || Response->GetResponseCode() > 299)
    {
        CONVAI_LOG(ConvaiBaseHttpLogs, Warning, TEXT("HTTP request failed with code %d, and with response:%s"), Response->GetResponseCode(), *Response->GetContentAsString());
        HandleFailure();
        return;
    }

    ResponseString = Response->GetContentAsString();
    ResponseData = Response->GetContent();
    HandleSuccess();
}

void UConvaiAPIBaseProxy::HandleSuccess()
{
    RemoveFromRoot();
}

void UConvaiAPIBaseProxy::HandleFailure()
{
    RemoveFromRoot();
}
// END Base api proxy






bool UConvaiAPITokenInBodyProxy::AddContentToRequest(CONVAI_HTTP_PAYLOAD_ARRAY_TYPE& DataToSend, const FString& Boundary)
{
    const TPair<FString, FString> AuthHeaderAndKey = UConvaiUtils::GetAuthHeaderAndKey();
    const FString AuthKey = AuthHeaderAndKey.Value;
    const FString AuthHeader = AuthHeaderAndKey.Key;

    if (!UConvaiFormValidation::ValidateAuthKey(AuthKey))
    {
        HandleFailure();
        return false;
    }

    if (AuthHeader == ConvaiConstants::Auth_Token_Header)
    {
        const FString ExpIdField = FString::Printf(TEXT("\r\n------%s\r\nContent-Disposition: form-data; name=\"experience_session_id\"\r\n\r\n%s"), *Boundary, *AuthKey);
        DataToSend.Append((uint8*)TCHAR_TO_UTF8(*ExpIdField), ExpIdField.Len());
    }

    return true;
}

bool UConvaiAPITokenInBodyProxy::AddContentToRequestAsString(TSharedPtr<FJsonObject>& ObjectToSend)
{
    const TPair<FString, FString> AuthHeaderAndKey = UConvaiUtils::GetAuthHeaderAndKey();
    const FString AuthKey = AuthHeaderAndKey.Value;
    const FString AuthHeader = AuthHeaderAndKey.Key;

    if (!UConvaiFormValidation::ValidateAuthKey(AuthKey))
    {
        HandleFailure();
        return false;
    }

    if (AuthHeader == ConvaiConstants::Auth_Token_Header)
    {
        ObjectToSend->SetStringField(TEXT("experience_session_id"), AuthKey);
    }

    return true;
}