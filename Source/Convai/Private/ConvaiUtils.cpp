// Copyright 2022 Convai Inc. All Rights Reserved.


#include "ConvaiUtils.h"
#include "Misc/FileHelper.h"
#include "Http.h"
#include "Containers/UnrealString.h"
#include "Containers/Map.h"
#include "Sound/SoundWave.h"
#include "AudioDevice.h"
#include "Interfaces/IAudioFormat.h"
#include "UObject/Object.h"
#include "GameFramework/PlayerController.h"
#include "Math/Vector.h"
#include "Camera/PlayerCameraManager.h"
#include "UObject/UObjectHash.h"
#include "Math/UnrealMathUtility.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/DefaultValueHelper.h"
#if PLATFORM_LINUX
	#include "Linux/LinuxPlatformFile.h"
#else
	#include "HAL/PlatformFilemanager.h"
#endif
#include "Engine/GameInstance.h"
#include "ConvaiSubsystem.h"
#include "Engine/GameEngine.h"
#include "GameFramework/Pawn.h"
#include "AudioDecompress.h"
#include "Runtime/Launch/Resources/Version.h"

#include "../Convai.h"
#include "ConvaiChatbotComponent.h"
#include "ConvaiPlayerComponent.h"

#include "Interfaces/IPluginManager.h"
#include "Engine/EngineTypes.h"

#if ENGINE_MAJOR_VERSION == 5
#include "AudioDecompress.h"
#endif


DEFINE_LOG_CATEGORY(ConvaiUtilsLog);
DEFINE_LOG_CATEGORY(ConvaiFormValidationLog);

namespace
{
	FString GetAbsolutePathFromFilePath(const FString& FilePath)
	{
		FString ProcessedPath;
		// Check if path is relative
		if (FPaths::IsRelative(FilePath))
		{
			// Combine the build directory with the relative file path
			ProcessedPath = FPaths::Combine(FPaths::LaunchDir(), FilePath);
		}
		else
		{
			// Path is absolute, use as is
			ProcessedPath = FilePath;
		}
		return ProcessedPath;
	}
};


UConvaiSubsystem* UConvaiUtils::GetConvaiSubsystem(const UObject* WorldContextObject)
{
	//UWorld* World = WorldPtr.Get();

	if (!WorldContextObject)
	{
		UE_LOG(ConvaiUtilsLog, Warning, TEXT("WorldContextObject ptr is invalid!"));
		return nullptr;
	}

	UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(WorldContextObject);
	if (!GameInstance)
	{
		UE_LOG(ConvaiUtilsLog, Warning, TEXT("Could not get pointer to a GameInstance"));
		return nullptr;
	}


	if (UConvaiSubsystem* ConvaiSubsystem = GameInstance->GetSubsystem<UConvaiSubsystem>())
	{
		return ConvaiSubsystem;
	}
	else
	{
		UE_LOG(ConvaiUtilsLog, Warning, TEXT("Could not get pointer to Convai Subsystem"));
		return nullptr;
	}

}

void UConvaiUtils::StereoToMono(TArray<uint8> stereoWavBytes, TArray<uint8>& monoWavBytes)
{
	//Change wav headers
	for (int i = 0; i < 44; i++)
	{
		//NumChannels starts from 22 to 24
		if (i == 22)
		{
			short NumChannels = (*(short*)&stereoWavBytes[i]);
			//UE_LOG(ConvaiUtilsLog, Warning, TEXT("NumChannels %d"), NumChannels);
			if (NumChannels == 1)
			{
				monoWavBytes = stereoWavBytes;
				return;
			}
			NumChannels = 1;
			monoWavBytes.Append((uint8*)&NumChannels, sizeof(NumChannels));
			i++;
		}
		//ByteRate starts from 28 to 32
		else if (i == 28)
		{
			int ByteRate = (*(int*)&stereoWavBytes[i]) / 2;
			monoWavBytes.Append((uint8*)&ByteRate, sizeof(ByteRate));
			i += 3;
		}
		//BlockAlign starts from 32 to 34
		else if (i == 32)
		{
			short BlockAlign = (*(short*)&stereoWavBytes[i]) / 2;
			monoWavBytes.Append((uint8*)&BlockAlign, sizeof(BlockAlign));
			i++;
		}
		//SubChunkSize starts from 40 to 44
		else if (i == 40)
		{
			int SubChunkSize = (*(int*)&stereoWavBytes[i]) / 2;
			monoWavBytes.Append((uint8*)&SubChunkSize, sizeof(SubChunkSize));
			i += 3;
		}
		else
		{
			monoWavBytes.Add(stereoWavBytes[i]);
		}
	}

	//Copies only the left channel and ignores the right channel
	for (int i = 44; i < stereoWavBytes.Num(); i += 4)
	{
		monoWavBytes.Add(stereoWavBytes[i]);
		monoWavBytes.Add(stereoWavBytes[i + 1]);
	}
}

bool UConvaiUtils::ReadFileAsByteArray(const FString FilePath, TArray<uint8>& Bytes)
{
	FString ProcessedFilePath = GetAbsolutePathFromFilePath(FilePath);
	return FFileHelper::LoadFileToArray(Bytes, *ProcessedFilePath, 0);
}

bool UConvaiUtils::SaveByteArrayAsFile(FString FilePath, TArray<uint8> Bytes)
{
	return FFileHelper::SaveArrayToFile(Bytes, *FilePath);
}

