// Copyright 2022 Convai Inc. All Rights Reserved.


#include "ConvaiGRPC.h"
#include "../Convai.h"
#include "ConviDefinitions.h"
#include "ConvaiActionUtils.h"
#include "ConvaiUtils.h"
#include "JsonObjectConverter.h"
#include "Kismet/GameplayStatics.h"
// #include <chrono>   
#include <string>

THIRD_PARTY_INCLUDES_START
#include <grpc++/grpc++.h>
THIRD_PARTY_INCLUDES_END

// Needed for CURL to work
#pragma comment (lib, "crypt32.lib")

DEFINE_LOG_CATEGORY(ConvaiGRPCLog);

using ::service::GetResponseRequest_GetResponseConfig;
using ::service::ActionConfig;
using ::service::AudioConfig;
using ::service::ActionConfig_Object;
using ::service::ActionConfig_Character;
using ::service::GetResponseRequest_GetResponseData;

using grpc::Channel;
using grpc::ClientAsyncResponseReader;
using grpc::ClientContext;
using grpc::CompletionQueue;
using grpc::Status;
using service::ConvaiService;


namespace {
	UConvaiSubsystem* GetConvaiSubsystem(const UObject* WorldContextObject)
	{
		//UWorld* World = WorldPtr.Get();

		if (!WorldContextObject)
		{
			UE_LOG(ConvaiGRPCLog, Warning, TEXT("WorldContextObject ptr is invalid!"));
			return nullptr;
		}

		UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(WorldContextObject);
		if (!GameInstance)
		{
			UE_LOG(ConvaiSubsystemLog, Warning, TEXT("Could not get pointer to a GameInstance"));
			return nullptr;
		}


		if (UConvaiSubsystem* ConvaiSubsystem = GameInstance->GetSubsystem<UConvaiSubsystem>())
		{
			return ConvaiSubsystem;
		}
		else
		{
			UE_LOG(ConvaiSubsystemLog, Warning, TEXT("Could not get pointer to Convai Subsystem"));
			return nullptr;
		}

	}



	char* StatusCodeStr[] =
	{
	"OK",
	"CANCELLED",
	"UNKNOWN",
	"INVALID_ARGUMENT",
	"DEADLINE_EXCEEDED",
	"NOT_FOUND",
	"ALREADY_EXISTS",
	"PERMISSION_DENIED",
	"UNAUTHENTICATED",
	"RESOURCE_EXHAUSTED",
	"FAILED_PRECONDITION",
	"ABORTED",
	"OUT_OF_RANGE",
	"UNIMPLEMENTED",
	"INTERNAL",
	"UNAVAILABLE",
	"DATA_LOSS",
	"DO_NOT_USE" };

}


UConvaiGRPCGetResponseProxy* UConvaiGRPCGetResponseProxy::CreateConvaiGRPCGetResponseProxy(UObject* WorldContextObject, FString UserQuery, FString CharID, bool VoiceResponse, FString SessionID, UConvaiEnvironment* Environment, bool GenerateActions)
{
	UConvaiGRPCGetResponseProxy* Proxy = NewObject<UConvaiGRPCGetResponseProxy>();
	Proxy->WorldPtr = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	Proxy->UserQuery = UserQuery;
	Proxy->CharID = CharID;
	Proxy->SessionID = SessionID;
	Proxy->VoiceResponse = VoiceResponse;
	Proxy->Environment = Environment;
	Proxy->GenerateActions = GenerateActions;
	Proxy->API_key = Convai::Get().GetConvaiSettings()->API_Key;

	return Proxy;
}

