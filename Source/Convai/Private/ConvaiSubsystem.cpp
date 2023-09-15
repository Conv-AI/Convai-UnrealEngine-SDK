// Copyright 2022 Convai Inc. All Rights Reserved.

#include "ConvaiSubsystem.h"
#include "ConvaiAndroid.h"
#include "Engine/Engine.h"

THIRD_PARTY_INCLUDES_START
// grpc includes
#include <grpc++/grpc++.h>
#include <chrono>
THIRD_PARTY_INCLUDES_END


using grpc::Channel;
using grpc::ClientAsyncResponseReader;
using grpc::ClientContext;
using grpc::CompletionQueue;
using grpc::Status;
using service::ConvaiService;

DEFINE_LOG_CATEGORY(ConvaiSubsystemLog);

using grpc::SslCredentialsOptions;

#if PLATFORM_WINDOWS
#include <wincrypt.h>
namespace
{

	std::string utf8Encode(const std::wstring& wstr)
	{
		if (wstr.empty())
			return std::string();

		int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(),
			NULL, 0, NULL, NULL);
		std::string strTo(sizeNeeded, 0);
		WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(),
			&strTo[0], sizeNeeded, NULL, NULL);
		return strTo;
	}

	SslCredentialsOptions getSslOptions()
	{
		// Fetch root certificate as required on Windows (s. issue 25533).
		SslCredentialsOptions result;

		// Open root certificate store.
		HANDLE hRootCertStore = CertOpenSystemStoreW(NULL, L"ROOT");
		if (!hRootCertStore)
			return result;

		// Get all root certificates.
		PCCERT_CONTEXT pCert = NULL;
		while ((pCert = CertEnumCertificatesInStore(hRootCertStore, pCert)) != NULL)
		{
			// Append this certificate in PEM formatted data.
			DWORD size = 0;
			CryptBinaryToStringW(pCert->pbCertEncoded, pCert->cbCertEncoded,
				CRYPT_STRING_BASE64HEADER, NULL, &size);
			std::vector<WCHAR> pem(size);
			CryptBinaryToStringW(pCert->pbCertEncoded, pCert->cbCertEncoded,
				CRYPT_STRING_BASE64HEADER, pem.data(), &size);

			result.pem_root_certs += utf8Encode(pem.data());
		}

		CertCloseStore(hRootCertStore, 0);
		return result;
	}
};
#endif

namespace
{
	const char* grpc_connectivity_state_str[] =
	{
		"GRPC_CHANNEL_IDLE",
		"GRPC_CHANNEL_CONNECTING",
		"GRPC_CHANNEL_READY",
		"GRPC_CHANNEL_TRANSIENT_FAILURE",
		"GRPC_CHANNEL_SHUTDOWN"
	};

};



uint32 FgRPCClient::Run()
{
    void* got_tag;
    bool ok = false;
	UE_LOG(ConvaiSubsystemLog, Log, TEXT("Start Run"));

    // Block until the next result is available in the completion queue "cq
	while (bIsRunning && cq_.Next(&got_tag, &ok)) {
		if (got_tag)
		{
			FgRPC_Delegate* gRPC_Delegate = static_cast<FgRPC_Delegate*>(got_tag);
			AsyncTask(ENamedThreads::GameThread, [&gRPC_Delegate, &ok, this]
			{
				if (bIsRunning)
				{
					gRPC_Delegate->ExecuteIfBound(ok);
				}
				else
				{
					UE_LOG(ConvaiSubsystemLog, Log, TEXT("Could not run gRPC delegate due to thread closing down"));
				}
			});
		}
		else
		{
			UE_LOG(ConvaiSubsystemLog, Log, TEXT("Bad got_tag"));
		}
		//FPlatformProcess::Sleep(.5);
    }
	UE_LOG(ConvaiSubsystemLog, Log, TEXT("End Run"));

	return 0;
}

void FgRPCClient::StartStub()
{
	OnStateChangeDelegate = FgRPC_Delegate::CreateRaw(this, &FgRPCClient::OnStateChange);
	CreateChannel();

	bIsRunning = true;
	Thread.Reset(FRunnableThread::Create(this, TEXT("gRPC_Stub")));
}