FString UConvaiUtils::ByteArrayToString(TArray<uint8> Bytes)
{
	FString s = BytesToString(Bytes.GetData(), Bytes.Num());
	FString Fixed;

	for (int i = 0; i < s.Len(); i++)
	{
		const TCHAR c = *(*s + i) - 1;
		Fixed.AppendChar(c);
	}
	return Fixed;
}

bool UConvaiUtils::WriteStringToFile(const FString& StringToWrite, const FString& FilePath)
{
	return FFileHelper::SaveStringToFile(StringToWrite, *FilePath);
}

bool UConvaiUtils::ReadStringFromFile(FString& OutString, const FString& FilePath)
{
	FString ProcessedFilePath = GetAbsolutePathFromFilePath(FilePath);
	return FFileHelper::LoadFileToString(OutString, *ProcessedFilePath);
}

float UConvaiUtils::CalculateAudioDuration(uint32 AudioSize, uint8 Channels, uint32 SampleRate, uint8 SampleSize)
{
	if (Channels == 0 || SampleRate == 0 || SampleSize == 0)
	{
		// Avoid division by zero
		return 0;
	}

	// Calculate the duration in seconds
	return float(AudioSize) / float(Channels * SampleRate * SampleSize);
}

void UConvaiUtils::ConvaiGetLookedAtCharacter(UObject* WorldContextObject, APlayerController* PlayerController, float Radius, bool PlaneView, TArray<UObject*> IncludedCharacters, TArray<UObject*> ExcludedCharacters, UConvaiChatbotComponent*& ConvaiCharacter, bool& Found)
{
	Found = false;
	float FocuseDotThresshold = 0.5;
	FVector CameraLocation, CameraForward;

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		UE_LOG(ConvaiUtilsLog, Warning, TEXT("Could not get a pointer to world!"));
		return;
	}

	if (!PlayerController)
	{
		PlayerController = UGameplayStatics::GetPlayerController(WorldContextObject, 0);
	}

	if (!PlayerController)
	{
		UE_LOG(ConvaiUtilsLog, Warning, TEXT("GetLookedAtCharacter: Could not get a pointer to PlayerController"));
		return;
	}

	if (PlayerController->PlayerCameraManager)
	{
		CameraLocation = PlayerController->PlayerCameraManager->GetCameraLocation();
		CameraForward = PlayerController->PlayerCameraManager->GetTransformComponent()->GetForwardVector();
	}
	else if (PlayerController->GetPawn())
	{
		CameraLocation = PlayerController->GetPawn()->GetActorLocation();
		CameraForward = PlayerController->GetPawn()->GetActorForwardVector();
	}
	else
	{
		UE_LOG(ConvaiUtilsLog, Warning, TEXT("GetLookedAtCharacter: Could not get a camera location"));
		return;
	}


	if (PlaneView)
	{
		CameraLocation.Z = 0;
		CameraForward.Z = 0;
		CameraForward.Normalize();
	}

	TArray<UObject*> ConvaiCharacters;
	GetObjectsOfClass(UConvaiChatbotComponent::StaticClass(), ConvaiCharacters, true, RF_ClassDefaultObject);

	for (int32 CharacterIndex = 0; CharacterIndex < ConvaiCharacters.Num(); ++CharacterIndex)
	{
		UConvaiChatbotComponent* CurrentConvaiCharacter = Cast<UConvaiChatbotComponent>(ConvaiCharacters[CharacterIndex]);
		check(CurrentConvaiCharacter);

		if (!IsValid(CurrentConvaiCharacter))
			continue;


		AActor* Owner = CurrentConvaiCharacter->GetOwner();

		if (Owner == nullptr || CurrentConvaiCharacter->GetWorld() != World)
			continue;

		bool Exclude = false;
		bool Include = false;

		for (UObject* CharacterToExclude : ExcludedCharacters)
		{
			if (!IsValid(CharacterToExclude))
				continue;

			if (CharacterToExclude == CurrentConvaiCharacter || CharacterToExclude == Owner)
			{
				Exclude = true;
				break;
			}
		}

		if (IncludedCharacters.Num())
		{
			for (UObject* CharacterToInclude : IncludedCharacters)
			{
				if (!IsValid(CharacterToInclude))
					continue;

				if (CharacterToInclude == CurrentConvaiCharacter || CharacterToInclude == Owner)
				{
					Include = true;
					break;
				}
			}
		}
		else
		{
			Include = true;
		}

		if (Exclude || !Include)
		{
			continue;
		}
		float DistSquared = 0;
		float DistSquared2D = 0;
		FVector CurrentCharacterLocation = CurrentConvaiCharacter->GetComponentLocation();
		if (PlaneView)
		{
			DistSquared2D = FVector::DistSquared2D(CurrentCharacterLocation, CameraLocation);
			if (Radius > 0 && DistSquared2D > Radius * Radius)
				continue;
		}
		else
		{
			DistSquared = FVector::DistSquared(CurrentCharacterLocation, CameraLocation);
			if (Radius > 0 && DistSquared > Radius * Radius)
				continue;
		}

		FVector DirCameraToCharacter = CurrentCharacterLocation - CameraLocation;
		if (PlaneView)
			DirCameraToCharacter.Z = 0;


		DirCameraToCharacter.Normalize();
		float CurrentFocuseDot = FVector::DotProduct(DirCameraToCharacter, CameraForward);
		float mxnScore = -1;
		float score = 0;

		if (PlaneView) {
			score = CurrentFocuseDot / DistSquared2D;
		}
		else {
			score = CurrentFocuseDot / DistSquared;
		}

		if (score > mxnScore && CurrentFocuseDot >= FocuseDotThresshold)
		{
			mxnScore = score;
			FocuseDotThresshold = CurrentFocuseDot;
			ConvaiCharacter = CurrentConvaiCharacter;
			Found = true;
			//UE_LOG(ConvaiUtilsLog, Log, TEXT("GetLookedAtCharacter: Found! %s = %f"), *CurrentConvaiCharacter->GetFullName(), FocuseDotThresshold);

		}
	}
}