void UConvaiGRPCGetResponseProxy::Activate()
{
	OnInitStreamDelegate = FgRPC_Delegate::CreateUObject(this, &ThisClass::OnStreamInit);
	OnStreamReadDelegate = FgRPC_Delegate::CreateUObject(this, &ThisClass::OnStreamRead);
	OnStreamWriteDelegate = FgRPC_Delegate::CreateUObject(this, &ThisClass::OnStreamWrite);
	OnStreamWriteDoneDelegate = FgRPC_Delegate::CreateUObject(this, &ThisClass::OnStreamWriteDone);
	OnStreamFinishDelegate = FgRPC_Delegate::CreateUObject(this, &ThisClass::OnStreamFinish);
	
	reply = std::unique_ptr<service::GetResponseResponse>(new service::GetResponseResponse());

	// Form Validation
	if (!UConvaiFormValidation::ValidateAPIKey(API_key) || !(UConvaiFormValidation::ValidateCharacterID(CharID)) || !(UConvaiFormValidation::ValidateSessionID(SessionID)))
	{
		OnFailure.ExecuteIfBound();
		return;
	}

	if (!WorldPtr.IsValid())
	{
		UE_LOG(ConvaiGRPCLog, Warning, TEXT("WorldPtr not valid"));
		OnFailure.ExecuteIfBound();
		return;
	}

	UConvaiSubsystem* ConvaiSubsystem = GetConvaiSubsystem(WorldPtr.Get());
	if (!ConvaiSubsystem)
	{
		UE_LOG(ConvaiGRPCLog, Warning, TEXT("Convai Subsystem is not valid"));
		OnFailure.ExecuteIfBound();
		return;
	}

	// Create a new stub instance
	stub_ = ConvaiSubsystem->gRPC_Runnable->GetNewStub();
	if (!stub_)
	{
		UE_LOG(ConvaiGRPCLog, Warning, TEXT("Could not aquire a new stub instance"));
		OnFailure.ExecuteIfBound();
		return;
	}

	// Aquire the completion queue instance
	cq_ = ConvaiSubsystem->gRPC_Runnable->GetCompletionQueue();
	if (!cq_)
	{
		UE_LOG(ConvaiGRPCLog, Warning, TEXT("Got an invalid completion queue instance"));
		OnFailure.ExecuteIfBound();
		return;
	}

	//ClientContext context;
	//std::chrono::system_clock::time_point deadline =
	//	std::chrono::system_clock::now() + std::chrono::milliseconds(3000);
	//client_context.set_deadline(deadline);

	// Initialize the stream
	stream_handler = stub_->AsyncGetResponse(&client_context, cq_, (void*)&OnInitStreamDelegate);
}

void UConvaiGRPCGetResponseProxy::WriteAudioDataToSend(uint8* Buffer, uint32 Length, bool LastWrite)
{
	LastWriteReceived = LastWrite;

	m_mutex.Lock();

	AudioBuffer.Append(Buffer, Length);

	m_mutex.Unlock();


	if (InformOnDataReceived)
	{
		// Inform of new data to send
		OnStreamWrite(true);

		// Reset
		InformOnDataReceived = false;
	}
}

void UConvaiGRPCGetResponseProxy::FinishWriting()
{
	LastWriteReceived = true;

	if (InformOnDataReceived)
	{
		// Inform of new data to send
		OnStreamWrite(true);

		// Reset
		InformOnDataReceived = false;
	}
}

void UConvaiGRPCGetResponseProxy::BeginDestroy()
{
	client_context.TryCancel();
	stub_.reset();
	Super::BeginDestroy();
	//UE_LOG(ConvaiGRPCLog, Log, TEXT("Destroying..."));
}

TArray<uint8> UConvaiGRPCGetResponseProxy::ConsumeFromAudioBuffer(bool& IsThisTheFinalWrite)
{
	// TODO: (Mohamed) optimize this to reduce number of copying operations to not spend much time in CS, also reserve memory for "output" array before entering

	TArray<uint8> output;

	m_mutex.Lock();
	uint32 Length = FMath::Min(AudioBuffer.Num(), (int32)ConvaiConstants::VoiceStreamMaxChunk);
	if (Length)
	{
		output.Append(AudioBuffer.GetData(), Length);

		if (Length == AudioBuffer.Num())
			AudioBuffer.SetNumUnsafeInternal(0);
		else
			AudioBuffer.RemoveAt(0, Length);
	}
	m_mutex.Unlock();


	if (LastWriteReceived && AudioBuffer.Num() == 0)
		IsThisTheFinalWrite = true;
	else
		IsThisTheFinalWrite = false;

	return output;
}

void UConvaiGRPCGetResponseProxy::LogAndEcecuteFailure(FString FuncName)
{
	UE_LOG(ConvaiGRPCLog, Warning,
	TEXT("%s: Status:%s | Debug Log:%s | Error message:%s | Error Details:%s | Error Code:%i"),
	*FString(FuncName), 
	*FString(status.ok()? "Ok" : "Not Ok"),
	*FString(reply->DebugString().c_str()), 
	*FString(status.error_message().c_str()), 
	*FString(status.error_details().c_str()), 
	status.error_code());

	if (!FailAlreadyExecuted)
	{
		OnFailure.ExecuteIfBound();
		FailAlreadyExecuted = true;
	}
}

