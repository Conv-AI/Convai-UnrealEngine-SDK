// Copyright 2022 Convai Inc. All Rights Reserved.


#include "ConvaiGRPC.h"
#include "../Convai.h"
#include "ConvaiDefinitions.h"
#include "ConvaiActionUtils.h"
#include "ConvaiUtils.h"
#include "JsonObjectConverter.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "HAL/PlatformProcess.h"
#include <string>
#include "Engine/EngineTypes.h"

THIRD_PARTY_INCLUDES_START
#include <grpc++/grpc++.h>
THIRD_PARTY_INCLUDES_END

// Needed for CURL to work
//#pragma comment (lib, "crypt32.lib")

DEFINE_LOG_CATEGORY(ConvaiGRPCLog);
DEFINE_LOG_CATEGORY(ConvaiGRPCFeedBackLog);

using ::service::GetResponseRequest_GetResponseConfig;
using ::service::TriggerConfig;
using ::service::ActionConfig;
using ::service::VisionInput;
using ::service::VisionInput_ImageData;
using ::service::AudioConfig;
using ::service::ActionConfig_Object;
using ::service::ActionConfig_Character;
using ::service::GetResponseRequest_GetResponseData;
using ::service::FaceModel;

using ::service::FeedbackRequest;
using ::service::FeedbackRequest_Feedback;

using grpc::Channel;
using grpc::ClientAsyncResponseReader;
using grpc::ClientContext;
using grpc::CompletionQueue;
using grpc::Status;
using service::ConvaiService;



