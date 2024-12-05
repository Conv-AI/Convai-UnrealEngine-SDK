
#include "RestAPI/ConvaiAPIBase.h"
#include "ConvaiUtils.h"

DEFINE_LOG_CATEGORY(ConvaiBaseHttpLogs);

// Base api proxy
void UConvaiAPIBaseProxy::Activate()
{
    TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();

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

bool UConvaiAPIBaseProxy::ConfigureRequest(TSharedRef<IHttpRequest> Request, const TCHAR* Verb)
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


    TArray<uint8> DataToSend;
    FString Boundary = TEXT("ConvaiPluginFormBoundary") + FString::FromInt(FDateTime::Now().GetTicks());
    if (AddContentToRequest(DataToSend, Boundary))
    {
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

void UConvaiAPIBaseProxy::OnHttpRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (!Response)
    {
        if (bWasSuccessful)
        {
            UE_LOG(ConvaiBaseHttpLogs, Warning, TEXT("HTTP request succeded - But response pointer is invalid"));
        }
        else
        {
            UE_LOG(ConvaiBaseHttpLogs, Warning, TEXT("HTTP request failed - Response pointer is invalid"));
        }

        HandleFailure();
        return;
    }
    if (!bWasSuccessful || Response->GetResponseCode() < 200 || Response->GetResponseCode() > 299)
    {
        UE_LOG(ConvaiBaseHttpLogs, Warning, TEXT("HTTP request failed with code %d, and with response:%s"), Response->GetResponseCode(), *Response->GetContentAsString());
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
