
#include "Utility/Log/ConvaiLogger.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/PlatformFile.h"
#include "Misc/App.h"
#include "HAL/PlatformProcess.h"  // for FPlatformProcess
#include "HAL/Event.h"            // for FEvent methods

FConvaiLogger& FConvaiLogger::Get()
{
    static FConvaiLogger Instance;
    return Instance;
}

FConvaiLogger::FConvaiLogger()
    : Thread(nullptr)
    , WakeEvent(FPlatformProcess::GetSynchEventFromPool(false))
    , bStopping(false)
{
    StartThread();
}

FConvaiLogger::~FConvaiLogger()
{
    ShutdownThread();
    if (WakeEvent)
    {
        FPlatformProcess::ReturnSynchEventToPool(WakeEvent);
    }
}

void FConvaiLogger::StartThread()
{
    // Logs go in "<ProjectDir>/ConvaiLog"
    const FString LogDir = FPaths::Combine(
        FPaths::ProjectDir(),
        TEXT("Saved"),
        TEXT("ConvaiLogs")
    );
    IPlatformFile& Plat = FPlatformFileManager::Get().GetPlatformFile();
    Plat.CreateDirectoryTree(*LogDir);

    LogFilePath = FPaths::Combine(
        LogDir,
        FString::Printf(
            TEXT("%s_%s.log"),
            FApp::GetProjectName(),
            *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"))
        )
    );

    Thread = FRunnableThread::Create(
        this,
        TEXT("ConvaiLoggerThread"),
        0,
        TPri_BelowNormal
    );
}

void FConvaiLogger::ShutdownThread()
{
    bStopping = true;
    if (WakeEvent) WakeEvent->Trigger();

    if (Thread)
    {
        Thread->WaitForCompletion();
        delete Thread;
        Thread = nullptr;
    }
}

uint32 FConvaiLogger::Run()
{
    while (!bStopping)
    {
        WakeEvent->Wait(500);

        TArray<FString> Batch;
        FString Msg;
        while (MessageQueue.Dequeue(Msg))
        {
            Batch.Add(Msg);
        }

        if (Batch.Num())
        {
            FString Combined = FString::Join(Batch, TEXT("\n")) + TEXT("\n");
            IPlatformFile& Plat = FPlatformFileManager::Get().GetPlatformFile();
            if (IFileHandle* Handle = Plat.OpenWrite(*LogFilePath, /*bAppend=*/ true))
            {
                FTCHARToUTF8 Converter(*Combined);
                Handle->Write(reinterpret_cast<const uint8*>(Converter.Get()), Converter.Length());
                Handle->Flush();
                delete Handle;
            }
        }
    }

    {
        TArray<FString> FinalBatch;
        FString Rem;
        while (MessageQueue.Dequeue(Rem))
        {
            FinalBatch.Add(Rem);
        }
        if (FinalBatch.Num())
        {
            const FString Combined = FString::Join(FinalBatch, TEXT("\n")) + TEXT("\n");
            IPlatformFile& Plat = FPlatformFileManager::Get().GetPlatformFile();
            if (IFileHandle* Handle = Plat.OpenWrite(*LogFilePath, /*bAppend=*/ true))
            {
                const FTCHARToUTF8 Converter(*Combined);
                Handle->Write(reinterpret_cast<const uint8*>(Converter.Get()), Converter.Length());
                Handle->Flush();
                delete Handle;
            }
        }
    }

    return 0;
}

void FConvaiLogger::Stop()
{
    bStopping = true;
}

void FConvaiLogger::Log(const FString& Message)
{
    const FString Formatted = FString::Printf(
        TEXT("[%s] %s"),
        *FDateTime::Now().ToString(TEXT("%H:%M:%S")),
        *Message
    );
    MessageQueue.Enqueue(Formatted);
    if (WakeEvent) WakeEvent->Trigger();
}


void UConvaiBlueprintLogger::C_ConvaiLog(UObject* WorldContextObject, EC_LogLevel Verbosity, const FString& Message)
{    
    const FString ContextName = WorldContextObject
        ? WorldContextObject->GetName()
        : TEXT("UnknownContext");

    const UEnum* EnumPtr = StaticEnum<EC_LogLevel>();
    const FString VerbName = EnumPtr
        ? EnumPtr->GetNameStringByValue(static_cast<int64>(Verbosity))
        : TEXT("UnknownVerbosity");

    const FString FullMessage = FString::Printf(
        TEXT("%s : %s : %s"),
        *ContextName, *VerbName, *Message);

    switch (Verbosity)
    {
    case EC_LogLevel::Verbose:
        UE_LOG(LogTemp, Verbose, TEXT("%s"), *FullMessage);
        break;
    case EC_LogLevel::Log:
        UE_LOG(LogTemp, Log,     TEXT("%s"), *FullMessage);
        break;
    case EC_LogLevel::Warning:
        UE_LOG(LogTemp, Warning, TEXT("%s"), *FullMessage);
        break;
    case EC_LogLevel::Error:
        UE_LOG(LogTemp, Error,   TEXT("%s"), *FullMessage);
        break;
    case EC_LogLevel::Fatal:
        UE_LOG(LogTemp, Fatal,   TEXT("%s"), *FullMessage);
        break;
    default:
        UE_LOG(LogTemp, Log,     TEXT("%s"), *FullMessage);
        break;
    }
    
    FConvaiLogger::Get().Log(FullMessage);
}