void UConvaiUtils::ConvaiGetLookedAtObjectOrCharacter(UObject* WorldContextObject, APlayerController* PlayerController, float Radius, bool PlaneView, TArray<FConvaiObjectEntry> ListToSearchIn, FConvaiObjectEntry& FoundObjectOrCharacter, bool& Found)
{
	Found = false;
	float FocuseDotThresshold = 0.5;
	FVector CameraLocation, CameraForward;

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		UE_LOG(ConvaiUtilsLog, Warning, TEXT("Could not get a pointer to world!"));
		return;
	}

	if (!PlayerController)
	{
		PlayerController = UGameplayStatics::GetPlayerController(WorldContextObject, 0);
	}

	if (!PlayerController)
	{
		UE_LOG(ConvaiUtilsLog, Warning, TEXT("ConvaiGetLookedAtActor: Could not get a pointer to PlayerController"));
		return;
	}

	if (PlayerController->PlayerCameraManager)
	{
		CameraLocation = PlayerController->PlayerCameraManager->GetCameraLocation();
		CameraForward = PlayerController->PlayerCameraManager->GetTransformComponent()->GetForwardVector();
	}
	else if (PlayerController->GetPawn())
	{
		CameraLocation = PlayerController->GetPawn()->GetActorLocation();
		CameraForward = PlayerController->GetPawn()->GetActorForwardVector();
	}
	else
	{
		UE_LOG(ConvaiUtilsLog, Warning, TEXT("ConvaiGetLookedAtActor: Could not get a camera location"));
		return;
	}


	if (PlaneView)
	{
		CameraLocation.Z = 0;
		CameraForward.Z = 0;
		CameraForward.Normalize();
	}


	for (int32 ItemIndex = 0; ItemIndex < ListToSearchIn.Num(); ++ItemIndex)
	{
		FConvaiObjectEntry CurrentItem = ListToSearchIn[ItemIndex];

		TWeakObjectPtr<AActor> CurrentItemRef = CurrentItem.Ref;

		if (!CurrentItemRef.IsValid())
			continue;

		if (CurrentItemRef->GetWorld() != World)
			continue;

		float DistSquared = 0;
		float DistSquared2D = 0;
		FVector CurrentItemLocation = CurrentItemRef->GetActorLocation();
		if (PlaneView)
		{
			DistSquared2D = FVector::DistSquared2D(CurrentItemLocation, CameraLocation);
			if (Radius > 0 && DistSquared2D > Radius * Radius)
				continue;
		}
		else
		{
			DistSquared = FVector::DistSquared(CurrentItemLocation, CameraLocation);
			if (Radius > 0 && DistSquared > Radius * Radius)
				continue;
		}

		FVector DirCameraToItem = CurrentItemLocation - CameraLocation;
		if (PlaneView)
			DirCameraToItem.Z = 0;

		DirCameraToItem.Normalize();
		float CurrentFocuseDot = FVector::DotProduct(DirCameraToItem, CameraForward);
		float mxnScore = -1;
		float score = 0;

		if (PlaneView) {
			score = CurrentFocuseDot / DistSquared2D;
		}
		else {
			score = CurrentFocuseDot / DistSquared;
		}

		if (score > mxnScore && CurrentFocuseDot >= FocuseDotThresshold)
		{
			mxnScore = score;
			FocuseDotThresshold = CurrentFocuseDot;
			FoundObjectOrCharacter = CurrentItem;
			Found = true;
		}
	}
}

void UConvaiUtils::ConvaiGetAllPlayerComponents(UObject* WorldContextObject, TArray<class UConvaiPlayerComponent*>& ConvaiPlayerComponents)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		UE_LOG(ConvaiUtilsLog, Warning, TEXT("Could not get a pointer to world!"));
		return;
	}

	ConvaiPlayerComponents.Empty();

	TArray<UObject*> ConvaiPlayerComponentsObjects;
	GetObjectsOfClass(UConvaiPlayerComponent::StaticClass(), ConvaiPlayerComponentsObjects, true, RF_ClassDefaultObject);

	for (int32 Index = 0; Index < ConvaiPlayerComponentsObjects.Num(); ++Index)
	{
		UConvaiPlayerComponent* CurrentConvaiPlayer = Cast<UConvaiPlayerComponent>(ConvaiPlayerComponentsObjects[Index]);

		if (!IsValid(CurrentConvaiPlayer))
			continue;

		AActor* Owner = CurrentConvaiPlayer->GetOwner();

		if (Owner == nullptr || CurrentConvaiPlayer->GetWorld() != World)
			continue;

		ConvaiPlayerComponents.Add(CurrentConvaiPlayer);
	}
}

