// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once

#include "Subsystems/SubsystemCollection.h"
#include "Net/OnlineBlueprintCallProxyBase.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HAL/Runnable.h"

THIRD_PARTY_INCLUDES_START
#include "gRPC/service.grpc.pb.h"
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
#undef FastFence
#undef MemoryBarrier
#undef Yield
THIRD_PARTY_INCLUDES_END

#include "ConvaiSubsystem.generated.h"

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
    FgRPCClient(std::shared_ptr<grpc::Channel> channel)
        //: stub_(ConvaiService::NewStub(channel))
        : channel(channel)
        , bIsRunning(false) 
    {}

    std::unique_ptr<service::ConvaiService::Stub> GetNewStub();

	grpc::CompletionQueue* GetCompletionQueue();

public:
    /**
     * FRunnable Interface
     * Loop while listening for completed responses.
     */
    virtual uint32 Run() override;

    void StartStub();

    virtual void Exit() override;

private: 
    mutable FCriticalSection CriticalSection;
    FThreadSafeBool bIsRunning;

    TUniquePtr<FRunnableThread> Thread;


private:

    // Out of the passed in Channel comes the stub, stored here, our view of the
    // server's exposed services.
    std::unique_ptr<service::ConvaiService::Stub> stub_;

    std::shared_ptr<grpc::Channel> channel;

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

public:
    TSharedPtr<FgRPCClient> gRPC_Runnable;
};
