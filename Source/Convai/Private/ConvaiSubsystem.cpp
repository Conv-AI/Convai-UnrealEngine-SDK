// Copyright 2022 Convai Inc. All Rights Reserved.

#include "ConvaiSubsystem.h"

THIRD_PARTY_INCLUDES_START
#include <wincrypt.h>

// grpc includes
#include <grpc++/grpc++.h>
THIRD_PARTY_INCLUDES_END


using grpc::Channel;
using grpc::ClientAsyncResponseReader;
using grpc::ClientContext;
using grpc::CompletionQueue;
using grpc::Status;
using service::ConvaiService;

DEFINE_LOG_CATEGORY(ConvaiSubsystemLog);

using grpc::SslCredentialsOptions;

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
			AsyncTask(ENamedThreads::GameThread, [gRPC_Delegate, ok] {gRPC_Delegate->ExecuteIfBound(ok); });
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
	bIsRunning = true;
	Thread.Reset(FRunnableThread::Create(this, TEXT("gRPC_Stub")));
}

void FgRPCClient::Exit()
{
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

std::unique_ptr<ConvaiService::Stub> FgRPCClient::GetNewStub()
{
	return ConvaiService::NewStub(channel);
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

	auto channel_creds = grpc::SslCredentials(getSslOptions());
	//grpc::CreateChannel("stream.convai.com", channel_creds)
	gRPC_Runnable = MakeShareable(new FgRPCClient(
		grpc::CreateChannel("stream.convai.com", channel_creds)
	));

	gRPC_Runnable->StartStub();
	UE_LOG(ConvaiSubsystemLog, Log, TEXT("UConvaiSubsystem Started"));
}

void UConvaiSubsystem::Deinitialize()
{
	gRPC_Runnable->Exit();
	Super::Deinitialize();
	UE_LOG(ConvaiSubsystemLog, Log, TEXT("UConvaiSubsystem Stopped"));
}