void UConvaiUtils::ConvaiGetAllChatbotComponents(UObject* WorldContextObject, TArray<class UConvaiChatbotComponent*>& ConvaiChatbotComponents)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		UE_LOG(ConvaiUtilsLog, Warning, TEXT("Could not get a pointer to world!"));
		return;
	}

	ConvaiChatbotComponents.Empty();

	TArray<UObject*> ConvaiChatbotComponentsObjects;
	GetObjectsOfClass(UConvaiChatbotComponent::StaticClass(), ConvaiChatbotComponentsObjects, true, RF_ClassDefaultObject);

	for (int32 Index = 0; Index < ConvaiChatbotComponentsObjects.Num(); ++Index)
	{
		UConvaiChatbotComponent* CurrentConvaiChatbot = Cast<UConvaiChatbotComponent>(ConvaiChatbotComponentsObjects[Index]);

		if (!IsValid(CurrentConvaiChatbot))
			continue;

		AActor* Owner = CurrentConvaiChatbot->GetOwner();

		if (Owner == nullptr || CurrentConvaiChatbot->GetWorld() != World)
			continue;

		ConvaiChatbotComponents.Add(CurrentConvaiChatbot);
	}
}

void UConvaiUtils::SetAPI_Key(FString API_Key)
{
	Convai::Get().GetConvaiSettings()->API_Key = API_Key;
}

FString UConvaiUtils::GetAPI_Key()
{
	return Convai::Get().GetConvaiSettings()->API_Key;
}

void UConvaiUtils::SetAuthToken(FString AuthToken)
{
	Convai::Get().GetConvaiSettings()->AuthToken = AuthToken;
}

FString UConvaiUtils::GetAuthToken()
{
	return Convai::Get().GetConvaiSettings()->AuthToken;
}

TPair<FString, FString> UConvaiUtils::GetAuthHeaderAndKey()
{
	FString API_Key = GetAPI_Key();
	FString AuthToken = GetAuthToken();

	FString KeyOrToken;
	FString HeaderString;

	if (!API_Key.IsEmpty())
	{
		KeyOrToken = API_Key;
		HeaderString = ConvaiConstants::API_Key_Header;
	}
	else if (!AuthToken.IsEmpty())
	{
		KeyOrToken = AuthToken;
		HeaderString = ConvaiConstants::Auth_Token_Header;
	}
	else
	{
		// Handle the case where both are empty if necessary
		KeyOrToken = "";
		HeaderString = "";
	}

	return TPair<FString, FString>(HeaderString, KeyOrToken);
}

FString UConvaiUtils::GetTestCharacterID()
{
	return Convai::Get().GetConvaiSettings()->TestCharacterID;
}

bool UConvaiUtils::IsNewActionSystemEnabled()
{
	return Convai::Get().GetConvaiSettings()->EnableNewActionSystem;
}

void UConvaiUtils::GetPluginInfo(FString PluginName, bool& Found, FString& VersionName, FString& EngineVersion, FString& FriendlyName)
{
	IPluginManager& PluginManager = IPluginManager::Get();
	TSharedPtr<IPlugin> Plugin = PluginManager.FindPlugin(PluginName);
	Found = false;

	if (Plugin.IsValid())
	{
		const FPluginDescriptor& PluginDescriptor = Plugin->GetDescriptor();
		VersionName = PluginDescriptor.VersionName;
		EngineVersion = PluginDescriptor.EngineVersion;
		FriendlyName = PluginDescriptor.FriendlyName;
		Found = true;
	}
}

void UConvaiUtils::GetPlatformInfo(FString& EngineVersion, FString& PlatformName)
{
	// Get the global engine version
	EngineVersion = FEngineVersion::Current().ToString();

	// Get the platform name
#if PLATFORM_WINDOWS
	PlatformName = TEXT("Windows");
#elif PLATFORM_MAC
	PlatformName = TEXT("Mac");
#elif PLATFORM_LINUX
	PlatformName = TEXT("Linux");
#elif PLATFORM_ANDROID
	PlatformName = TEXT("Android");
#else
	PlatformName = TEXT("Unknown");
#endif
}

namespace
{
	// This struct contains information about the sound buffer.
	struct SongBufferInfo
	{
		int32 RawPCMDataSize;
		int32 NumChannels;
		float Duration;
		int32 SampleRate;

		SongBufferInfo() : RawPCMDataSize(0), NumChannels(0), Duration(0), SampleRate(0) {}

		SongBufferInfo(int32 PCMDataSize, int32 numChannels, float duration, int32 sampleRate)
			: RawPCMDataSize(PCMDataSize), NumChannels(numChannels), Duration(duration), SampleRate(sampleRate)
		{
		}
	};

	// this struct contains the sound buffer + information about it.
	struct SongBufferData
	{
		TArray<uint8> RawPCMData;
		SongBufferInfo BufferInfo;

		// default to nothing.
		SongBufferData() : SongBufferData(0, 0, 0, 0) {}

		// allocate memory as we populate the structure.
		SongBufferData(int32 PCMDataSize, int32 numChannels, float duration, int32 sampleRate)
			: BufferInfo(PCMDataSize, numChannels, duration, sampleRate)
		{
			RawPCMData.SetNumZeroed(PCMDataSize);
		}
	};