void UConvaiGRPCGetResponseProxy::ExtendDeadline()
{
	// Extend the deadline
	std::chrono::system_clock::time_point deadline =
		std::chrono::system_clock::now() + std::chrono::milliseconds(3000);
	//client_context.set_deadline(deadline);
}

void UConvaiGRPCGetResponseProxy::OnStreamInit(bool ok)
{
	//TODO (Mohamed) handle status variable

	if (!IsValid(this))
	{
		UE_LOG(ConvaiGRPCLog, Warning, TEXT("OnStreamInit Could not initialize due to pending kill!"));
		LogAndEcecuteFailure("OnStreamInit");
		return;
	}

	if (!ok)
	{
		LogAndEcecuteFailure("OnStreamInit");
		//UE_LOG(ConvaiGRPCLog, Warning, TEXT("OnStreamInit Could not initialize!"));
		//stream_handler->Finish(&status, (void*)&OnStreamFinishDelegate);
		return;
	}

	UE_LOG(ConvaiGRPCLog, Log, TEXT("GRPC GetResponse stream initialized"));



	// Create Action Configuration
	ActionConfig* action_config = new ActionConfig();
	FString MainCharacter;
	if (GenerateActions && IsValid(Environment))
	{
		action_config->set_classification("multistep");
		for (FString action : Environment->Actions) // Add Actions
		{
			action_config->add_actions(TCHAR_TO_ANSI(*action));
		}

		for (FConvaiObjectEntry object : Environment->Objects) // Add Objects
		{
			ActionConfig_Object* action_config_object = action_config->add_objects();
			action_config_object->set_name(TCHAR_TO_ANSI(*object.Name));
			action_config_object->set_description(TCHAR_TO_ANSI(*object.Description));
		}

		for (FConvaiObjectEntry character : Environment->Characters) // Add Characters
		{
			ActionConfig_Character* action_config_character = action_config->add_characters();
			action_config_character->set_name(TCHAR_TO_ANSI(*character.Name));
			action_config_character->set_bio(TCHAR_TO_ANSI(*character.Description));
		}

		// Get the speaker/main character name
		MainCharacter = Environment->MainCharacter.Name;
	}


	// Create Audio Configuration
	AudioConfig* audio_config = new AudioConfig();
	audio_config->set_sample_rate_hertz(ConvaiConstants::VoiceCaptureSampleRate);

	// Create the config object that holds Audio and Action configs
	GetResponseRequest_GetResponseConfig* getResponseConfig = new GetResponseRequest_GetResponseConfig();
	getResponseConfig->set_api_key(TCHAR_TO_ANSI(*API_key));
	getResponseConfig->set_session_id(TCHAR_TO_ANSI(*SessionID));
	getResponseConfig->set_character_id(TCHAR_TO_ANSI(*CharID));
	if (GenerateActions)
	{
		getResponseConfig->set_allocated_action_config(action_config);
		getResponseConfig->set_speaker(TCHAR_TO_ANSI(*MainCharacter));
	}
	getResponseConfig->set_allocated_audio_config(audio_config);

	// Set the config object in the request object to be passed to the API
	request.Clear();
	request.set_allocated_get_response_config(getResponseConfig);

#if ConvaiDebugMode
	FString DebugString(request.DebugString().c_str());
	UE_LOG(ConvaiGRPCLog, Log, TEXT("request: %s"), *DebugString);
#endif 

	// Do a write task
	stream_handler->Write(request, (void*)&OnStreamWriteDelegate);
	//UE_LOG(ConvaiGRPCLog, Log, TEXT("stream_handler->Write"));

	// Do a read task
	stream_handler->Read(reply.get(), (void*)&OnStreamReadDelegate);
	//UE_LOG(ConvaiGRPCLog, Log, TEXT("stream_handler->Read"));
}

