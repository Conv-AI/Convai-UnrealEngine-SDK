// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once

#include "Subsystems/SubsystemCollection.h"
#include "Net/OnlineBlueprintCallProxyBase.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HAL/Runnable.h"
#include "HAL/ThreadSafeBool.h"

THIRD_PARTY_INCLUDES_START
#include "Proto/service.grpc.pb.h"
#undef min
#undef max
#undef UpdateResource
#undef GetObject
#undef GetUObject
#undef GetClassName
#undef InterlockedExchange
#undef InterlockedAnd
#undef InterlockedIncrement
#undef InterlockedDecrement
#undef InterlockedCompareExchange
#undef FastFence
#undef MemoryBarrier
#undef Yield
#undef CaptureStackBackTrace
#undef GetCurrentTime
THIRD_PARTY_INCLUDES_END

#include "ConvaiSubsystem.generated.h"

#ifdef __APPLE__
extern bool GetAppleMicPermission();
#endif

namespace grpc
{
	class Channel;

	template <class R>
	class ClientAsyncResponseReader;

	class ClientContext;
	class CompletionQueue;
	class Status;

	template <class W, class R>
	class ClientAsyncReaderWriter;
	class ChannelCredentials;
}


//namespace service
//{
//	//class ConvaiService;
//	
//	class ConvaiService::Stub;
//	class GetResponseResponse;
//	class GetResponseRequest;
//};

DECLARE_LOG_CATEGORY_EXTERN(ConvaiSubsystemLog, Log, All);

DECLARE_DELEGATE_OneParam(FgRPC_Delegate, bool);

class FgRPCClient : public FRunnable {
public:
	FgRPCClient(std::string target, const std::shared_ptr<grpc::ChannelCredentials>& creds);

    std::unique_ptr<service::ConvaiService::Stub> GetNewStub();

	grpc::CompletionQueue* GetCompletionQueue();

public:
    /**
     * FRunnable Interface
     * Loop while listening for completed responses.
     */
    virtual uint32 Run() override;

    void StartStub();

	void CreateChannel();

	void OnStateChange(bool ok);

    virtual void Exit() override;

private: 
    mutable FCriticalSection CriticalSection;
    FThreadSafeBool bIsRunning;

    TUniquePtr<FRunnableThread> Thread;


private:

    // Out of the passed in Channel comes the stub, stored here, our view of the
    // server's exposed services.
    std::unique_ptr<service::ConvaiService::Stub> stub_;

    std::shared_ptr<grpc::Channel> Channel;
	std::shared_ptr<grpc::ChannelCredentials> Creds;
	std::string Target;

	FgRPC_Delegate OnStateChangeDelegate;

    // The producer-consumer queue we use to communicate asynchronously with the
    // gRPC runtime.
	grpc::CompletionQueue cq_;
};


UCLASS(meta = (DisplayName = "Convai Subsystem"))
class UConvaiSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()


public:
	UConvaiSubsystem();

	// Begin USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	// End USubsystem

	void GetAndroidMicPermission();

public:
    TSharedPtr<FgRPCClient> gRPC_Runnable;
};