	USoundWave* WavDataToSoundwave(TArray<uint8> Data)
	{
		FWaveModInfo WaveInfo;
		FString ErrorReason;
		if (WaveInfo.ReadWaveInfo(Data.GetData(), Data.Num(), &ErrorReason))
		{
			USoundWave* SoundWave = NewObject<USoundWave>();

			//From FSoundWavePCMWriter::ApplyBufferToSoundWave() UE4.24
			SoundWave->SetSampleRate(*WaveInfo.pSamplesPerSec);
			SoundWave->NumChannels = *WaveInfo.pChannels;

			const int32 BytesDataPerSecond = *WaveInfo.pChannels * (*WaveInfo.pBitsPerSample / 8.f) * *WaveInfo.pSamplesPerSec;
			if (BytesDataPerSecond)
			{
				SoundWave->Duration = float(WaveInfo.SampleDataSize) / float(BytesDataPerSecond);
			}

			SoundWave->RawPCMDataSize = WaveInfo.SampleDataSize;

			SoundWave->RawPCMData = static_cast<uint8*>(FMemory::Malloc(WaveInfo.SampleDataSize));
			FMemory::Memcpy(SoundWave->RawPCMData, WaveInfo.SampleDataStart, WaveInfo.SampleDataSize);
			return SoundWave;
		}
		else
		{
			//UE_LOG(ConvaiT2SHttpLog, Warning, TEXT("%s"), *ErrorReason);
			return nullptr;
		}
	}

	bool DecompressUSoundWave(USoundWave* soundWave, TSharedPtr<SongBufferData>& Out_SongBufferData)
	{
		FAudioDevice* audioDevice = GEngine ? GEngine->GetMainAudioDeviceRaw() : nullptr;

		if (!audioDevice || !soundWave || soundWave->GetName() == TEXT("None"))
			return false;

		bool breturn = false;

#if ENGINE_MAJOR_VERSION == 4 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 3)

		// Ensure we have the sound data. Compressed format is fine.
		soundWave->InitAudioResource(audioDevice->GetRuntimeFormat(soundWave));

		// Create a decoder for this audio. We want the PCM data.
		ICompressedAudioInfo* AudioInfo = audioDevice->CreateCompressedAudioInfo(soundWave);

#else
		// Ensure we have the sound data. Compressed format is fine.
		soundWave->InitAudioResource(soundWave->GetRuntimeFormat());
		
		// Create a decoder for this audio. We want the PCM data.
		ICompressedAudioInfo* AudioInfo = IAudioInfoFactoryRegistry::Get().Create(soundWave->GetRuntimeFormat());
#endif

		// Decompress complete audio to this buffer
		FSoundQualityInfo QualityInfo = { 0 };
#if ENGINE_MAJOR_VERSION == 4
		if (AudioInfo->ReadCompressedInfo(soundWave->ResourceData, soundWave->ResourceSize, &QualityInfo))
		{
			Out_SongBufferData = TSharedPtr<SongBufferData>(new SongBufferData(
				QualityInfo.SampleDataSize, QualityInfo.NumChannels, QualityInfo.Duration, QualityInfo.SampleRate));

			// Decompress all the sample data into preallocated memory now
			AudioInfo->ExpandFile(Out_SongBufferData->RawPCMData.GetData(), &QualityInfo);

			breturn = true;
		}
#else
		FAudioDevice* AudioDevice = GEngine->GetMainAudioDeviceRaw();
		if (AudioDevice)
		{
			#if (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 3)
			FName format = AudioDevice->GetRuntimeFormat(soundWave);
			#else
			FName format = soundWave->GetRuntimeFormat();
			#endif
			soundWave->InitAudioResource(format);
		}

		const uint8* ResourceData = soundWave->GetResourceData();
		uint32 ResourceSize = soundWave->GetResourceSize();

		if (!ResourceData || ResourceSize <= 0)
		{
			return breturn;
		}

		if (AudioInfo->ReadCompressedInfo(ResourceData, ResourceSize, &QualityInfo))
		{
			Out_SongBufferData = TSharedPtr<SongBufferData>(new SongBufferData(
				QualityInfo.SampleDataSize, QualityInfo.NumChannels, QualityInfo.Duration, QualityInfo.SampleRate));

			// Decompress all the sample data into preallocated memory now
			AudioInfo->ExpandFile(Out_SongBufferData->RawPCMData.GetData(), &QualityInfo);

			breturn = true;
		}
#endif
		// Clean up.
		delete AudioInfo;

		return breturn;
	}
};

TArray<uint8> UConvaiUtils::ExtractPCMDataFromSoundWave(USoundWave* SoundWave, int32& OutSampleRate, int32& OutNumChannels)
{
	TArray<uint8> PCMData;

	if (!SoundWave)
	{
		UE_LOG(LogTemp, Warning, TEXT("SoundWave is null!"));
		return PCMData;
	}

	if (SoundWave->RawPCMDataSize > 0)
	{
		PCMData.Append(SoundWave->RawPCMData, SoundWave->RawPCMDataSize);
		OutSampleRate = SoundWave->GetSampleRateForCurrentPlatform();
		OutNumChannels = SoundWave->NumChannels;
	}
	else
	{
		TSharedPtr<SongBufferData> SongBuffer;
		if (DecompressUSoundWave(SoundWave, SongBuffer) && SongBuffer.IsValid())
		{
			PCMData = SongBuffer->RawPCMData;
			OutSampleRate = SongBuffer->BufferInfo.SampleRate;
			OutNumChannels = SongBuffer->BufferInfo.NumChannels;
		}
	}
	return PCMData;
}

