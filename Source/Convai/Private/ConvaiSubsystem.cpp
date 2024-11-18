// Copyright 2022 Convai Inc. All Rights Reserved.

#include "ConvaiSubsystem.h"
#include "ConvaiAndroid.h"
#include "Engine/Engine.h"
#include "Async/Async.h"
#include "../Convai.h"
#include "HAL/PlatformProcess.h"

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

	SslCredentialsOptions getSslOptionsystem()
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

	SslCredentialsOptions getSslOptions() {
		SslCredentialsOptions result;

		std::string pem_root_certs = R"(-----BEGIN CERTIFICATE-----
MIIFjDCCA3SgAwIBAgINAgCOsgIzNmWLZM3bmzANBgkqhkiG9w0BAQsFADBHMQsw
CQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExMQzEU
MBIGA1UEAxMLR1RTIFJvb3QgUjEwHhcNMjAwODEzMDAwMDQyWhcNMjcwOTMwMDAw
MDQyWjBGMQswCQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZp
Y2VzIExMQzETMBEGA1UEAxMKR1RTIENBIDFENDCCASIwDQYJKoZIhvcNAQEBBQAD
ggEPADCCAQoCggEBAKvAqqPCE27l0w9zC8dTPIE89bA+xTmDaG7y7VfQ4c+mOWhl
UebUQpK0yv2r678RJExK0HWDjeq+nLIHN1Em5j6rARZixmyRSjhIR0KOQPGBMUld
saztIIJ7O0g/82qj/vGDl//3t4tTqxiRhLQnTLXJdeB+2DhkdU6IIgx6wN7E5NcU
H3Rcsejcqj8p5Sj19vBm6i1FhqLGymhMFroWVUGO3xtIH91dsgy4eFKcfKVLWK3o
2190Q0Lm/SiKmLbRJ5Au4y1euFJm2JM9eB84Fkqa3ivrXWUeVtye0CQdKvsY2Fka
zvxtxvusLJzLWYHk55zcRAacDA2SeEtBbQfD1qsCAwEAAaOCAXYwggFyMA4GA1Ud
DwEB/wQEAwIBhjAdBgNVHSUEFjAUBggrBgEFBQcDAQYIKwYBBQUHAwIwEgYDVR0T
AQH/BAgwBgEB/wIBADAdBgNVHQ4EFgQUJeIYDrJXkZQq5dRdhpCD3lOzuJIwHwYD
VR0jBBgwFoAU5K8rJnEaK0gnhS9SZizv8IkTcT4waAYIKwYBBQUHAQEEXDBaMCYG
CCsGAQUFBzABhhpodHRwOi8vb2NzcC5wa2kuZ29vZy9ndHNyMTAwBggrBgEFBQcw
AoYkaHR0cDovL3BraS5nb29nL3JlcG8vY2VydHMvZ3RzcjEuZGVyMDQGA1UdHwQt
MCswKaAnoCWGI2h0dHA6Ly9jcmwucGtpLmdvb2cvZ3RzcjEvZ3RzcjEuY3JsME0G
A1UdIARGMEQwCAYGZ4EMAQIBMDgGCisGAQQB1nkCBQMwKjAoBggrBgEFBQcCARYc
aHR0cHM6Ly9wa2kuZ29vZy9yZXBvc2l0b3J5LzANBgkqhkiG9w0BAQsFAAOCAgEA
IVToy24jwXUr0rAPc924vuSVbKQuYw3nLflLfLh5AYWEeVl/Du18QAWUMdcJ6o/q
FZbhXkBH0PNcw97thaf2BeoDYY9Ck/b+UGluhx06zd4EBf7H9P84nnrwpR+4GBDZ
K+Xh3I0tqJy2rgOqNDflr5IMQ8ZTWA3yltakzSBKZ6XpF0PpqyCRvp/NCGv2KX2T
uPCJvscp1/m2pVTtyBjYPRQ+QuCQGAJKjtN7R5DFrfTqMWvYgVlpCJBkwlu7+7KY
3cTIfzE7cmALskMKNLuDz+RzCcsYTsVaU7Vp3xL60OYhqFkuAOOxDZ6pHOj9+OJm
YgPmOT4X3+7L51fXJyRH9KfLRP6nT31D5nmsGAOgZ26/8T9hsBW1uo9ju5fZLZXV
VS5H0HyIBMEKyGMIPhFWrlt/hFS28N1zaKI0ZBGD3gYgDLbiDT9fGXstpk+Fmc4o
lVlWPzXe81vdoEnFbr5M272HdgJWo+WhT9BYM0Ji+wdVmnRffXgloEoluTNcWzc4
1dFpgJu8fF3LG0gl2ibSYiCi9a6hvU0TppjJyIWXhkJTcMJlPrWx1VytEUGrX2l0
JDwRjW/656r0KVB02xHRKvm2ZKI03TglLIpmVCK3kBKkKNpBNkFt8rhafcCKOb9J
x/9tpNFlQTl7B39rJlJWkR17QnZqVptFePFORoZmFzM=
-----END CERTIFICATE-----

-----BEGIN CERTIFICATE-----
MIIFVzCCAz+gAwIBAgINAgPlk28xsBNJiGuiFzANBgkqhkiG9w0BAQwFADBHMQsw
CQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExMQzEU
MBIGA1UEAxMLR1RTIFJvb3QgUjEwHhcNMTYwNjIyMDAwMDAwWhcNMzYwNjIyMDAw
MDAwWjBHMQswCQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZp
Y2VzIExMQzEUMBIGA1UEAxMLR1RTIFJvb3QgUjEwggIiMA0GCSqGSIb3DQEBAQUA
A4ICDwAwggIKAoICAQC2EQKLHuOhd5s73L+UPreVp0A8of2C+X0yBoJx9vaMf/vo
27xqLpeXo4xL+Sv2sfnOhB2x+cWX3u+58qPpvBKJXqeqUqv4IyfLpLGcY9vXmX7w
Cl7raKb0xlpHDU0QM+NOsROjyBhsS+z8CZDfnWQpJSMHobTSPS5g4M/SCYe7zUjw
TcLCeoiKu7rPWRnWr4+wB7CeMfGCwcDfLqZtbBkOtdh+JhpFAz2weaSUKK0Pfybl
qAj+lug8aJRT7oM6iCsVlgmy4HqMLnXWnOunVmSPlk9orj2XwoSPwLxAwAtcvfaH
szVsrBhQf4TgTM2S0yDpM7xSma8ytSmzJSq0SPly4cpk9+aCEI3oncKKiPo4Zor8
Y/kB+Xj9e1x3+naH+uzfsQ55lVe0vSbv1gHR6xYKu44LtcXFilWr06zqkUspzBmk
MiVOKvFlRNACzqrOSbTqn3yDsEB750Orp2yjj32JgfpMpf/VjsPOS+C12LOORc92
wO1AK/1TD7Cn1TsNsYqiA94xrcx36m97PtbfkSIS5r762DL8EGMUUXLeXdYWk70p
aDPvOmbsB4om3xPXV2V4J95eSRQAogB/mqghtqmxlbCluQ0WEdrHbEg8QOB+DVrN
VjzRlwW5y0vtOUucxD/SVRNuJLDWcfr0wbrM7Rv1/oFB2ACYPTrIrnqYNxgFlQID
AQABo0IwQDAOBgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4E
FgQU5K8rJnEaK0gnhS9SZizv8IkTcT4wDQYJKoZIhvcNAQEMBQADggIBAJ+qQibb
C5u+/x6Wki4+omVKapi6Ist9wTrYggoGxval3sBOh2Z5ofmmWJyq+bXmYOfg6LEe
QkEzCzc9zolwFcq1JKjPa7XSQCGYzyI0zzvFIoTgxQ6KfF2I5DUkzps+GlQebtuy
h6f88/qBVRRiClmpIgUxPoLW7ttXNLwzldMXG+gnoot7TiYaelpkttGsN/H9oPM4
7HLwEXWdyzRSjeZ2axfG34arJ45JK3VmgRAhpuo+9K4l/3wV3s6MJT/KYnAK9y8J
ZgfIPxz88NtFMN9iiMG1D53Dn0reWVlHxYciNuaCp+0KueIHoI17eko8cdLiA6Ef
MgfdG+RCzgwARWGAtQsgWSl4vflVy2PFPEz0tv/bal8xa5meLMFrUKTX5hgUvYU/
Z6tGn6D/Qqc6f1zLXbBwHSs09dR2CQzreExZBfMzQsNhFRAbd03OIozUhfJFfbdT
6u9AWpQKXCBfTkBdYiJ23//OYb2MI3jSNwLgjt7RETeJ9r/tSQdirpLsQBqvFAnZ
0E6yove+7u7Y/9waLd64NnHi/Hm3lCXRSHNboTXns5lndcEZOitHTtNCjv0xyBZm
2tIMPNuzjsmhDYAPexZ3FL//2wmUspO8IFgV6dtxQ/PeEMMA3KgqlbbC1j+Qa3bb
bP6MvPJwNQzcmRk13NfIRmPVNnGuV/u3gm3c
-----END CERTIFICATE-----

-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)";

		result.pem_root_certs = pem_root_certs;
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
		FPlatformProcess::Sleep(0.0001f);
		if (got_tag)
		{
			FgRPC_Delegate* gRPC_Delegate = static_cast<FgRPC_Delegate*>(got_tag);

			if (bIsRunning)
			{
				gRPC_Delegate->ExecuteIfBound(ok);
			}
			else
			{
				UE_LOG(ConvaiSubsystemLog, Log, TEXT("Could not run gRPC delegate due to thread closing down"));
			}
		}
		else
		{
			UE_LOG(ConvaiSubsystemLog, Log, TEXT("Bad got_tag"));
		}

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
	if (!bIsRunning)
	{
		return;
	}

	bIsRunning = false;
	{
		FScopeLock Lock(&CriticalSection);
		cq_.Shutdown();
	}
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
	FScopeLock Lock(&CriticalSection);

	// # TODO (Mohamed): Handling Mic permissions requires refactoring and a unified pipeline for all platforms
	// Delaying asking for permission for MacOS due to a crash
	#ifdef __APPLE__
    GetAppleMicPermission();
	#endif

	grpc_connectivity_state state = Channel->GetState(false);

	if (state != grpc_connectivity_state::GRPC_CHANNEL_READY)
	{
		UE_LOG(ConvaiSubsystemLog, Warning, TEXT("gRPC channel not ready yet.. Current State: %s"), *FString(grpc_connectivity_state_str[state]));
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

	//AsyncTask(ENamedThreads::GameThread, [WeakThis = MakeWeakObjectPtr(this)]
	//	{
			bool AllowInsecureConnection = Convai::Get().GetConvaiSettings()->AllowInsecureConnection;

			std::shared_ptr<grpc::ChannelCredentials> channel_creds;
#if PLATFORM_WINDOWS
			if (AllowInsecureConnection)
				channel_creds = grpc::InsecureChannelCredentials();
			else
				channel_creds = grpc::SslCredentials(getSslOptions());
#else
			if (AllowInsecureConnection)
				channel_creds = grpc::InsecureChannelCredentials();
			else
				channel_creds = grpc::SslCredentials(grpc::SslCredentialsOptions());
#endif


			FString URL = Convai::Get().GetConvaiSettings()->CustomURL;
			URL.TrimEndInline();
			URL.TrimStartInline();

			if (URL.IsEmpty())
				URL = "stream.convai.com";

			gRPC_Runnable = MakeShareable(new FgRPCClient(TCHAR_TO_UTF8(*URL), channel_creds));

			//gRPC_Runnable = MakeShareable(new FgRPCClient(std::string("0.tcp.us-cal-1.ngrok.io:13976"), channel_creds));

			gRPC_Runnable->StartStub();
			UE_LOG(ConvaiSubsystemLog, Log, TEXT("UConvaiSubsystem Started"));

#if PLATFORM_ANDROID
			GetAndroidMicPermission();
#endif
		//});
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
