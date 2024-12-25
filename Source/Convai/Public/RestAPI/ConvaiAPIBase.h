// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Http.h"
#include "Interfaces/IHttpResponse.h" 
#include "Net/OnlineBlueprintCallProxyBase.h"
#include "Dom/JsonObject.h" 
#include "ConvaiAPIBase.generated.h"


DECLARE_LOG_CATEGORY_EXTERN(ConvaiBaseHttpLogs, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FStringHttpResponseCallbackSignature, FString, ResponseString);

namespace ConvaiHttpConstants
{
	static const TCHAR* GET = TEXT("GET");
	static const TCHAR* POST = TEXT("POST");
}

//-------------------------------------------Base API class-----------------------------------------

/** Base class for all the Convai API calls */
UCLASS()
class CONVAI_API UConvaiAPIBaseProxy : public UOnlineBlueprintCallProxyBase
{
	GENERATED_BODY()

public:
	virtual void Activate() override;

protected:
	/*IHttp interface*/
	virtual void OnHttpRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	/*END IHttp interface*/

	virtual bool ConfigureRequest(TSharedRef<IHttpRequest> Request, const TCHAR* Verb);
	virtual bool AddContentToRequest(TArray<uint8>& DataToSend, const FString& Boundary) { return false; }
	virtual bool AddContentToRequestAsString(TSharedPtr<FJsonObject>& ObjectToSend) { return false; }
	virtual void HandleSuccess();
	virtual void HandleFailure();

public:
	FString URL;
	FString ResponseString;
	TArray<uint8> ResponseData;
};

//--------------------------------------------------------------------------------------------------