void UConvaiUtils::PCMDataToWav(TArray<uint8> InPCMBytes, TArray<uint8>& OutWaveFileData, int NumChannels, int SampleRate)
{
	SerializeWaveFile(OutWaveFileData, InPCMBytes.GetData(), InPCMBytes.Num(), NumChannels, SampleRate);
}

USoundWave* UConvaiUtils::PCMDataToSoundWav(TArray<uint8> InPCMBytes, int NumChannels, int SampleRate)
{
	if (InPCMBytes.Num() <= 44)
		return nullptr;

	TArray<uint8> OutSerializeWave;
	// insert the .wav format headers at the beggining of the PCM data
	SerializeWaveFile(OutSerializeWave, InPCMBytes.GetData(), InPCMBytes.Num(), NumChannels, SampleRate);

	// Save the wav file to disk for debug
	//FString SaveDir = "C:\\Users\\pc\\Videos\\MetahumansConvaiTutorial\\outtest.wav";
	//UE_LOG(ConvaiUtilsLog, Log, TEXT("OutSerializeWave.Num() final: %d bytes "), OutSerializeWave.Num());
	//FFileHelper::SaveArrayToFile(OutSerializeWave, *SaveDir);

	return UConvaiUtils::WavDataToSoundWave(OutSerializeWave);
}

USoundWave* UConvaiUtils::WavDataToSoundWave(TArray<uint8> InWavData)
{
	return WavDataToSoundwave(InWavData);
}

void UConvaiUtils::ResampleAudio(float currentSampleRate, float targetSampleRate, int numChannels, bool reduceToMono, int16* currentPcmData, int numSamplesToConvert, TArray<int16>& outResampledPcmData)
{
	// Calculate the ratio of input to output sample rates
	float sampleRateRatio = currentSampleRate / targetSampleRate;

	// Determine the number of output channels
	int outNumChannels = reduceToMono ? 1 : numChannels;

	// Determine the number of frames to iterate over
	int32 numFramesToConvert = FMath::CeilToInt((float)numSamplesToConvert / (float)numChannels);

	// Calculate the number output frames
	int32 numOutputFrames = FMath::CeilToInt(numFramesToConvert * targetSampleRate / currentSampleRate);

	// Resize the output array to the expected size
	outResampledPcmData.Reset(numOutputFrames * outNumChannels);

	// Initialize variables for tracking the current and next frame indices
	float currentFrameIndex = 0.0f;
	float nextFrameIndex = 0.0f;

	// Iterate over the frames and resample the audio
	for (;;)
	{
		// Calculate the next frame index
		nextFrameIndex += sampleRateRatio;

		if (currentFrameIndex >= numFramesToConvert || nextFrameIndex > numFramesToConvert)
		{
			break;
		}

		// Calculate the number of input samples to average over
		int32 numInputSamplesToAverage = FMath::CeilToInt(nextFrameIndex - currentFrameIndex);

		// Initialize the sum of the input samples
		int32 sumOfInputSamples = 0;

		// Sum the input samples
		for (int channel = 0; channel < outNumChannels; ++channel)
		{
			for (int inputSampleIndex = 0; inputSampleIndex < numInputSamplesToAverage; ++inputSampleIndex)
			{
				int32 currentSampleIndex = FMath::FloorToInt(currentFrameIndex + inputSampleIndex) * numChannels + channel;
				int16 currentSampleValue = *(currentPcmData + currentSampleIndex);
				sumOfInputSamples += currentSampleValue;
			}

			// Calculate the average of the input samples
			int16 averageSampleValue = (int16)(sumOfInputSamples / numInputSamplesToAverage);

			// Add the resampled sample to the output array
			outResampledPcmData.Add(averageSampleValue);
		}

		// Update the current frame index
		currentFrameIndex = nextFrameIndex;
	}
}

void UConvaiUtils::ResampleAudio(float currentSampleRate, float targetSampleRate, int numChannels, bool reduceToMono, const TArray<int16>& currentPcmData, int numSamplesToConvert, TArray<int16>& outResampledPcmData)
{
	// Call the other function using this instance
	ResampleAudio(currentSampleRate, targetSampleRate, numChannels, reduceToMono, (int16*)currentPcmData.GetData(), numSamplesToConvert, outResampledPcmData);
}

FString UConvaiUtils::FUTF8ToFString(const char* StringToConvert)
{
	// Create a TCHAR (wide string) from the UTF-8 string using Unreal's FUTF8ToTCHAR class
	FUTF8ToTCHAR Converter(StringToConvert);

	// Create an FString from the converted wide string
	FString text_string(Converter.Get());

	return text_string;
}

