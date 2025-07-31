// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RestAPI/ConvaiAPIBase.h"
#include "ConvaiDefinitions.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ConvaiLTMProxy.generated.h"

struct FConvaiSpeakerInfo;

DECLARE_LOG_CATEGORY_EXTERN(LTMHttpLogs, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSpeakerIDListHttpResponseCallbackSignature, const TArray<FConvaiSpeakerInfo>&, SpeakerIDs);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSpeakerIDHttpResponseCallbackSignature, const FConvaiSpeakerInfo&, SpeakerID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLTMStatusHttpResponseCallbackSignature, bool, Status);



// Create speaker id
UCLASS()
class CONVAI_API UConvaiCreateSpeakerID : public UConvaiAPIBaseProxy
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FSpeakerIDHttpResponseCallbackSignature OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FSpeakerIDHttpResponseCallbackSignature OnFailure;

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DisplayName = "Convai Create Speaker ID"), Category = "Convai|LTM")
	static UConvaiCreateSpeakerID* ConvaiCreateSpeakerIDProxy(FString SpeakerName, FString DeviceId);

protected:
	virtual bool ConfigureRequest(TSharedRef<CONVAI_HTTP_REQUEST_INTERFACE> Request, const TCHAR* Verb) override;
	virtual bool AddContentToRequest(CONVAI_HTTP_PAYLOAD_ARRAY_TYPE& DataToSend, const FString& Boundary)  override { return false; }
	virtual bool AddContentToRequestAsString(TSharedPtr<FJsonObject>& ObjectToSend) override;
	virtual void HandleSuccess() override;
	virtual void HandleFailure() override;

	FString AssociatedSpeakerName;
	FString AssociatedDeviceId;
	FConvaiSpeakerInfo AssociatedSpeakerInfo;
};
// END create speaker id




// List speaker ids
UCLASS()
class CONVAI_API UConvaiListSpeakerID : public UConvaiAPIBaseProxy
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FSpeakerIDListHttpResponseCallbackSignature OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FSpeakerIDListHttpResponseCallbackSignature OnFailure;

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DisplayName = "Convai List Speaker IDs"), Category = "Convai|LTM")
	static UConvaiListSpeakerID* ConvaiListSpeakerIDProxy();

protected:
	virtual bool ConfigureRequest(TSharedRef<CONVAI_HTTP_REQUEST_INTERFACE> Request, const TCHAR* Verb) override;
	virtual bool AddContentToRequest(CONVAI_HTTP_PAYLOAD_ARRAY_TYPE& DataToSend, const FString& Boundary)  override { return false; }
	virtual bool AddContentToRequestAsString(TSharedPtr<FJsonObject>& ObjectToSend) override { return false; }
	virtual void HandleSuccess() override;
	virtual void HandleFailure() override;
};
// END list speaker ids






// Delete speaker id
UCLASS()
class CONVAI_API UConvaiDeleteSpeakerID : public UConvaiAPIBaseProxy
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FStringHttpResponseCallbackSignature OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FStringHttpResponseCallbackSignature OnFailure;

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DisplayName = "Convai Delete Speaker ID"), Category = "Convai|LTM")
	static UConvaiDeleteSpeakerID* ConvaiDeleteSpeakerIDProxy(FString SpeakerID);

protected:
	virtual bool ConfigureRequest(TSharedRef<CONVAI_HTTP_REQUEST_INTERFACE> Request, const TCHAR* Verb) override;
	virtual bool AddContentToRequest(CONVAI_HTTP_PAYLOAD_ARRAY_TYPE& DataToSend, const FString& Boundary)  override { return false; }
	virtual bool AddContentToRequestAsString(TSharedPtr<FJsonObject>& ObjectToSend) override;
	virtual void HandleSuccess() override;
	virtual void HandleFailure() override;

	FString AssociatedSpeakerID;
};
// END Delete speaker id






// Get LTM status
UCLASS()
class CONVAI_API UConvaiGetLTMStatus : public UConvaiAPIBaseProxy
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FLTMStatusHttpResponseCallbackSignature OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FLTMStatusHttpResponseCallbackSignature OnFailure;

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DisplayName = "Convai Get LTM Status"), Category = "Convai|LTM")
	static UConvaiGetLTMStatus* ConvaiGetLTMStatusProxy(FString CharacterID);

protected:
	virtual bool ConfigureRequest(TSharedRef<CONVAI_HTTP_REQUEST_INTERFACE> Request, const TCHAR* Verb) override;
	virtual bool AddContentToRequest(CONVAI_HTTP_PAYLOAD_ARRAY_TYPE& DataToSend, const FString& Boundary)  override { return false; }
	virtual bool AddContentToRequestAsString(TSharedPtr<FJsonObject>& ObjectToSend) override;
	virtual void HandleSuccess() override;
	virtual void HandleFailure() override;

	FString AssociatedCharacterID;
};
// END Get LTM status






// Set LTM status
UCLASS()
class CONVAI_API UConvaiSetLTMStatus : public UConvaiAPIBaseProxy
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FStringHttpResponseCallbackSignature OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FStringHttpResponseCallbackSignature OnFailure;

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DisplayName = "Convai Set LTM Status"), Category = "Convai|LTM")
	static UConvaiSetLTMStatus* ConvaiSetLTMStatusProxy(FString CharacterID, bool bEnable);

protected:
	virtual bool ConfigureRequest(TSharedRef<CONVAI_HTTP_REQUEST_INTERFACE> Request, const TCHAR* Verb) override;
	virtual bool AddContentToRequest(CONVAI_HTTP_PAYLOAD_ARRAY_TYPE& DataToSend, const FString& Boundary)  override { return false; }
	virtual bool AddContentToRequestAsString(TSharedPtr<FJsonObject>& ObjectToSend) override;
	virtual void HandleSuccess() override;
	virtual void HandleFailure() override;

	FString AssociatedCharacterID;
	bool bAssociatedEnable;
};
// END Set LTM status






UCLASS()
class CONVAI_API UConvaiLTMUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static bool ParseConvaiSpeakerInfoArray(const FString& JsonString, TArray<FConvaiSpeakerInfo>& OutSpeakerInfoArray);
	static bool GetLTMStatus(const FString& JsonString, bool& bEnabled);
};