void UConvaiGRPCGetResponseProxy::OnStreamWrite(bool ok)
{
	if (!IsValid(this))
	{
		UE_LOG(ConvaiGRPCLog, Warning, TEXT("OnStreamWrite failed due to pending kill!"));
		LogAndEcecuteFailure("OnStreamWrite");
		return;
	}

	if (!ok)
	{
		LogAndEcecuteFailure("OnStreamWrite");
		stream_handler->Finish(&status, (void*)&OnStreamFinishDelegate);
		return;
	}

	//UE_LOG(ConvaiGRPCLog, Log, TEXT("OnStreamWrite"));




	//UE_LOG(ConvaiGRPCLog, Log, TEXT("OnStreamWrite"));

	UConvaiSubsystem* ConvaiSubsystem = GetConvaiSubsystem(WorldPtr.Get());

	// Clear the request data to make it ready to hold the new data we are going to send
	request.Clear();
	GetResponseRequest_GetResponseData* get_response_data = new GetResponseRequest_GetResponseData();

	bool IsThisTheFinalWrite;

	// If there is text
	if (UserQuery.Len())
	{
		// Add in the text data
		get_response_data->set_text_data(TCHAR_TO_ANSI(*UserQuery));
		IsThisTheFinalWrite = true;
	}
	else
	{
		// Try to consume the next chunk of mic data
		uint8* Buffer = nullptr;
		TArray<uint8> Data = ConsumeFromAudioBuffer(IsThisTheFinalWrite);

		if (!Data.Num())
		{
			if (IsThisTheFinalWrite)
			{
				// Tell the server that we have finished writing
				UE_LOG(ConvaiGRPCLog, Log, TEXT("stream_handler->WritesDone"));
				stream_handler->WritesDone((void*)&OnStreamWriteDoneDelegate); UE_LOG(ConvaiGRPCLog, Log, TEXT("OnStreamWrite Done Writing"));
			}
			else
			{
				// Let us know when new data is available
				InformOnDataReceived = true; // UE_LOG(ConvaiGRPCLog, Log, TEXT("OnStreamWrite: Awaiting audio data"));
			}

			// Do not proceed
			return;
		}
		else
		{
			// Load the audio data to the request
			get_response_data->set_audio_data(Data.GetData(), Data.Num()); // UE_LOG(ConvaiGRPCLog, Log, TEXT("OnStreamWrite: Sending %d bytes"), DataLen);
		}
	}

	// Prepare the request
	request.set_allocated_get_response_data(get_response_data);


	// #if ConvaiDebugMode
	//     FString DebugString(request.DebugString().c_str());
	//     UE_LOG(ConvaiGRPCLog, Warning, TEXT("request: %s"), *DebugString);
	// #endif 

	if (IsThisTheFinalWrite)
	{
		// Send the data and tell the server that this is the last piece of data
		UE_LOG(ConvaiGRPCLog, Log, TEXT("stream_handler->WriteLast"));
		stream_handler->WriteLast(request, grpc::WriteOptions(), (void*)&OnStreamWriteDoneDelegate);
	}
	else
	{
		// Do a normal send of the data
		//UE_LOG(ConvaiGRPCLog, Log, TEXT("stream_handler->Write"));
		stream_handler->Write(request, (void*)&OnStreamWriteDelegate);
	}

}

void UConvaiGRPCGetResponseProxy::OnStreamWriteDone(bool ok)
{
	if (!IsValid(this))
	{
		UE_LOG(ConvaiGRPCLog, Warning, TEXT("OnStreamWriteDone failed due to pending kill!"));
		LogAndEcecuteFailure("OnStreamWriteDone");
		return;
	}

	if (!ok)
	{
		LogAndEcecuteFailure("OnStreamWriteDone");
		stream_handler->Finish(&status, (void*)&OnStreamFinishDelegate);
		return;
	}

	UE_LOG(ConvaiGRPCLog, Log, TEXT("OnStreamWriteDone"));



	// Tell the server early on that we are ready to finish the stream any time it wishes
	UE_LOG(ConvaiGRPCLog, Log, TEXT("stream_handler->Finish"));
	stream_handler->Finish(&status, (void*)&OnStreamFinishDelegate);
}