int UConvaiUtils::LevenshteinDistance(const FString& s, const FString& t)
{
	// Degenerate cases
	if (s == t) return 0;
	if (s.Len() == 0) return t.Len();
	if (t.Len() == 0) return s.Len();

	// Create two work vectors of integer distances
	TArray<int32> v0;
	v0.Init(0, t.Len() + 1);
	TArray<int32> v1;
	v1.Init(0, t.Len() + 1);

	// Initialize v0 (the previous row of distances)
	// This row is A[0][i]: edit distance for an empty s
	// The distance is just the number of characters to delete from t
	for (int32 i = 0; i < v0.Num(); i++)
	{
		v0[i] = i;
	}

	for (int32 i = 0; i < s.Len(); i++)
	{
		// Calculate v1 (current row distances) from the previous row v0

		// First element of v1 is A[i+1][0]
		// Edit distance is delete (i+1) characters from s to match an empty t
		v1[0] = i + 1;

		// Use formula to fill in the rest of the row
		for (int32 j = 0; j < t.Len(); j++)
		{
			int32 cost = (s[i] == t[j]) ? 0 : 2; // Here, we change the cost of substitution to 2
			v1[j + 1] = FMath::Min3(v1[j] + 1, v0[j + 1] + 1, v0[j] + cost);
		}

		// Copy v1 (current row) to v0 (previous row) for next iteration
		for (int32 j = 0; j < v0.Num(); j++)
		{
			v0[j] = v1[j];
		}
	}

	return v1[t.Len()];
}

TArray<FAnimationFrame> UConvaiUtils::ParseJsonToBlendShapeData(const FString& JsonString)
{
	TArray<FAnimationFrame> AnimationFrames;

	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	TSharedPtr<FJsonValue> JsonParsed;
	if (FJsonSerializer::Deserialize(Reader, JsonParsed) && JsonParsed.IsValid() && JsonParsed->Type == EJson::Array)
	{
		TArray<TSharedPtr<FJsonValue>> FrameArray = JsonParsed->AsArray();
		for (auto FrameVal : FrameArray)
		{
			TSharedPtr<FJsonObject> FrameObj = FrameVal->AsObject();
			FAnimationFrame NewFrame;

			NewFrame.FrameIndex = FrameObj->GetIntegerField("FrameIndex");

			TArray<TSharedPtr<FJsonValue>> BlendShapeArray = FrameObj->GetArrayField("BlendShapes");
			for (auto BlendShapeVal : BlendShapeArray)
			{
				TSharedPtr<FJsonObject> BlendShapeObj = BlendShapeVal->AsObject();
				FName name = FName(BlendShapeObj->GetStringField("name"));
				double score;
				bool Success = BlendShapeObj->TryGetNumberField("score", score);
				if (!Success)
					score = 0;

				NewFrame.BlendShapes.Add(name, score);
			}

			AnimationFrames.Add(NewFrame);
		}
	}

	return AnimationFrames;
}

bool UConvaiUtils::ParseVisemeValuesToAnimationFrame(const FString& VisemeValuesString, FAnimationFrame& AnimationFrame)
{
	// Split the input string by ','
	TArray<FString> StringValues;
	VisemeValuesString.ParseIntoArray(StringValues, TEXT(","));

	// Ensure that the number of parsed values is the same as the number of viseme names
	if (StringValues.Num() != ConvaiConstants::VisemeNames.Num())
	{
		// Log an error message and return the uninitialized FAnimationFrame object
		//UE_LOG(LogTemp, Error, TEXT("Number of values does not match the number of viseme names."));
		return false;
	}

	float ValuesSum = 0.0f; // Used to check if all blendshapes are zeros

	bool ignore = true;
	// Loop over the parsed string values and the viseme names simultaneously
	for (int32 Index = 0; Index < StringValues.Num(); ++Index)
	{
		// Convert each string value to a float and add it to the TMap in AnimationFrame
		float Value;
		if (StringValues[Index].TrimStartAndEnd().IsNumeric())
		{
			Value = FCString::Atof(*StringValues[Index]);
			AnimationFrame.BlendShapes.Add(*ConvaiConstants::VisemeNames[Index], Value);
			if (Value > 0.03)
				ignore = false;
			ValuesSum += Value; // Add the value to the sum
		}
		else
		{
			// Log a warning message if a string value is not numeric
			//UE_LOG(LogTemp, Warning, TEXT("Invalid numeric value: %s"), *StringValues[Index])
			Value = 0;
			AnimationFrame.BlendShapes.Add(*ConvaiConstants::VisemeNames[Index], Value);
		}
	}

	// If the sum of all parsed values is close to zero, set the first blendshape to 1
	if (ValuesSum < 0.1 || ignore)
	{
		if (ConvaiConstants::VisemeNames.Num() > 0)
		{
			AnimationFrame.BlendShapes[*ConvaiConstants::VisemeNames[0]] = 1.0f;
			return false;
		}
	}

	return true;
}

AActor* UConvaiUtils::ConvaiCloneActor(AActor* InputActor)
{
	UWorld* World = InputActor->GetWorld();
	FActorSpawnParameters params;
	params.Template = InputActor;

	UClass* ItemClass = InputActor->GetClass();
	AActor* const SpawnedActor = World->SpawnActor<AActor>(ItemClass, params);
	return SpawnedActor;
}

FString UConvaiUtils::ConvaiAnimationSequenceToJson(const FAnimationSequenceBP& AnimationSequenceBP)
{
	return AnimationSequenceBP.AnimationSequence.ToJson();
}

void UConvaiUtils::ConvaiAnimationSequenceFromJson(const FString& JsonString, FAnimationSequenceBP& AnimationSequenceBP)
{
	AnimationSequenceBP.AnimationSequence.FromJson(JsonString);
}

