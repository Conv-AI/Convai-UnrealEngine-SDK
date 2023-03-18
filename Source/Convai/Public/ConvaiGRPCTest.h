//// Copyright 2022 Convai Inc. All Rights Reserved.
//
//#pragma once
//
//
//#include "CoreMinimal.h"
//#include "Kismet/BlueprintFunctionLibrary.h"
//#include "ConvaiSubsystem.h"
//
//#include "ConvaiGRPCTest.generated.h"
//
//
//DECLARE_LOG_CATEGORY_EXTERN(ConvaiGRPCTestLog, Log, All);
//
//DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FConvaiSayHelloCallbackSignature, FString, Response);
//
//class USoundWave;
//
///**
// * 
// */
//UCLASS()
//class UConvaiSayHelloProxy : public UConvaiBaseProxy
//{
//	GENERATED_UCLASS_BODY()
//
//	// Called when there is a successful http response
//	UPROPERTY(BlueprintAssignable)
//	FConvaiSayHelloCallbackSignature OnSuccess;
//
//	// Called when there is an unsuccessful http response
//	UPROPERTY(BlueprintAssignable)
//	FConvaiSayHelloCallbackSignature OnFailure;
//
//	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DisplayName = "Convai Test Say Hello", WorldContext = "WorldContextObject"), Category = "Convai|gRPC")
//	static UConvaiSayHelloProxy* CreateConvaiSayHelloQueryProxy(UObject* WorldContextObject, FString Name);
//
//	virtual void Activate() override;
//
//	void OnReceived(bool ok);
//
//	virtual void Success() override;
//	virtual void Failed() override;
//	virtual void Finish() override;
//
//
//	FString Name;
//	FString Response;
//
//	HelloRequest request;
//	HelloResponse reply;
//
//	// Context for the client. It could be used to convey extra information to
//	// the server and/or tweak certain RPC behaviors.
//	ClientContext context;
//
//	// Storage for the status of the RPC upon completion.
//	Status status;
//
//
//	std::unique_ptr<ClientAsyncResponseReader<HelloResponse>> response_reader;
//
//	// Pointer to the world
//	TWeakObjectPtr<UWorld> WorldPtr;
//
//private:
//	FgRPC_Delegate callback;
//};
//
///**
// *
// */
//UCLASS()
//class UConvaiSayHelloStreamProxy : public UConvaiBaseProxy
//{
//	GENERATED_UCLASS_BODY()
//
//	// Called when there is a successful http response
//	UPROPERTY(BlueprintAssignable)
//	FConvaiSayHelloCallbackSignature OnNewData;
//
//	// Called when there is an unsuccessful http response
//	UPROPERTY(BlueprintAssignable)
//	FConvaiSayHelloCallbackSignature OnFailure;
//
//	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DisplayName = "Convai Test Say Hello Stream", WorldContext = "WorldContextObject"), Category = "Convai|gRPC")
//	static UConvaiSayHelloStreamProxy* CreateConvaiSayHelloStreamQueryProxy(UObject* WorldContextObject, TArray<FString> Names);
//
//	virtual void Activate() override;
//
//	void OnStreamInit(bool ok);
//	void OnStreamRead(bool ok);
//	void OnStreamWrite(bool ok);
//
//	virtual void Success() override;
//	virtual void Failed() override;
//	virtual void Finish() override;
//
//
//	TArray<FString> Names;
//	FString Response;
//	int pointer;
//
//	HelloRequest request;
//	HelloResponse reply;
//
//	// Context for the client. It could be used to convey extra information to
//	// the server and/or tweak certain RPC behaviors.
//	ClientContext context;
//
//	// Storage for the status of the RPC upon completion.
//	Status status;
//
//
//	std::unique_ptr<::grpc::ClientAsyncReaderWriter< HelloRequest, HelloResponse>> stream_handler;
//
//	// Pointer to the world
//	TWeakObjectPtr<UWorld> WorldPtr;
//
//private:
//	FgRPC_Delegate OnInitStreamDelegate;
//	FgRPC_Delegate OnStreamReadDelegate;
//	FgRPC_Delegate OnStreamWriteDelegate;
//
//};
