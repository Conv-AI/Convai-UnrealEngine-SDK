// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once

#include "ConvaiSubsystem.h"


#include "CoreMinimal.h"
#include "Sound/SoundWave.h"
#include "Net/OnlineBlueprintCallProxyBase.h"
#include "HAL/ThreadSafeBool.h"
#include "ConvaiGRPC.generated.h"


DECLARE_LOG_CATEGORY_EXTERN(ConvaiGRPCLog, Log, All);

class USoundWave;
class UConvaiEnvironment;
struct FAnimationFrame;
struct FAnimationSequence;
struct FConvaiResultAction;

DECLARE_DELEGATE_ThreeParams(FConvaiGRPCOnNarrativeDataSignature, const FString /*BT_Code*/, const FString /*BT_Constants*/, const FString /*NarrativeSectionID*/);
DECLARE_DELEGATE_ThreeParams(FConvaiGRPCOnTranscriptionSignature, const FString /*Transcription*/, bool /*IsTranscriptionReady*/, bool /*IsFinal*/);
DECLARE_DELEGATE_FourParams(FConvaiGRPCOnDataSignature, const FString /*ReceivedText*/, const TArray<uint8>& /*ReceivedAudio*/, uint32 /*SampleRate*/, bool /*IsFinal*/);
DECLARE_DELEGATE_OneParam(FConvaiGRPCOnFaceDataSignature, FAnimationSequence /*FaceData*/);
DECLARE_DELEGATE_OneParam(FConvaiGRPCOnActionsSignature, const TArray<FConvaiResultAction>& /*ActionSequence*/);
DECLARE_DELEGATE_OneParam(FConvaiGRPCOnEmotionSignature, FString /*EmotionResponse*/);
DECLARE_DELEGATE_OneParam(FConvaiGRPCOnSessiondIDSignature, FString /*SessionID*/);
DECLARE_DELEGATE(FConvaiGRPCOnEventSignature);


/**
 *
 */
UCLASS()
class UConvaiGRPCGetResponseProxy : public UObject
{
	GENERATED_BODY()
public:
	// Called when user transcription is received
	FThreadSafeDelegateWrapper<FConvaiGRPCOnTranscriptionSignature> OnTranscriptionReceived;

	// Called when new text and/or Audio data is received
	FThreadSafeDelegateWrapper<FConvaiGRPCOnDataSignature> OnDataReceived;

	// Called when new face data is received
	FThreadSafeDelegateWrapper<FConvaiGRPCOnFaceDataSignature> OnFaceDataReceived;

	// Called when actions are received
	FThreadSafeDelegateWrapper<FConvaiGRPCOnActionsSignature> OnActionsReceived;

	// Called when new SessionID is received
	FThreadSafeDelegateWrapper<FConvaiGRPCOnSessiondIDSignature> OnSessionIDReceived;

	FThreadSafeDelegateWrapper<FConvaiGRPCOnNarrativeDataSignature> OnNarrativeDataReceived;

	// Called when new Emotion is received
	FThreadSafeDelegateWrapper<FConvaiGRPCOnEmotionSignature> OnEmotionReceived;

	// Called when the response stream is done
	FThreadSafeDelegateWrapper<FConvaiGRPCOnEventSignature> OnFinish;

	// Called when there is an unsuccessful response
	FThreadSafeDelegateWrapper<FConvaiGRPCOnEventSignature> OnFailure;

	//UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DisplayName = "Convai GRPC Test", WorldContext = "WorldContextObject"), Category = "Convai|gRPC")
	static UConvaiGRPCGetResponseProxy* CreateConvaiGRPCGetResponseProxy(UObject* WorldContextObject, FString UserQuery, FString TriggerName, FString TriggerMessage, FString CharID, bool VoiceResponse, bool RequireFaceData, bool GeneratesVisemesAsBlendshapes, FString SessionID, UConvaiEnvironment* Environment, bool GenerateActions, FString API_Key);
	static UConvaiGRPCGetResponseProxy* CreateConvaiGRPCGetResponseProxy(UObject* WorldContextObject, FString UserQuery, FString CharID, bool VoiceResponse, bool RequireFaceData, bool GeneratesVisemesAsBlendshapes, FString SessionID, UConvaiEnvironment* Environment, bool GenerateActions, FString API_Key);

	void Activate();

	void WriteAudioDataToSend(uint8* Buffer, uint32 Length, bool LastWrite);

	void FinishWriting();

	//~ Begin UObject Interface.
	virtual void BeginDestroy() override;
	//~ End UObject Interface.

private:

	void CallFinish();

	TArray<uint8> ConsumeFromAudioBuffer(bool& IsThisTheFinalWrite);

	void LogAndEcecuteFailure(FString FuncName);

	void ExtendDeadline();

	void OnStreamInit(bool ok);
	void OnStreamRead(bool ok);
	void OnStreamWrite(bool ok);
	void OnStreamWriteDone(bool ok);
	void OnStreamFinish(bool ok);

	FgRPC_Delegate OnInitStreamDelegate;
	FgRPC_Delegate OnStreamReadDelegate;
	FgRPC_Delegate OnStreamWriteDelegate;
	FgRPC_Delegate OnStreamWriteDoneDelegate; // called when a stream->writesdone is successful
	
    // called when a stream->finish is successful it means that the server decided
	// to end the stream, we should check the status variable in that call
	// https://groups.google.com/g/grpc-io/c/R0NTqKaHLdE 
	FgRPC_Delegate OnStreamFinishDelegate;

	service::GetResponseRequest request;
	std::unique_ptr<service::GetResponseResponse> reply;

	// Context for the client. It could be used to convey extra information to
	// the server and/or tweak certain RPC behaviors.
	//std::unique_ptr<ClientContext> context;

	// Storage for the status of the RPC upon completion.
	grpc::Status status;

	std::unique_ptr<::grpc::ClientAsyncReaderWriter< service::GetResponseRequest, service::GetResponseResponse>> stream_handler;

	grpc::ClientContext client_context;

	// True if we are writing audio to the server, false if we are in the receiving stage
	bool StreamInProgress = false;

	bool FailAlreadyExecuted = false;

	std::unique_ptr<service::ConvaiService::Stub> stub_;

	grpc::CompletionQueue* cq_;

	uint32 NumberOfAudioBytesSent = 0;

private:
	// Inputs
	FString URL;
	FString API_Key;
	FString UserQuery;
	FString TriggerName;
	FString TriggerMessage;
	bool VoiceResponse;
	bool RequireFaceData;
	bool GeneratesVisemesAsBlendshapes;
	FString CharID;
	FString SessionID;
	bool GenerateActions;
	class UConvaiEnvironment* Environment;


private:
	// becomes true if we receive a non-ok header
	FThreadSafeBool InformOnDataReceived;

	// Stores the audio data to be streamed to the API
	TArray<uint8> AudioBuffer;

	// True when we are informed that the "AudioBuffer" is complete and no more audio will be received
	FThreadSafeBool LastWriteReceived;

	// Used to regulate access to the AudioBuffer to prevent racing condition
	FCriticalSection m_mutex;

	// Pointer to the world
	TWeakObjectPtr<UWorld> WorldPtr;

	FThreadSafeBool ReceivedFinish;
	FThreadSafeBool CalledFinish;
};