void FgRPCClient::CreateChannel()
{
	UE_LOG(ConvaiSubsystemLog, Log, TEXT("gRPC Creating Channel..."));
	grpc::ChannelArguments args;
	args.SetMaxReceiveMessageSize(2147483647);

	Channel = grpc::CreateCustomChannel(Target, Creds, args);

	//Channel->NotifyOnStateChange(grpc_connectivity_state::GRPC_CHANNEL_CONNECTING, std::chrono::system_clock::time_point().max(), &cq_, (void*)&OnStateChangeDelegate);
}

void FgRPCClient::OnStateChange(bool ok)
{
	//if (!bIsRunning)
	//	return;

	grpc_connectivity_state state;
	
	if (Channel)
		state = Channel->GetState(true);
	else
		state = grpc_connectivity_state::GRPC_CHANNEL_SHUTDOWN;


	if (state == grpc_connectivity_state::GRPC_CHANNEL_SHUTDOWN)
	{
		if (!bIsRunning)
		{
			UE_LOG(ConvaiSubsystemLog, Warning, TEXT("gRPC channel state changed to %s... Attempting to reconnect"), *FString(grpc_connectivity_state_str[state]));
			CreateChannel();
		}
		else
		{
			UE_LOG(ConvaiSubsystemLog, Log, TEXT("gRPC channel state changed to %s... Closing"), *FString(grpc_connectivity_state_str[state]));
		}
		//Channel->NotifyOnStateChange(state, std::chrono::system_clock::time_point().max(), &cq_, (void*)&OnStateChangeDelegate);
	}
	else
	{
		UE_LOG(ConvaiSubsystemLog, Log, TEXT("gRPC channel state changed to %s"), *FString(grpc_connectivity_state_str[state]));
		Channel->NotifyOnStateChange(state, std::chrono::system_clock::time_point().max(), &cq_, (void*)&OnStateChangeDelegate);
	}
}

void FgRPCClient::Exit()
{
	//Channel.reset();
	//Channel = nullptr;

	if (!bIsRunning)
	{
		return;
	}

	bIsRunning = false;
	{
		FScopeLock Lock(&CriticalSection);
		cq_.Shutdown();
	}
	//Thread->WaitForCompletion();
	//Thread->Kill();
}

FgRPCClient::FgRPCClient(std::string InTarget,
	const std::shared_ptr<grpc::ChannelCredentials>& InCreds)
	: bIsRunning(false),
	Creds(InCreds),
	Target(InTarget)
{
}

std::unique_ptr<ConvaiService::Stub> FgRPCClient::GetNewStub()
{
	grpc_connectivity_state state = Channel->GetState(false);

	if (state != grpc_connectivity_state::GRPC_CHANNEL_READY)
	{
		UE_LOG(ConvaiSubsystemLog, Warning, TEXT("gRPC channel not ready yet.. Current State: %s"), *FString(grpc_connectivity_state_str[state]));
		//return nullptr;
	}
	return ConvaiService::NewStub(Channel);
}

CompletionQueue* FgRPCClient::GetCompletionQueue()
{
	return &cq_;
}

UConvaiSubsystem::UConvaiSubsystem()
	: UGameInstanceSubsystem()
{
}

void UConvaiSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

#if PLATFORM_WINDOWS
	auto channel_creds = grpc::SslCredentials(getSslOptions());
#else
	auto channel_creds = grpc::SslCredentials(grpc::SslCredentialsOptions());
#endif
	gRPC_Runnable = MakeShareable(new FgRPCClient(std::string("stream.convai.com"), channel_creds));

	gRPC_Runnable->StartStub();
	UE_LOG(ConvaiSubsystemLog, Log, TEXT("UConvaiSubsystem Started"));

	#if PLATFORM_ANDROID
	GetAndroidMicPermission();
	#endif

	#ifdef __APPLE__
    GetAppleMicPermission();
	#endif
}

void UConvaiSubsystem::Deinitialize()
{
	gRPC_Runnable->Exit();
	Super::Deinitialize();
	UE_LOG(ConvaiSubsystemLog, Log, TEXT("UConvaiSubsystem Stopped"));
}

void UConvaiSubsystem::GetAndroidMicPermission()
{
	if (!UConvaiAndroid::ConvaiAndroidHasMicrophonePermission())
		UConvaiAndroid::ConvaiAndroidAskMicrophonePermission();
}