namespace {
	const char* StatusCodeStr[] =
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

UConvaiGRPCGetResponseProxy* UConvaiGRPCGetResponseProxy::CreateConvaiGRPCGetResponseProxy(UObject* WorldContextObject, FConvaiGRPCGetResponseParams ConvaiGRPCGetResponseParams)
{
	UConvaiGRPCGetResponseProxy* Proxy = NewObject<UConvaiGRPCGetResponseProxy>();
	Proxy->WorldPtr = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	Proxy->ConvaiGRPCGetResponseParams = ConvaiGRPCGetResponseParams;
	Proxy->MaxRetries = 3;

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
	if (!UConvaiFormValidation::ValidateAuthKey(ConvaiGRPCGetResponseParams.AuthKey) || !(UConvaiFormValidation::ValidateCharacterID(ConvaiGRPCGetResponseParams.CharID)) || !(UConvaiFormValidation::ValidateSessionID(ConvaiGRPCGetResponseParams.SessionID)))
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

	UConvaiSubsystem* ConvaiSubsystem = UConvaiUtils::GetConvaiSubsystem(WorldPtr.Get());
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

	bool Found;
	FString VersionName;
	FString EngineVersion;
	FString PlatformName;
	FString PluginEngineVersion;
	FString FriendlyName;

	UConvaiUtils::GetPluginInfo(FString("Convai"), Found, VersionName, FriendlyName, PluginEngineVersion);
	UConvaiUtils::GetPlatformInfo(EngineVersion, PlatformName);

	// Add metadata
	client_context.AddMetadata("engine", "Unreal Engine");
	client_context.AddMetadata("engine_version", TCHAR_TO_UTF8(*EngineVersion));
	client_context.AddMetadata("platform_name", TCHAR_TO_UTF8(*PlatformName));

	if (Found)
	{
		client_context.AddMetadata("plugin_engine_version", TCHAR_TO_UTF8(*PluginEngineVersion));
		client_context.AddMetadata("plugin_version", TCHAR_TO_UTF8(*VersionName));
		client_context.AddMetadata("plugin_base_name", TCHAR_TO_UTF8(*FriendlyName));
	}
	else
	{
		client_context.AddMetadata("plugin_engine_version", "Unknown");
		client_context.AddMetadata("plugin_version", "Unknown");
		client_context.AddMetadata("plugin_base_name", "Unknown");
	}

	ReceivedFinish = false;

	// Set long timeout for request
	std::chrono::system_clock::time_point deadline = std::chrono::system_clock::now() + std::chrono::hours(1);
	client_context.set_deadline(deadline);

	// Initialize the stream
	FScopeLock Lock(&GPRCInitSection);
	stream_handler = stub_->AsyncGetResponse(&client_context, cq_, (void*)&OnInitStreamDelegate);
	UE_LOG(ConvaiGRPCLog, Log,
	TEXT("AsyncGetResponse started | Character ID : %s | Session ID : %s"),
	*ConvaiGRPCGetResponseParams.CharID,
	*ConvaiGRPCGetResponseParams.SessionID);
}

void UConvaiGRPCGetResponseProxy::WriteAudioDataToSend(uint8* Buffer, uint32 Length)
{
	m_mutex.Lock();

	AudioBuffer.Append(Buffer, Length);

	m_mutex.Unlock();

	// UE_LOG(ConvaiGRPCLog, Log, TEXT("WriteAudioDataToSend:: InformOnDataReceived = %s"), InformOnDataReceived ? *FString("True") : *FString("False"));
	if (InformOnDataReceived)
	{
		// Reset
		InformOnDataReceived = false;

		//UE_LOG(ConvaiGRPCLog, Log, TEXT("WriteAudioDataToSend:: Informing On Data Received"));

		// Inform of new data to send
		OnStreamWrite(true);
	}
}

void UConvaiGRPCGetResponseProxy::FinishWriting()
{
	LastWriteReceived = true;

	UE_LOG(ConvaiGRPCLog, Log, TEXT("Finish Writing to audio data buffer"));

	if (InformOnDataReceived)
	{
		UE_LOG(ConvaiGRPCLog, Log, TEXT("FinishWriting:: Informing On Data Received"));

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
	UE_LOG(ConvaiGRPCLog, Log,
		TEXT("Destroying UConvaiGRPCGetResponseProxy... | Character ID : %s | Session ID : %s"),
		*ConvaiGRPCGetResponseParams.CharID,
		*ConvaiGRPCGetResponseParams.SessionID);
	Super::BeginDestroy();
}

void UConvaiGRPCGetResponseProxy::CallFinish()
{
	if (CalledFinish || !stream_handler)
	{
		UE_LOG(ConvaiGRPCLog, Log, TEXT("stream_handler->Finish is already called | Character ID : %s | Session ID : %s"),
			*ConvaiGRPCGetResponseParams.CharID,
			*ConvaiGRPCGetResponseParams.SessionID);
		return;
	}

	CalledFinish = true;
	UE_LOG(ConvaiGRPCLog, Log,
		TEXT("Calling Stream Finish | Character ID : %s | Session ID : %s"),
		*ConvaiGRPCGetResponseParams.CharID,
		*ConvaiGRPCGetResponseParams.SessionID);
	stream_handler->Finish(&status, (void*)&OnStreamFinishDelegate);
}

TArray<uint8> UConvaiGRPCGetResponseProxy::ConsumeFromAudioBuffer(bool& IsThisTheFinalWrite)
{
	// TODO: (Mohamed) optimize and clean this to reduce number of copying operations to not spend much time in CS, also reserve memory for "output" array before entering

	TArray<uint8> output;

	m_mutex.Lock();
	//uint32 Length = FMath::Min(AudioBuffer.Num(), (int32)ConvaiConstants::VoiceStreamMaxChunk);
	uint32 Length = AudioBuffer.Num();
	if (Length)
	{
		output.Append(AudioBuffer.GetData(), Length);

		if (Length == AudioBuffer.Num())
			AudioBuffer.Empty(AudioBuffer.Max());
		else
			AudioBuffer.RemoveAt(0, Length);
	}


	if (LastWriteReceived)
		IsThisTheFinalWrite = true;
	else
		IsThisTheFinalWrite = false;
	m_mutex.Unlock();

	return output;
}

void UConvaiGRPCGetResponseProxy::LogAndEcecuteFailure(FString FuncName)
{
	UE_LOG(ConvaiGRPCLog, Warning,
		TEXT("%s: Status.ok():%s | Debug Log:%s | Error message:%s | Error Details:%s | Error Code:%i | Character ID:%s | Session ID:%s"),
		*FString(FuncName),
		*FString(status.ok() ? "Ok" : "Not Ok"),
		*FString(reply->DebugString().c_str()),
		*FString(status.error_message().c_str()),
		*FString(status.error_details().c_str()),
		status.error_code(),
		*ConvaiGRPCGetResponseParams.CharID,
		*ConvaiGRPCGetResponseParams.SessionID);

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

	
#if ENGINE_MAJOR_VERSION < 5
	if (!IsValid(this) || HasAnyFlags(RF_BeginDestroyed))
	{
		UE_LOG(ConvaiGRPCLog, Warning, TEXT("OnStreamInit Could not initialize due to pending kill! | Character ID : %s | Session ID : %s"),
			*ConvaiGRPCGetResponseParams.CharID,
			*ConvaiGRPCGetResponseParams.SessionID);
		LogAndEcecuteFailure("OnStreamInit");
		return;
}
#else
	if (!IsValid(this) || !IsValidChecked(this) || HasAnyFlags(RF_BeginDestroyed))
	{
		UE_LOG(ConvaiGRPCLog, Warning, TEXT("OnStreamInit Could not initialize due to pending kill! | Character ID : %s | Session ID : %s"),
			*ConvaiGRPCGetResponseParams.CharID,
			*ConvaiGRPCGetResponseParams.SessionID);
		LogAndEcecuteFailure("OnStreamInit");
		return;
	}
#endif

	if (!ok || !status.ok())
	{
		LogAndEcecuteFailure("OnStreamInit");
		return;
	}

	UE_LOG(ConvaiGRPCLog, Log, TEXT("GRPC GetResponse stream initialized | Character ID : %s | Session ID : %s"),
		*ConvaiGRPCGetResponseParams.CharID,
		*ConvaiGRPCGetResponseParams.SessionID);

	FScopeLock Lock(&GPRCInitSection);

	// Create Action Configuration
	ActionConfig* action_config = new ActionConfig();
	FString MainCharacter;
	if (ConvaiGRPCGetResponseParams.GenerateActions && IsValid(ConvaiGRPCGetResponseParams.Environment))
	{
		action_config->set_classification("multistep");
		for (FString action : ConvaiGRPCGetResponseParams.Environment->Actions) // Add Actions
		{
			action_config->add_actions(TCHAR_TO_UTF8(*action));
		}

		for (FConvaiObjectEntry object : ConvaiGRPCGetResponseParams.Environment->Objects) // Add Objects
		{
			ActionConfig_Object* action_config_object = action_config->add_objects();
			FString FinalName = object.Name;
			if (object.Description.Len())
			{
				FinalName = FinalName.Append(*FString(" <"));
				FinalName = FinalName.Append(*object.Description);
				FinalName = FinalName.Append(">");
			}
			action_config_object->set_name(TCHAR_TO_UTF8(*FinalName));
			action_config_object->set_description(TCHAR_TO_UTF8(*object.Description));
		}

		for (FConvaiObjectEntry character : ConvaiGRPCGetResponseParams.Environment->Characters) // Add Characters
		{
			ActionConfig_Character* action_config_character = action_config->add_characters();
			FString FinalName = character.Name;
			if (character.Description.Len())
			{
				FinalName = FinalName.Append(*FString(" <"));
				FinalName = FinalName.Append(*character.Description);
				FinalName = FinalName.Append(">");
			}

			action_config_character->set_name(TCHAR_TO_UTF8(*FinalName));
			action_config_character->set_bio(TCHAR_TO_UTF8(*character.Description));
		}

		// Check if we have an attention object set
		FConvaiObjectEntry AttentionObject = ConvaiGRPCGetResponseParams.Environment->AttentionObject;
		if (AttentionObject.Name.Len() != 0)
		{
			FString FinalName = AttentionObject.Name;
			if (AttentionObject.Description.Len())
			{
				FinalName = FinalName.Append(*FString(" <"));
				FinalName = FinalName.Append(*AttentionObject.Description);
				FinalName = FinalName.Append(">");
			}
			action_config->set_current_attention_object(TCHAR_TO_UTF8(*FinalName));
		}
	}

	if (IsValid(ConvaiGRPCGetResponseParams.Environment))
	{
		// Get the speaker/main character name
		MainCharacter = ConvaiGRPCGetResponseParams.Environment->MainCharacter.Name;
	}

	// Create Audio Configuration
	AudioConfig* audio_config = new AudioConfig();
	audio_config->set_sample_rate_hertz((int32)ConvaiConstants::VoiceCaptureSampleRate);
	audio_config->set_enable_facial_data(ConvaiGRPCGetResponseParams.RequireFaceData);
	audio_config->set_disable_audio(!ConvaiGRPCGetResponseParams.VoiceResponse);
	if (ConvaiGRPCGetResponseParams.RequireFaceData)
	{
		//FaceModel faceModel = ConvaiGRPCGetResponseParams.GeneratesVisemesAsBlendshapes ? FaceModel::FACE_MODEL_A_2X_MODEL_NAME : FaceModel::FACE_MODEL_OVR_MODEL_NAME;
		FaceModel faceModel = ConvaiGRPCGetResponseParams.GeneratesVisemesAsBlendshapes ? FaceModel::FACE_MODEL_PHONEMES_MODEL_NAME : FaceModel::FACE_MODEL_OVR_MODEL_NAME;
		audio_config->set_face_model(faceModel);
	}

	// Create the config object that holds Audio and Action configs
	GetResponseRequest_GetResponseConfig* getResponseConfig = new GetResponseRequest_GetResponseConfig();

	if (ConvaiGRPCGetResponseParams.AuthHeader == ConvaiConstants::Auth_Token_Header)
	{
		getResponseConfig->set_api_auth_token(TCHAR_TO_UTF8(*ConvaiGRPCGetResponseParams.AuthKey));
	}
	else if (ConvaiGRPCGetResponseParams.AuthHeader == ConvaiConstants::API_Key_Header)
	{
		getResponseConfig->set_api_key(TCHAR_TO_UTF8(*ConvaiGRPCGetResponseParams.AuthKey));
	}
	else
	{
		UE_LOG(ConvaiGRPCLog, Warning, TEXT("OnStreamInit Could not initialize due to invalid Authentication type! | Character ID : %s | Session ID : %s | Authentication type: %s | Authentication Key: %s"),
			*ConvaiGRPCGetResponseParams.CharID,
			*ConvaiGRPCGetResponseParams.SessionID,
			*ConvaiGRPCGetResponseParams.AuthHeader,
			*ConvaiGRPCGetResponseParams.AuthKey);
		LogAndEcecuteFailure("OnStreamInit");
		return;
	}


	getResponseConfig->set_session_id(TCHAR_TO_UTF8(*ConvaiGRPCGetResponseParams.SessionID));
	getResponseConfig->set_character_id(TCHAR_TO_UTF8(*ConvaiGRPCGetResponseParams.CharID));

	if (ConvaiGRPCGetResponseParams.GenerateActions)
	{
		getResponseConfig->set_allocated_action_config(action_config);
		getResponseConfig->set_speaker(TCHAR_TO_UTF8(*MainCharacter));
	}
	getResponseConfig->set_allocated_audio_config(audio_config);

	// Set narrative_template_keys 
	google::protobuf::Map<std::string, std::string>* narrative_template_keys_ProtoMap = getResponseConfig->mutable_narrative_template_keys();
	for (const TPair<FString, FString>& Elem : ConvaiGRPCGetResponseParams.Narrative_Template_Keys)
	{
		std::string Key = TCHAR_TO_UTF8(*Elem.Key);
		std::string Value = TCHAR_TO_UTF8(*Elem.Value);
		(*narrative_template_keys_ProtoMap)[Key] = Value;
	}

#if ConvaiDebugMode
	FString DebugString(getResponseConfig->DebugString().c_str());
	UE_LOG(ConvaiGRPCLog, Log, TEXT("request: %s | Character ID : %s | Session ID : %s"),
		*DebugString,
		*ConvaiGRPCGetResponseParams.CharID,
		*ConvaiGRPCGetResponseParams.SessionID);
#endif 


	// Create Vision Configuration
	if (ConvaiGRPCGetResponseParams.ConvaiGRPCVisionParams.data.Num() > 0)
	{
		VisionInput* vision_input = new VisionInput();;
		VisionInput_ImageData* vision_input_image_data = new VisionInput_ImageData();
		vision_input_image_data->set_width(ConvaiGRPCGetResponseParams.ConvaiGRPCVisionParams.width);
		vision_input_image_data->set_height(ConvaiGRPCGetResponseParams.ConvaiGRPCVisionParams.height);
		vision_input_image_data->set_data(ConvaiGRPCGetResponseParams.ConvaiGRPCVisionParams.data.GetData(), ConvaiGRPCGetResponseParams.ConvaiGRPCVisionParams.data.Num());
		vision_input->set_allocated_image_data(vision_input_image_data);
		getResponseConfig->set_allocated_vision_input(vision_input);
	}

	// Set the config object in the request object to be passed to the API
	request.Clear();
	request.set_allocated_get_response_config(getResponseConfig);


	// Do a write task
	stream_handler->Write(request, (void*)&(OnStreamWriteDelegate));
	UE_LOG(ConvaiGRPCLog, Log,
		TEXT("Initial Stream Write | Character ID : %s | Session ID : %s"),
		*ConvaiGRPCGetResponseParams.CharID,
		*ConvaiGRPCGetResponseParams.SessionID);


	// Do a read task
	stream_handler->Read(reply.get(), (void*)&OnStreamReadDelegate);
	UE_LOG(ConvaiGRPCLog, Log,
		TEXT("Initial Stream Read | Character ID : %s | Session ID : %s"),
		*ConvaiGRPCGetResponseParams.CharID,
		*ConvaiGRPCGetResponseParams.SessionID); }

void UConvaiGRPCGetResponseProxy::OnStreamWrite(bool ok)
{
	if (!IsValid(this))
	{
		UE_LOG(ConvaiGRPCLog, Warning, TEXT("OnStreamWrite failed due to pending kill!"));
		LogAndEcecuteFailure("OnStreamWrite");
		return;
	}

	if (!ok || !status.ok())
	{
		LogAndEcecuteFailure("OnStreamWrite");
		CallFinish();
		return;
	}

	if (CalledFinish)
		return;

	// UE_LOG(ConvaiGRPCLog, Log, TEXT("OnStreamWriteBegin"));

	// Clear the request data to make it ready to hold the new data we are going to send
	request.Clear();
	GetResponseRequest_GetResponseData* get_response_data = new GetResponseRequest_GetResponseData();

	bool IsThisTheFinalWrite;

	// If there is text
	if (ConvaiGRPCGetResponseParams.UserQuery.Len())
	{
		// Add in the text data
		get_response_data->set_text_data(TCHAR_TO_UTF8(*ConvaiGRPCGetResponseParams.UserQuery));
		UE_LOG(ConvaiGRPCLog, Log,
		TEXT("Sent UserQuery %s: | Character ID : %s | Session ID : %s"),
		*ConvaiGRPCGetResponseParams.UserQuery,
		*ConvaiGRPCGetResponseParams.CharID,
		*ConvaiGRPCGetResponseParams.SessionID);
		IsThisTheFinalWrite = true;
	}
	else if (ConvaiGRPCGetResponseParams.TriggerName.Len() || ConvaiGRPCGetResponseParams.TriggerMessage.Len()) // If there is a trigger message
	{
		// Add in the trigger data
		TriggerConfig* triggerConfig = new TriggerConfig();
		triggerConfig->set_trigger_name(TCHAR_TO_UTF8(*ConvaiGRPCGetResponseParams.TriggerName));
		triggerConfig->set_trigger_message(TCHAR_TO_UTF8(*ConvaiGRPCGetResponseParams.TriggerMessage));
		get_response_data->set_allocated_trigger_data(triggerConfig);
		IsThisTheFinalWrite = true;
		UE_LOG(ConvaiGRPCLog, Log,
			TEXT("Sent %s %s: | Character ID : %s | Session ID : %s"),
			*FString(ConvaiGRPCGetResponseParams.TriggerName.IsEmpty() ? "Trigger Message" : "Trigger Name"),
			*FString(ConvaiGRPCGetResponseParams.TriggerName.IsEmpty() ? ConvaiGRPCGetResponseParams.TriggerMessage : ConvaiGRPCGetResponseParams.TriggerName),
			*ConvaiGRPCGetResponseParams.CharID,
			*ConvaiGRPCGetResponseParams.SessionID);
	}
	else // Normal voice data
	{
		// Try to consume the next chunk of mic data
		uint8* Buffer = nullptr;
		TArray<uint8> Data = ConsumeFromAudioBuffer(IsThisTheFinalWrite);

		if (!Data.Num())
		{
			if (IsThisTheFinalWrite)
			{
				// Tell the server that we have finished writing
				UE_LOG(ConvaiGRPCLog, Log, TEXT("Calling Stream WritesDone | LastWriteReceived : %s | AudioBuffer.Num() : %d | Character ID : %s | Session ID : %s"),
				(LastWriteReceived ? TEXT("True") : TEXT("False")),
				AudioBuffer.Num(),
				*ConvaiGRPCGetResponseParams.CharID,
				*ConvaiGRPCGetResponseParams.SessionID);
				stream_handler->WritesDone((void*)&OnStreamWriteDoneDelegate); UE_LOG(ConvaiGRPCLog, Log, TEXT("On Stream Write Done Writing"));
				FinishedWritingToStream = true;
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

		NumberOfAudioBytesSent += Data.Num();
	}
	// Prepare the request
	request.set_allocated_get_response_data(get_response_data);

	//#if ConvaiDebugMode
	//    FString DebugString(request.DebugString().c_str());
	//    UE_LOG(ConvaiGRPCLog, Warning, TEXT("request: %s"), *DebugString);
	//#endif 

	if (IsThisTheFinalWrite)
	{
		// Send the data and tell the server that this is the last piece of data
		UE_LOG(ConvaiGRPCLog, Log, TEXT("Calling Stream WriteLast | Character ID : %s | Session ID : %s"),
			*ConvaiGRPCGetResponseParams.CharID,
			*ConvaiGRPCGetResponseParams.SessionID);
		stream_handler->WriteLast(request, grpc::WriteOptions(), (void*)&OnStreamWriteDoneDelegate);
		FinishedWritingToStream = true;
	}
	else
	{
		// Do a normal send of the data
		//UE_LOG(ConvaiGRPCLog, Log, TEXT("Calling Stream Write | LastWriteReceived : %s | AudioBuffer.Num() : %d | Character ID : %s | Session ID : %s"),
		//	(LastWriteReceived ? TEXT("True") : TEXT("False")),
		//	AudioBuffer.Num(),
		//	*ConvaiGRPCGetResponseParams.CharID,
		//	*ConvaiGRPCGetResponseParams.SessionID);
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

	if (!ok || !status.ok())
	{
		LogAndEcecuteFailure("OnStreamWriteDone");
		CallFinish();
		return;
	}

	UE_LOG(ConvaiGRPCLog, Log, TEXT("OnStreamWriteDone"));
	UE_LOG(ConvaiGRPCLog, Log, TEXT("NumberOfAudioBytesSent %i"), NumberOfAudioBytesSent);
}

void UConvaiGRPCGetResponseProxy::OnStreamRead(bool ok)
{
	if (!IsValid(this)) {
		UE_LOG(ConvaiGRPCLog, Warning, TEXT("OnStreamRead failed due to pending kill!"));
		LogAndEcecuteFailure("OnStreamRead");
		return;
	}

	if (ReceivedFinish)
		return;

	// Error handling
	if (!ok || !status.ok()) {
		// Log the error details
		//UE_LOG(ConvaiGRPCLog, Log,
		//	TEXT("OnStreamRead - Received non-ok response: ok:%s Status.ok:%s | Error message:%s | Code:%i | LastWriteReceived:%s | InformOnDataReceived:%s | AudioBuffer.Num():%d | Character ID:%s | Session ID:%s"),
		//	*FString(ok ? "True" : "False"),
		//	*FString(status.ok() ? "Ok" : "Not Ok"),
		//	*FString(status.error_message().c_str()),
		//	status.error_code(),
		//	(LastWriteReceived ? TEXT("True") : TEXT("False")),
		//	(InformOnDataReceived ? TEXT("True") : TEXT("False")),
		//	AudioBuffer.Num(),
		//	*ConvaiGRPCGetResponseParams.CharID,
		//	*ConvaiGRPCGetResponseParams.SessionID);

		// Retry logic: increment the retry counter
		RetryCount++;
		if (RetryCount >= MaxRetries) {
			UE_LOG(ConvaiGRPCLog, Log, TEXT("No more data to read after 3 attempts. Calling Finish..."));
			CallFinish();  // Close the stream after 3 attempts
			return;
		}

		// Otherwise, initiate another read to continue the stream (retrying)
		//UE_LOG(ConvaiGRPCLog, Log, TEXT("Retrying OnStreamRead... Attempt %d of %d"), RetryCount, MaxRetries);
		FPlatformProcess::Sleep(0.1f);
		stream_handler->Read(reply.get(), (void*)&OnStreamReadDelegate);
		return;
	}

	// Reset the retry counter on a successful read
	RetryCount = 0;

	ReceivedFinalResponse = reply->audio_response().end_of_response();

	// Grab the session ID
	std::string SessionID_std = reply->session_id();
	if (SessionID_std.size())
	{
		ConvaiGRPCGetResponseParams.SessionID = FString(SessionID_std.c_str());
		// Broadcast the Session ID
		OnSessionIDReceived.ExecuteIfBound(ConvaiGRPCGetResponseParams.SessionID);
	}

	std::string InteractionID_std = reply->interaction_id();
	if (InteractionID_std.size())
	{
		// Broadcast the Interaction ID
		OnInteractionIDReceived.ExecuteIfBound(FString(InteractionID_std.c_str()));
	}

	if (reply->has_user_query()) // Is there transcription ready
	{
		std::string UserQuery_std = reply->user_query().text_data();
		bool IsTranscriptionReady = reply->user_query().is_final();
		bool IsFinalTranscription = reply->user_query().end_of_response();

		// Convert UTF8 to UTF16 FString
		FString text_string = UConvaiUtils::FUTF8ToFString(UserQuery_std.c_str());

		OnTranscriptionReceived.ExecuteIfBound(text_string, IsTranscriptionReady, IsFinalTranscription);
		UE_LOG(ConvaiGRPCLog, Log,
			TEXT("Received UserQuery %s: | Character ID : %s | Session ID : %s | IsFinalTranscription : %s"),
			*text_string,
			*ConvaiGRPCGetResponseParams.CharID,
			*ConvaiGRPCGetResponseParams.SessionID,
			*FString(IsFinalTranscription ? "True" : "False"));
	}
	else if (reply->has_audio_response()) // Is there an audio response
	{
		// Grab bot text
		std::string text_string_std = reply->audio_response().text_data();

		// Convert UTF8 to UTF16 FString
		FString text_string = UConvaiUtils::FUTF8ToFString(text_string_std.c_str());

		// Grab bot audio
		::std::string audio_data = reply->audio_response().audio_data();
		TArray<uint8> VoiceData;
		float SampleRate = 0;
		if (reply->audio_response().audio_data().length() > 46)
		{
			VoiceData = TArray<uint8>(reinterpret_cast<const uint8*>(audio_data.data() + 46), audio_data.length() - 46);
			SampleRate = reply->audio_response().audio_config().sample_rate_hertz();
			UE_LOG(ConvaiGRPCLog, Log, TEXT("Received Audio Chunk: %f secs | Character ID : % s | Session ID : % s"), float(audio_data.length()) / (SampleRate * 2.0),
				*ConvaiGRPCGetResponseParams.CharID,
				*ConvaiGRPCGetResponseParams.SessionID);
		}
		FAnimationSequence FaceDataAnimation;

		if (ConvaiGRPCGetResponseParams.RequireFaceData)
		{
			bool HasVisemes = reply->audio_response().has_visemes_data();
			bool HasBlendshapesData = reply->audio_response().has_blendshapes_data();
			//bool HasBlendshapesFrame = reply->audio_response().has_blendshapes_frame();

			if (HasBlendshapesData && ConvaiGRPCGetResponseParams.GeneratesVisemesAsBlendshapes)
			{
				std::string FaceBlendshapeData = reply->audio_response().blendshapes_data().blendshape_data();
				if (FaceBlendshapeData.size() > 0)
				{
					FString FaceData_string = UConvaiUtils::FUTF8ToFString(FaceBlendshapeData.c_str());
					FaceDataAnimation.AnimationFrames = UConvaiUtils::ParseJsonToBlendShapeData(FaceData_string);
				}
			}
			//else if (HasBlendshapesFrame && ConvaiGRPCGetResponseParams.GeneratesVisemesAsBlendshapes)
			//{
			//	float FPS = reply->audio_response().blendshapes_frame().fps();
			//	int FrameIndex = reply->audio_response().blendshapes_frame().frame_index();
			//	float FrameTime = FPS == 0.0 ? 1.0 / 30.0 : 1.0 / FPS;
			//	auto BlendshapesFrame = reply->audio_response().blendshapes_frame().blendshapes();
			//	float multipier = 1.5;
			//	FAnimationFrame AnimationFrame;
			//	AnimationFrame.FrameIndex = FrameIndex;
			//	AnimationFrame.BlendShapes.Add("browDownLeft", BlendshapesFrame.brow_down_left() * 1);
			//	AnimationFrame.BlendShapes.Add("browDownRight", BlendshapesFrame.brow_down_right() * 1);
			//	AnimationFrame.BlendShapes.Add("browInnerUp", BlendshapesFrame.brow_inner_up() * 1);
			//	AnimationFrame.BlendShapes.Add("browOuterUpLeft", BlendshapesFrame.brow_outer_up_left() * 1);
			//	AnimationFrame.BlendShapes.Add("browOuterUpRight", BlendshapesFrame.brow_outer_up_right() * 1);
			//	AnimationFrame.BlendShapes.Add("cheekPuff", BlendshapesFrame.cheek_puff() * multipier);
			//	AnimationFrame.BlendShapes.Add("cheekSquintLeft", BlendshapesFrame.cheek_squint_left() * multipier);
			//	AnimationFrame.BlendShapes.Add("cheekSquintRight", BlendshapesFrame.cheek_squint_right() * multipier);

			//	AnimationFrame.BlendShapes.Add("eyeBlinkLeft", BlendshapesFrame.eye_blink_left());
			//	AnimationFrame.BlendShapes.Add("eyeBlinkRight", BlendshapesFrame.eye_blink_right());

			//	AnimationFrame.BlendShapes.Add("eyeLookDownLeft", BlendshapesFrame.eye_look_down_left() * multipier);
			//	AnimationFrame.BlendShapes.Add("eyeLookDownRight", BlendshapesFrame.eye_look_down_right() * multipier);
			//	AnimationFrame.BlendShapes.Add("eyeLookInLeft", BlendshapesFrame.eye_look_in_left() * multipier);
			//	AnimationFrame.BlendShapes.Add("eyeLookInRight", BlendshapesFrame.eye_look_in_right() * multipier);
			//	AnimationFrame.BlendShapes.Add("eyeLookOutLeft", BlendshapesFrame.eye_look_out_left() * multipier);
			//	AnimationFrame.BlendShapes.Add("eyeLookOutRight", BlendshapesFrame.eye_look_out_right() * multipier);
			//	AnimationFrame.BlendShapes.Add("eyeLookUpLeft", BlendshapesFrame.eye_look_up_left() * multipier);
			//	AnimationFrame.BlendShapes.Add("eyeLookUpRight", BlendshapesFrame.eye_look_up_right() * multipier);
			//	AnimationFrame.BlendShapes.Add("eyeSquintLeft", BlendshapesFrame.eye_squint_left() * 1);
			//	AnimationFrame.BlendShapes.Add("eyeSquintRight", BlendshapesFrame.eye_squint_right() * 1);
			//	AnimationFrame.BlendShapes.Add("eyeWideLeft", BlendshapesFrame.eye_wide_left() * 1);
			//	AnimationFrame.BlendShapes.Add("eyeWideRight", BlendshapesFrame.eye_wide_right() * 1);
			//	AnimationFrame.BlendShapes.Add("jawForward", BlendshapesFrame.jaw_forward() * multipier);
			//	AnimationFrame.BlendShapes.Add("jawLeft", BlendshapesFrame.jaw_left() * multipier);
			//	AnimationFrame.BlendShapes.Add("jawOpen", BlendshapesFrame.jaw_open() * multipier);
			//	AnimationFrame.BlendShapes.Add("jawRight", BlendshapesFrame.jaw_right() * multipier);
			//	AnimationFrame.BlendShapes.Add("mouthClose", BlendshapesFrame.mouth_close() * multipier);
			//	AnimationFrame.BlendShapes.Add("mouthDimpleLeft", BlendshapesFrame.mouth_dimple_left() * multipier);
			//	AnimationFrame.BlendShapes.Add("mouthDimpleRight", BlendshapesFrame.mouth_dimple_right() * multipier);
			//	AnimationFrame.BlendShapes.Add("mouthFrownLeft", BlendshapesFrame.mouth_frown_left() * multipier);
			//	AnimationFrame.BlendShapes.Add("mouthFrownRight", BlendshapesFrame.mouth_frown_right() * multipier);
			//	AnimationFrame.BlendShapes.Add("mouthFunnel", BlendshapesFrame.mouth_funnel() * multipier);
			//	AnimationFrame.BlendShapes.Add("mouthLeft", BlendshapesFrame.mouth_left() * multipier);
			//	AnimationFrame.BlendShapes.Add("mouthLowerDownLeft", BlendshapesFrame.mouth_lower_down_left() * multipier);
			//	AnimationFrame.BlendShapes.Add("mouthLowerDownRight", BlendshapesFrame.mouth_lower_down_right() * multipier);
			//	AnimationFrame.BlendShapes.Add("mouthPressLeft", BlendshapesFrame.mouth_press_left() * multipier);
			//	AnimationFrame.BlendShapes.Add("mouthPressRight", BlendshapesFrame.mouth_press_right() * multipier);
			//	AnimationFrame.BlendShapes.Add("mouthPucker", BlendshapesFrame.mouth_pucker() * multipier);
			//	AnimationFrame.BlendShapes.Add("mouthRight", BlendshapesFrame.mouth_right() * multipier);
			//	AnimationFrame.BlendShapes.Add("mouthRollLower", BlendshapesFrame.mouth_roll_lower() * multipier);
			//	AnimationFrame.BlendShapes.Add("mouthRollUpper", BlendshapesFrame.mouth_roll_upper() * multipier);
			//	AnimationFrame.BlendShapes.Add("mouthShrugLower", BlendshapesFrame.mouth_shrug_lower() * multipier);
			//	AnimationFrame.BlendShapes.Add("mouthShrugUpper", BlendshapesFrame.mouth_shrug_upper() * multipier);
			//	AnimationFrame.BlendShapes.Add("mouthSmileLeft", BlendshapesFrame.mouth_smile_left() * multipier);
			//	AnimationFrame.BlendShapes.Add("mouthSmileRight", BlendshapesFrame.mouth_smile_right() * multipier);
			//	AnimationFrame.BlendShapes.Add("mouthStretchLeft", BlendshapesFrame.mouth_stretch_left() * multipier);
			//	AnimationFrame.BlendShapes.Add("mouthStretchRight", BlendshapesFrame.mouth_stretch_right() * multipier);
			//	AnimationFrame.BlendShapes.Add("mouthUpperUpLeft", BlendshapesFrame.mouth_upper_up_left() * multipier);
			//	AnimationFrame.BlendShapes.Add("mouthUpperUpRight", BlendshapesFrame.mouth_upper_up_right() * multipier);
			//	AnimationFrame.BlendShapes.Add("noseSneerLeft", BlendshapesFrame.nose_sneer_left() * multipier);
			//	AnimationFrame.BlendShapes.Add("noseSneerRight", BlendshapesFrame.nose_sneer_right() * multipier);
			//	AnimationFrame.BlendShapes.Add("tongueOut", BlendshapesFrame.tongue_out());
			//	//AnimationFrame.BlendShapes.Add("headRoll", BlendshapesFrame.head_roll() * (180.0/3.14));
			//	//AnimationFrame.BlendShapes.Add("headPitch", BlendshapesFrame.head_pitch() * (180.0 / 3.14));
			//	//AnimationFrame.BlendShapes.Add("headYaw", BlendshapesFrame.head_yaw() * (180.0 / 3.14));
			//	FaceDataAnimation.AnimationFrames.Add(AnimationFrame);
			//	FaceDataAnimation.Duration = FrameTime;
			//	FaceDataAnimation.FrameRate = FPS;
			//	 UE_LOG(ConvaiGRPCLog, Log, TEXT("GetResponse FaceData: %s - Frame Time: %f - Frame Index: %d"), *AnimationFrame.ToString(), FrameTime, FrameIndex);
			//}
			else if (HasVisemes && !ConvaiGRPCGetResponseParams.GeneratesVisemesAsBlendshapes)
			{
				auto Visemes = reply->audio_response().visemes_data().visemes();

				if (Visemes.sil() >= 0)
				{
					FAnimationFrame AnimationFrame;
					AnimationFrame.BlendShapes.Add("sil", Visemes.sil());
					AnimationFrame.BlendShapes.Add("PP", Visemes.pp());
					AnimationFrame.BlendShapes.Add("FF", Visemes.ff());
					AnimationFrame.BlendShapes.Add("TH", Visemes.th());
					AnimationFrame.BlendShapes.Add("DD", Visemes.dd());
					AnimationFrame.BlendShapes.Add("kk", Visemes.kk());
					AnimationFrame.BlendShapes.Add("CH", Visemes.ch());
					AnimationFrame.BlendShapes.Add("SS", Visemes.ss());
					AnimationFrame.BlendShapes.Add("nn", Visemes.nn());
					AnimationFrame.BlendShapes.Add("RR", Visemes.rr());
					AnimationFrame.BlendShapes.Add("aa", Visemes.aa());
					AnimationFrame.BlendShapes.Add("E", Visemes.e());
					AnimationFrame.BlendShapes.Add("ih", Visemes.ih());
					AnimationFrame.BlendShapes.Add("oh", Visemes.oh());
					AnimationFrame.BlendShapes.Add("ou", Visemes.ou());
					FaceDataAnimation.AnimationFrames.Add(AnimationFrame);
					FaceDataAnimation.Duration += 0.01;
					FaceDataAnimation.FrameRate = 100;
				}
				//UE_LOG(ConvaiGRPCLog, Log, TEXT("GetResponse FaceData: %s"), *AnimationFrame.ToString());
			}

			if (VoiceData.Num() > 0 && FaceDataAnimation.Duration == 0)
			{
				float FaceDataDuration = float(VoiceData.Num() - 44) / float(SampleRate * 2); // Assuming 1 channel
				FaceDataAnimation.Duration = FaceDataDuration;
			}

			if (FaceDataAnimation.AnimationFrames.Num() > 0 && FaceDataAnimation.Duration > 0)
			{
				OnFaceDataReceived.ExecuteIfBound(FaceDataAnimation);
				TotalLipSyncResponsesReceived += 1;
			}

			if (ReceivedFinalResponse)
			{
				UE_LOG(ConvaiGRPCLog, Log, TEXT("Chatbot Total Received Lipsync Responses: %d Responses"), TotalLipSyncResponsesReceived);
				TotalLipSyncResponsesReceived = 0;
			}
		}

		if (reply->audio_response().has_emotion_response())
		{
			FString EmotionType = UConvaiUtils::FUTF8ToFString(reply->audio_response().emotion_response().emotion().c_str());
			FString EmotionScale = UConvaiUtils::FUTF8ToFString(reply->audio_response().emotion_response().scale().c_str());
			FString EmotionResponse = EmotionType + " " + EmotionScale;
			UE_LOG(ConvaiGRPCLog, Log, TEXT("EmotionResponse: %s"), *EmotionResponse);
			OnEmotionReceived.ExecuteIfBound(EmotionResponse, FAnimationFrame(), false);
		}
		if (reply->audio_response().has_blendshapes_data())
		{
			FString BlendshapesData = UConvaiUtils::FUTF8ToFString(reply->audio_response().blendshapes_data().blendshape_data().c_str());
			UE_LOG(ConvaiGRPCLog, Log, TEXT("BlendshapesData: %s"), *BlendshapesData);
		}

		// Broadcast the audio and text
		OnDataReceived.ExecuteIfBound(text_string, VoiceData, SampleRate, ReceivedFinalResponse);
		if (ReceivedFinalResponse || !text_string.IsEmpty())
		{
			UE_LOG(ConvaiGRPCLog, Log,
				TEXT("Received Text %s: | Character ID : %s | Session ID : %s | ReceivedFinalResponse : %s"),
				*text_string,
				*ConvaiGRPCGetResponseParams.CharID,
				*ConvaiGRPCGetResponseParams.SessionID,
				*FString(ReceivedFinalResponse ? "True" : "False"));
		}
	}
	else if (reply->has_action_response()) // Is there an action response
	{
		// Convert Action string to FString
		FString SequenceString = UConvaiUtils::FUTF8ToFString(reply->action_response().action().c_str());

#if ConvaiDebugMode
		UE_LOG(ConvaiGRPCLog, Log, TEXT("GetResponse SequenceString: %s"), *SequenceString);
#endif 

		// Parse the actions
		TArray<FConvaiResultAction> SequenceOfActions;
		TArray<FString> Sequence = UConvaiActions::SmartSplit(SequenceString);

		for (auto s : Sequence)
		{
			FConvaiResultAction ConvaiResultAction;
			if (UConvaiActions::ParseAction(ConvaiGRPCGetResponseParams.Environment, s, ConvaiResultAction)) // if successful
			{
				SequenceOfActions.Add(ConvaiResultAction);
			}

			UE_LOG(ConvaiGRPCLog, Log, TEXT("Action: %s"), *ConvaiResultAction.Action);
		}
		// Broadcast the actions
		OnActionsReceived.ExecuteIfBound(SequenceOfActions);
	}
	else if (reply->has_bt_response())
	{
		FString BT_Code = UConvaiUtils::FUTF8ToFString(reply->bt_response().bt_code().c_str());
		FString BT_Constants = UConvaiUtils::FUTF8ToFString(reply->bt_response().bt_constants().c_str());
		FString NarrativeSectionID = UConvaiUtils::FUTF8ToFString(reply->bt_response().narrative_section_id().c_str());
		OnNarrativeDataReceived.ExecuteIfBound(BT_Code, BT_Constants, NarrativeSectionID);
		UE_LOG(ConvaiGRPCLog, Log,
			TEXT("Narrative Section Received %s: | Character ID : %s | Session ID : %s | ReceivedFinalResponse : %s"),
			*NarrativeSectionID,
			*ConvaiGRPCGetResponseParams.CharID,
			*ConvaiGRPCGetResponseParams.SessionID,
			*FString(ReceivedFinalResponse ? "True" : "False"));
	}
	else if (!reply->emotion_response().empty())
	{
		FString EmotionResponseDebug = UConvaiUtils::FUTF8ToFString(reply->DebugString().c_str());
		UE_LOG(ConvaiGRPCLog, Log, TEXT("GetResponse EmotionResponseDebug: %s"), *EmotionResponseDebug);
		FString EmotionResponse = UConvaiUtils::FUTF8ToFString(reply->emotion_response().c_str());
		OnEmotionReceived.ExecuteIfBound(EmotionResponse, FAnimationFrame(), true);
	}
	else if (!reply->debug_log().empty()) // This is a debug message response
	{
#if ConvaiDebugMode
		FString DebugString(reply->debug_log().c_str());
		UE_LOG(ConvaiGRPCLog, Log, TEXT("Debug log: %s"), *DebugString);
#endif 
	}

	// Initiate another read task
	reply->Clear();
	stream_handler->Read(reply.get(), (void*)&OnStreamReadDelegate);
}

void UConvaiGRPCGetResponseProxy::OnStreamFinish(bool ok)
{
	ReceivedFinish = true;

	if (!ok || !status.ok())
	{
		LogAndEcecuteFailure("OnStreamFinish");
		return;
	}

#if ConvaiDebugMode
	UE_LOG(ConvaiGRPCLog, Log,
		TEXT("On Stream Finish | Character ID : %s | Session ID : %s"),
		*ConvaiGRPCGetResponseParams.CharID,
		*ConvaiGRPCGetResponseParams.SessionID);
#endif 


	OnFinish.ExecuteIfBound();
}




UConvaiGRPCSubmitFeedbackProxy* UConvaiGRPCSubmitFeedbackProxy::CreateConvaiGRPCSubmitFeedbackProxy(UObject* WorldContextObject, FString InteractionID, bool ThumbsUp, FString FeedbackText)
{
	UConvaiGRPCSubmitFeedbackProxy* Proxy = NewObject<UConvaiGRPCSubmitFeedbackProxy>();
	Proxy->WorldPtr = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	Proxy->InteractionID = InteractionID;
	Proxy->ThumbsUp = ThumbsUp;
	Proxy->FeedbackText = FeedbackText;
	return Proxy;
}

void UConvaiGRPCSubmitFeedbackProxy::Activate()
{
	OnStreamFinishDelegate = FgRPC_Delegate::CreateUObject(this, &ThisClass::OnStreamFinish);

	reply = std::unique_ptr<service::FeedbackResponse>(new service::FeedbackResponse());

	// Form Validation
	if (!(UConvaiFormValidation::ValidateSessionID(InteractionID)))
	{
		LogAndEcecuteFailure("Activate");
		return;
	}

	if (!WorldPtr.IsValid())
	{
		UE_LOG(ConvaiGRPCFeedBackLog, Warning, TEXT("WorldPtr not valid"));
		LogAndEcecuteFailure("Activate");
		return;
	}

	UConvaiSubsystem* ConvaiSubsystem = UConvaiUtils::GetConvaiSubsystem(WorldPtr.Get());
	if (!ConvaiSubsystem)
	{
		UE_LOG(ConvaiGRPCFeedBackLog, Warning, TEXT("Convai Subsystem is not valid"));
		LogAndEcecuteFailure("Activate");
		return;
	}

	// Create a new stub instance
	stub_ = ConvaiSubsystem->gRPC_Runnable->GetNewStub();
	if (!stub_)
	{
		UE_LOG(ConvaiGRPCFeedBackLog, Warning, TEXT("Could not aquire a new stub instance"));
		LogAndEcecuteFailure("Activate");
		return;
	}

	// Aquire the completion queue instance
	cq_ = ConvaiSubsystem->gRPC_Runnable->GetCompletionQueue();
	if (!cq_)
	{
		UE_LOG(ConvaiGRPCFeedBackLog, Warning, TEXT("Got an invalid completion queue instance"));
		LogAndEcecuteFailure("Activate");
		return;
	}

	bool Found;
	FString VersionName;
	FString EngineVersion;
	FString PlatformName;
	FString PluginEngineVersion;
	FString FriendlyName;

	UConvaiUtils::GetPluginInfo(FString("Convai"), Found, VersionName, FriendlyName, PluginEngineVersion);
	UConvaiUtils::GetPlatformInfo(EngineVersion, PlatformName);

	// Add metadata
	client_context.AddMetadata("engine", "Unreal Engine");
	client_context.AddMetadata("engine_version", TCHAR_TO_UTF8(*EngineVersion));
	client_context.AddMetadata("platform_name", TCHAR_TO_UTF8(*PlatformName));

	if (Found)
	{
		client_context.AddMetadata("plugin_engine_version", TCHAR_TO_UTF8(*PluginEngineVersion));
		client_context.AddMetadata("plugin_version", TCHAR_TO_UTF8(*VersionName));
		client_context.AddMetadata("plugin_base_name", TCHAR_TO_UTF8(*FriendlyName));
	}
	else
	{
		client_context.AddMetadata("plugin_engine_version", "Unknown");
		client_context.AddMetadata("plugin_version", "Unknown");
		client_context.AddMetadata("plugin_base_name", "Unknown");
	}

	FeedbackRequest_Feedback* feedbackRequest_Feedback = new FeedbackRequest_Feedback();
	feedbackRequest_Feedback->set_feedback_text(TCHAR_TO_UTF8(*FeedbackText));
	feedbackRequest_Feedback->set_thumbs_up(ThumbsUp);

	request.set_allocated_text_feedback(feedbackRequest_Feedback);
	request.set_interaction_id(TCHAR_TO_UTF8(*InteractionID));


	stream_handler = stub_->AsyncSubmitFeedback(&client_context, request, cq_);
	stream_handler->Finish(reply.get(), &status, (void*)&OnStreamFinishDelegate);
}

void UConvaiGRPCSubmitFeedbackProxy::OnStreamFinish(bool ok)
{
	if (!ok || !status.ok())
	{
		LogAndEcecuteFailure("OnStreamFinish");
		return;
	}

	Response = FString(reply->feedback_response().c_str());

	Response = FString(reply->feedback_response().c_str());

#if ConvaiDebugMode
	FString ThumbsUpString = ThumbsUp ? "True" : "False";
	UE_LOG(ConvaiGRPCFeedBackLog, Log,
		TEXT("On Stream Finish | Interaction ID : %s | Feedback Text : %s | ThumbsUp: %s"),
		*InteractionID,
		*FeedbackText,
		*ThumbsUpString);
#endif 


	success();
}

void UConvaiGRPCSubmitFeedbackProxy::BeginDestroy()
{
	client_context.TryCancel();
	stub_.reset();
	FString ThumbsUpString = ThumbsUp ? "True" : "False";
	UE_LOG(ConvaiGRPCFeedBackLog, Log,
	TEXT("On Stream Finish | Interaction ID : %s | Feedback Text : %s | ThumbsUp: %s"),
	*InteractionID,
	*FeedbackText,
	*ThumbsUpString);
	Super::BeginDestroy();
}

void UConvaiGRPCSubmitFeedbackProxy::LogAndEcecuteFailure(FString FuncName)
{
	FString ThumbsUpString = ThumbsUp ? "True" : "False";

	UE_LOG(ConvaiGRPCFeedBackLog, Warning,
		TEXT("%s: Status.ok():%s | Debug Log:%s | Error message:%s | Error Details:%s | Error Code:%i | Interaction ID : %s | Feedback Text : %s | ThumbsUp: %s"),
		*FString(FuncName),
		*FString(status.ok() ? "Ok" : "Not Ok"),
		*FString(reply->DebugString().c_str()),
		*FString(status.error_message().c_str()),
		*FString(status.error_details().c_str()),
		status.error_code(),
		*InteractionID,
		*FeedbackText,
		*ThumbsUpString);

	failed();
}

void UConvaiGRPCSubmitFeedbackProxy::failed()
{
	OnFailure.Broadcast(Response);
	finish();
}

void UConvaiGRPCSubmitFeedbackProxy::success()
{
	OnSuccess.Broadcast(Response);
	finish();
}

void UConvaiGRPCSubmitFeedbackProxy::finish()
{
}
