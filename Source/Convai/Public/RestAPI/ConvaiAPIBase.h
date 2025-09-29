// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Net/OnlineBlueprintCallProxyBase.h"
#include "Dom/JsonObject.h"
#include "Runtime/Launch/Resources/Version.h"

#ifdef USE_CONVAI_HTTP
#else
#define USE_CONVAI_HTTP 0
#endif

#if USE_CONVAI_HTTP
// Use ConvaiHttp module
	#include "ConvaihttpModule.h"
	#include "Interfaces/IConvaihttpRequest.h"
	#include "Interfaces/IConvaihttpResponse.h"
	#define CONVAI_HTTP_MODULE FConvaihttpModule
	#define CONVAI_HTTP_REQUEST_INTERFACE IConvaihttpRequest
	#define CONVAI_HTTP_RESPONSE_INTERFACE IConvaihttpResponse
	#define CONVAI_HTTP_REQUEST_PTR FConvaihttpRequestPtr
	#define CONVAI_HTTP_RESPONSE_PTR FConvaihttpResponsePtr
	#define CONVAI_HTTP_PAYLOAD_ARRAY_TYPE TArray64<uint8>
	#define CONVAI_HTTP_DOWN_PROGRESS_TYPE uint64
#else
// Use HTTP module
	#include "HttpModule.h"
	#include "Interfaces/IHttpRequest.h"
	#include "Interfaces/IHttpResponse.h"
	#define CONVAI_HTTP_MODULE FHttpModule
	#define CONVAI_HTTP_REQUEST_INTERFACE IHttpRequest
	#define CONVAI_HTTP_RESPONSE_INTERFACE IHttpResponse
	#define CONVAI_HTTP_REQUEST_PTR FHttpRequestPtr
	#define CONVAI_HTTP_RESPONSE_PTR FHttpResponsePtr
	#define CONVAI_HTTP_PAYLOAD_ARRAY_TYPE TArray<uint8>
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4)
	#define CONVAI_HTTP_DOWN_PROGRESS_TYPE uint64
#else
	#define CONVAI_HTTP_DOWN_PROGRESS_TYPE int32
#endif
#endif

#include "ConvaiAPIBase.generated.h"


DECLARE_LOG_CATEGORY_EXTERN(ConvaiBaseHttpLogs, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FStringHttpResponseCallbackSignature, FString, ResponseString);

namespace ConvaiHttpConstants
{
	static const TCHAR* GET = TEXT("GET");
	static const TCHAR* POST = TEXT("POST");
	static const TCHAR* PUT = TEXT("PUT");
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
	virtual void OnHttpRequestComplete(CONVAI_HTTP_REQUEST_PTR Request, CONVAI_HTTP_RESPONSE_PTR Response, bool bWasSuccessful);
	/*END IHttp interface*/
	
	virtual bool ConfigureRequest(TSharedRef<CONVAI_HTTP_REQUEST_INTERFACE> Request, const TCHAR* Verb);
	virtual bool AddContentToRequest(CONVAI_HTTP_PAYLOAD_ARRAY_TYPE& DataToSend, const FString& Boundary) { return false; }
	virtual bool AddContentToRequestAsString(TSharedPtr<FJsonObject>& ObjectToSend) { return false; }
	virtual void HandleSuccess();
	virtual void HandleFailure();

public:
	FString URL;
	FString ResponseString;
	CONVAI_HTTP_PAYLOAD_ARRAY_TYPE ResponseData; 
};

//--------------------------------------------------------------------------------------------------


/** Base class for all the Convai Asset manager API calls */
UCLASS()
class CONVAI_API UConvaiAPITokenInBodyProxy : public UConvaiAPIBaseProxy
{
	GENERATED_BODY()

protected:
	virtual bool AddContentToRequest(CONVAI_HTTP_PAYLOAD_ARRAY_TYPE& DataToSend, const FString& Boundary) override;
	virtual bool AddContentToRequestAsString(TSharedPtr<FJsonObject>& ObjectToSend) override;
};

