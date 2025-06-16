// ConvaiLogger.h
#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Containers/Queue.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"  
#include "HAL/PlatformFile.h"         
#include "HAL/ThreadSafeBool.h"
#include "HAL/Event.h"

#if WITH_EDITOR
  // In editor: log to both UE4’s log window AND your file logger
  #define CONVAI_LOG(Category, Verbosity, Format, ...)                  \
  UE_LOG(Category, Verbosity, Format, ##__VA_ARGS__);					\
  FConvaiLogger::Get().Log(                                             \
  FString::Printf(														\
  TEXT(#Category) TEXT(" : ") TEXT(#Verbosity) TEXT(" : ") Format,		\
  ##__VA_ARGS__															\
  )																		\
  );     

#else
  #define CONVAI_LOG(Category, Verbosity, Format, ...)                      \
  FConvaiLogger::Get().Log(                                                 \
  FString::Printf(															\
  TEXT(#Category) TEXT(" : ") TEXT(#Verbosity) TEXT(" : ") Format,			\
  ##__VA_ARGS__																\
  )																			\
  );
#endif

/**
 * Asynchronous, file-based logger singleton using IFileHandle in append mode.
 */
class CONVAI_API FConvaiLogger final : public FRunnable
{
public:
	static FConvaiLogger& Get();
	void Log(const FString& Message);

	// FRunnable interface
	virtual uint32 Run() override;
	virtual void Stop() override;
	
private:
	FConvaiLogger();
	virtual ~FConvaiLogger() override;

	// start/stop thread
	void StartThread();
	void ShutdownThread();

	FRunnableThread* Thread;
	TQueue<FString, EQueueMode::Mpsc> MessageQueue;
	FEvent* WakeEvent;
	FString LogFilePath;
	FThreadSafeBool bStopping;
};
