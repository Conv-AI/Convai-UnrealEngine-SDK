//// Copyright 2022 Convai Inc. All Rights Reserved.
//
//#include "ConvaiGRPCTest.h"
//#include "Kismet/GameplayStatics.h"
//
//DEFINE_LOG_CATEGORY(ConvaiGRPCTestLog);
//
//
//namespace GRPCTest{
//	UConvaiSubsystem* GetConvaiSubsystem(TWeakObjectPtr<UWorld> WorldPtr)
//	{
//		UWorld* World = WorldPtr.Get();
//
//		if (!World)
//		{
//			UE_LOG(ConvaiGRPCTestLog, Warning, TEXT("Could not get a pointer to world!"));
//			return nullptr;
//		}
//
//		UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(WorldPtr.Get());
//		if (!GameInstance)
//		{
//			UE_LOG(ConvaiSubsystemLog, Warning, TEXT("Could not get pointer to a GameInstance"));
//			return nullptr;
//		}
//
//		
//		if (UConvaiSubsystem * ConvaiSubsystem = GameInstance->GetSubsystem<UConvaiSubsystem>())
//		{
//			return ConvaiSubsystem;
//		}
//		else
//		{
//			UE_LOG(ConvaiSubsystemLog, Warning, TEXT("Could not get pointer to Convai Subsystem"));
//			return nullptr;
//		}
//		
//	}
//
//	char* StatusCodeStr[] =
//	{
//	"OK",
//	"CANCELLED",
//	"UNKNOWN",
//	"INVALID_ARGUMENT",
//	"DEADLINE_EXCEEDED",
//	"NOT_FOUND",
//	"ALREADY_EXISTS",
//	"PERMISSION_DENIED",
//	"UNAUTHENTICATED",
//	"RESOURCE_EXHAUSTED",
//	"FAILED_PRECONDITION",
//	"ABORTED",
//	"OUT_OF_RANGE",
//	"UNIMPLEMENTED",
//	"INTERNAL",
//	"UNAVAILABLE",
//	"DATA_LOSS",
//	"DO_NOT_USE" };
//}
//
//UConvaiSayHelloProxy::UConvaiSayHelloProxy(const FObjectInitializer& ObjectInitializer)
//	: Super(ObjectInitializer)
//	, callback(FgRPC_Delegate::CreateUObject(this, &ThisClass::OnReceived))
//{
//}
//
//UConvaiSayHelloProxy* UConvaiSayHelloProxy::CreateConvaiSayHelloQueryProxy(UObject* WorldContextObject, FString Name)
//{
//	UConvaiSayHelloProxy* Proxy = NewObject<UConvaiSayHelloProxy>();
//	Proxy->WorldPtr = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
//	Proxy->Name = Name;
//
//	return Proxy;
//}
//
//void UConvaiSayHelloProxy::Activate()
//{
//	request.set_name(std::string(TCHAR_TO_UTF8(*Name)));
//
//	if (UConvaiSubsystem* ConvaiSubsystem = GRPCTest::GetConvaiSubsystem(WorldPtr))
//	{
//		ConvaiSubsystem->gRPC_Runnable->SayHello(&callback, response_reader, &context, &status, &request, &reply);
//	}
//}
//
//void UConvaiSayHelloProxy::OnReceived(bool ok)
//{
//	if (!status.ok())
//	{
//		UE_LOG(ConvaiGRPCTestLog, Warning, TEXT("GRPC error %s with message: %s"), *FString(GRPCTest::StatusCodeStr[status.error_code()]), *FString(UTF8_TO_TCHAR(status.error_message().c_str())));
//		Failed();
//		return;
//	}
//
//	Success();
//}
//
//void UConvaiSayHelloProxy::Success()
//{
//	OnSuccess.Broadcast(FString(UTF8_TO_TCHAR(reply.message().c_str())));
//	Finish();
//}
//
//void UConvaiSayHelloProxy::Failed()
//{
//	OnFailure.Broadcast(FString("Failed"));
//	Finish();
//}
//
//void UConvaiSayHelloProxy::Finish()
//{
//}
//
//
//UConvaiSayHelloStreamProxy::UConvaiSayHelloStreamProxy(const FObjectInitializer& ObjectInitializer)
//	: Super(ObjectInitializer)
//	, OnInitStreamDelegate(FgRPC_Delegate::CreateUObject(this, &ThisClass::OnStreamInit))
//	, OnStreamReadDelegate(FgRPC_Delegate::CreateUObject(this, &ThisClass::OnStreamRead))
//	, OnStreamWriteDelegate(FgRPC_Delegate::CreateUObject(this, &ThisClass::OnStreamWrite))
//	, pointer(0)
//{
//}
//
//UConvaiSayHelloStreamProxy* UConvaiSayHelloStreamProxy::CreateConvaiSayHelloStreamQueryProxy(UObject* WorldContextObject, TArray<FString> Names)
//{
//	UConvaiSayHelloStreamProxy* Proxy = NewObject<UConvaiSayHelloStreamProxy>();
//	Proxy->WorldPtr = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
//	Proxy->Names = Names;
//	return Proxy;
//}
//
//void UConvaiSayHelloStreamProxy::Activate()
//{
//	if (Names.Num() == 0)
//	{
//		Failed();
//		UE_LOG(ConvaiGRPCTestLog, Warning, TEXT("GRPC Hello stream no data to send"));
//		return;
//	}
//
//	if (UConvaiSubsystem* ConvaiSubsystem = GRPCTest::GetConvaiSubsystem(WorldPtr))
//	{
//		ConvaiSubsystem->gRPC_Runnable->HelloStream(&OnInitStreamDelegate, stream_handler, &context, &status, nullptr, nullptr);
//	}
//}
//
//void UConvaiSayHelloStreamProxy::OnStreamInit(bool ok)
//{
//	//TODO (Mohamed) handle status variable
//
//	if (!ok)
//		return;
//
//	UE_LOG(ConvaiGRPCTestLog, Log, TEXT("GRPC Hello stream initialized"));
//	request.set_name(std::string(TCHAR_TO_UTF8(*Names[pointer++])));
//	if (UConvaiSubsystem* ConvaiSubsystem = GRPCTest::GetConvaiSubsystem(WorldPtr))
//	{
//		// Do a write task
//		ConvaiSubsystem->gRPC_Runnable->HelloStream(&OnStreamWriteDelegate, stream_handler, &context, &status, &request, &reply);
//
//		// Do a read task (by putting "request" as nullptr we are sending a read request)
//		ConvaiSubsystem->gRPC_Runnable->HelloStream(&OnStreamReadDelegate, stream_handler, &context, &status, nullptr, &reply);
//	}
//}
//
//void UConvaiSayHelloStreamProxy::OnStreamRead(bool ok)
//{
//	//TODO (Mohamed) handle status variable
//	if (!ok)
//		return;
//
//	Success();
//
//	if (UConvaiSubsystem* ConvaiSubsystem = GRPCTest::GetConvaiSubsystem(WorldPtr))
//	{
//		ConvaiSubsystem->gRPC_Runnable->HelloStream(&OnStreamReadDelegate, stream_handler, &context, &status, nullptr, &reply);
//	}
//}
//
//void UConvaiSayHelloStreamProxy::OnStreamWrite(bool ok)
//{
//	//TODO (Mohamed) handle status variable
//	if (!ok)
//		return;
//
//	UE_LOG(ConvaiGRPCTestLog, Log, TEXT("GRPC Wrote one request"));
//
//	if (pointer >= Names.Num())
//	{
//		UE_LOG(ConvaiGRPCTestLog, Log, TEXT("GRPC Done writing to stream"));
//		return;
//	}
//
//	bool FinalWrite = false;
//
//	if (pointer == Names.Num() - 1)
//		FinalWrite = true;
//
//	request.set_name(std::string(TCHAR_TO_UTF8(*Names[pointer++])));
//	if (UConvaiSubsystem* ConvaiSubsystem = GRPCTest::GetConvaiSubsystem(WorldPtr))
//	{
//		ConvaiSubsystem->gRPC_Runnable->HelloStream(&OnStreamWriteDelegate, stream_handler, &context, &status, &request, &reply, FinalWrite);
//	}
//}
//
//void UConvaiSayHelloStreamProxy::Success()
//{
//	FString Message = FString(UTF8_TO_TCHAR(reply.message().c_str()));
//	OnNewData.Broadcast(Message);
//	Finish();
//}
//
//void UConvaiSayHelloStreamProxy::Failed()
//{
//	OnFailure.Broadcast(FString("Failure"));
//	Finish();
//}
//
//void UConvaiSayHelloStreamProxy::Finish()
//{
//}