void UConvaiGRPCGetResponseProxy::OnStreamRead(bool ok)
{
	if (!IsValid(this))
	{
		UE_LOG(ConvaiGRPCLog, Warning, TEXT("OnStreamRead failed due to pending kill!"));
		LogAndEcecuteFailure("OnStreamRead");
		return;
	}


	if (!ok && !status.ok())
	{
		LogAndEcecuteFailure("OnStreamRead");
		return;
	}
	//UE_LOG(ConvaiGRPCLog, Log, TEXT("OnStreamRead"));



	UConvaiSubsystem* ConvaiSubsystem = GetConvaiSubsystem(WorldPtr.Get());

	// Grab the session ID
	std::string SessionID_std = reply->session_id();
	if (SessionID_std.size())
	{
		SessionID = FString(SessionID_std.c_str());
		// Broadcast the Session ID
		OnSessionIDReceived.ExecuteIfBound(SessionID);

#if ConvaiDebugMode
		//UE_LOG(ConvaiGRPCLog, Log, TEXT("SessionID: %s"), *SessionID);
#endif 
	}


	if (reply->has_user_query()) // Is there transcription ready
	{
		std::string UserQuery_std = reply->user_query().text_data();
		bool IsTranscriptionReady = reply->user_query().is_final();
		bool IsFinalTranscription = reply->user_query().end_of_response();
		OnTranscriptionReceived.ExecuteIfBound(FString(UserQuery_std.c_str()), IsTranscriptionReady, IsFinalTranscription);
		//UE_LOG(ConvaiGRPCLog, Log, TEXT("UserQuery: %s, Final: %d"), *FString(UserQuery_std.c_str()), IsFinalUserQuery);
	}
	else if (reply->has_audio_response()) // Is there an audio response
	{
		// Grab bot text
		FString text_string(reply->audio_response().text_data().c_str());

		// Grab bot audio
		::std::string audio_data = reply->audio_response().audio_data();
		TArray<uint8> VoiceData = TArray<uint8>(reinterpret_cast<const uint8*>(audio_data.data() + 44), audio_data.length() - 44);
		bool IsFinalResponse = reply->audio_response().end_of_response();

		// Broadcast the audio and text
		OnDataReceived.ExecuteIfBound(text_string, VoiceData, reply->audio_response().audio_config().sample_rate_hertz(), IsFinalResponse);
	}
	else if (reply->has_action_response()) // Is there an action response
	{
		// Convert Action string to FString
		FString SequenceString(reply->action_response().action().c_str());

#if ConvaiDebugMode
		UE_LOG(ConvaiGRPCLog, Log, TEXT("GetResponse SequenceString: %s"), *SequenceString);
#endif 

		// Parse the actions
		TArray<FConvaiResultAction> SequenceOfActions;
		TArray<FString> Sequence;
		SequenceString.ParseIntoArray(Sequence, TEXT(","), true);
		for (auto s : Sequence)
		{
			FConvaiResultAction ConvaiResultAction;
			if (UConvaiActions::ParseAction(Environment, s, ConvaiResultAction)) // if successful
			{
				SequenceOfActions.Add(ConvaiResultAction);
			}

			UE_LOG(ConvaiGRPCLog, Log, TEXT("Action: %s"), *ConvaiResultAction.Action);
		}
		// Broadcast the actions
		OnActionsReceived.ExecuteIfBound(SequenceOfActions);
	}
	else // This is a debug message response
	{
#if ConvaiDebugMode
		FString DebugString(reply->debug_log().c_str());
		//FString DebugString(reply->DebugString().c_str());
		UE_LOG(ConvaiGRPCLog, Log, TEXT("Debug log: %s"), *DebugString);
#endif 
		// This is usually the last read, do not ask for further reads
		return;
	}

	// Initiate another read task
	reply->Clear();
	//UE_LOG(ConvaiGRPCLog, Log, TEXT("stream_handler->Read"));
	stream_handler->Read(reply.get(), (void*)&OnStreamReadDelegate);
}

void UConvaiGRPCGetResponseProxy::OnStreamFinish(bool ok)
{
	//if (!IsValid(this))
	//{
	//	UE_LOG(ConvaiGRPCLog, Warning, TEXT("OnStreamFinish failed due to pending kill!"));
	//	LogAndEcecuteFailure("OnStreamFinish");
	//	return;
	//}

	if (!ok || !status.ok())
	{
		LogAndEcecuteFailure("OnStreamFinish");
		return;
	}

	//if ()
	//{
	//	UE_LOG(ConvaiGRPCLog, Log, TEXT("OnStreamFinish received non-ok status"));
	//}

#if ConvaiDebugMode
	UE_LOG(ConvaiGRPCLog, Log, TEXT("OnStreamFinish"));
#endif 


	OnFinish.ExecuteIfBound();
}