TMap<FName, float> UConvaiUtils::MapBlendshapes(const TMap<FName, float>& InputBlendshapes, const TMap<FName, FConvaiBlendshapeParameters>& BlendshapeMap, float GlobalMultiplier, float GlobalOffset)
{
	TMap<FName, float> OutputMap;

	// Generate arrays for original blendshape names and values
	TArray<FName> OriginalNames;
	TArray<float> OriginalValues;
	InputBlendshapes.GenerateKeyArray(OriginalNames);
	InputBlendshapes.GenerateValueArray(OriginalValues);

	// Loop through each original blendshape
	for (int i = 0; i < OriginalNames.Num(); i++)
	{
		FName OriginalName = OriginalNames[i];
		float OriginalValue = OriginalValues[i];

		// Check if the original name has a mapped parameter
		const FConvaiBlendshapeParameters* MappedParameter = BlendshapeMap.Find(OriginalName);

		if (MappedParameter)
		{
			float Multiplier = MappedParameter->Multiplyer;
			float Offset = MappedParameter->Offset;
			bool UseOverrideValue = MappedParameter->UseOverrideValue;
			float OverrideValue = MappedParameter->OverrideValue;
			float ClampMinValue = MappedParameter->ClampMinValue;
			float ClampMaxValue = MappedParameter->ClampMaxValue;
			bool IgnoreGlobalModifiers = MappedParameter->IgnoreGlobalModifiers;

			// Loop through each target name specified in the mapped parameter
			for (FName TargetName : MappedParameter->TargetNames)
			{
				if (UseOverrideValue)
				{
					// Use the override value if specified
					OutputMap.Add(TargetName, OverrideValue);
				}
				else
				{
					// Calculate the final blendshape value using the multiplier and offset
					float CalculatedValue;
					if (IgnoreGlobalModifiers)
					{
						CalculatedValue = Multiplier * OriginalValue + Offset;
					}
					else
					{
						CalculatedValue = Multiplier * OriginalValue * GlobalMultiplier + Offset + GlobalOffset;
					}

					CalculatedValue = CalculatedValue > ClampMaxValue ? ClampMaxValue : CalculatedValue;
					CalculatedValue = CalculatedValue < ClampMinValue ? ClampMinValue : CalculatedValue;

					// If this curve appeared before then choose the higher value
					if (float* PreviousValue = OutputMap.Find(TargetName))
					{
						if (CalculatedValue <= *PreviousValue)
						{
							continue;
						}
					}

					OutputMap.Add(TargetName, CalculatedValue);
				}
			}
		}
		else
		{
			// If no mapped parameter exists for the original name, keep the original value
			OutputMap.Add(OriginalName, OriginalValue);
		}
	}

	return OutputMap;
}

bool UConvaiSettingsUtils::GetParamValueAsString(const FString& paramName, FString& outValue) {
	FString input = Convai::Get().GetConvaiSettings()->ExtraParams;
	FString result;
	FString trimmedInput = input.Replace(TEXT(" "), TEXT("")); // Remove all spaces
	if (trimmedInput.Split(paramName + TEXT("="), nullptr, &result)) {
		result.Split(TEXT(","), &result, nullptr);
		outValue = result.TrimStartAndEnd().Replace(TEXT("\""), TEXT("")).TrimStartAndEnd();
		return true;
	}
	outValue = FString();
	return false;
}

bool UConvaiSettingsUtils::GetParamValueAsFloat(const FString& paramName, float& outValue) {
	FString stringValue;
	if (GetParamValueAsString(paramName, stringValue)) {
		FDefaultValueHelper::ParseFloat(stringValue, outValue);
		return true;
	}
	outValue = 0.0f;
	return false;
}

bool UConvaiSettingsUtils::GetParamValueAsInt(const FString& paramName, int32& outValue) {
	FString stringValue;
	if (GetParamValueAsString(paramName, stringValue)) {
		FDefaultValueHelper::ParseInt(stringValue, outValue);
		return true;
	}
	outValue = 0;
	return false;
}

bool UConvaiUtils::WriteSoundWaveToWavFile(USoundWave* SoundWave, const FString& FilePath)
{
	// Access the raw PCM data from the USoundWave
	int32 OutSampleRate;
	int32 OutNumChannels;
	TArray<uint8> RawData = ExtractPCMDataFromSoundWave(SoundWave, OutSampleRate, OutNumChannels);

#if ENGINE_MAJOR_VERSION < 5
	if (RawData.Num() == 0)
		return false;
#else
	if (RawData.IsEmpty())
		return false;
#endif

	TArray<uint8> OutWaveFileData;

	PCMDataToWav(RawData, OutWaveFileData, OutNumChannels, OutSampleRate);

	// Use the helper function to save the raw PCM data as a .wav file
	return SaveByteArrayAsFile(FilePath, OutWaveFileData);
}

USoundWave* UConvaiUtils::ReadWavFileAsSoundWave(const FString & FilePath)
{
	FString ProcessedFilePath = GetAbsolutePathFromFilePath(FilePath);

	TArray<uint8> RawData;
	if (!ReadFileAsByteArray(ProcessedFilePath, RawData))
	{
		return nullptr;
	}

	// Create a new USoundWave object
	USoundWave* NewSoundWave = WavDataToSoundWave(RawData);

	return NewSoundWave;
}
