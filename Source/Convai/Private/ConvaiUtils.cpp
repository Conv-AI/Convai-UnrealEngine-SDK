// Copyright 2022 Convai Inc. All Rights Reserved.


#include "ConvaiUtils.h"
#include "Misc/FileHelper.h"
#include "Http.h"
#include "Containers/UnrealString.h"
#include <AudioDecoder.h>
#include "Sound/SoundWave.h"
#include "UObject/Object.h"
#include "GameFramework/PlayerController.h"
#include "Math/Vector.h"
#include "Camera/PlayerCameraManager.h"
#include "UObject/UObjectHash.h"
#include "../Convai.h"

#include "ConvaiChatbotComponent.h"

DEFINE_LOG_CATEGORY(ConvaiUtilsLog);
DEFINE_LOG_CATEGORY(ConvaiFormValidationLog);

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


bool UConvaiUtils::ReadFileAsByteArray(FString FilePath, TArray<uint8>& Bytes)
{
	return FFileHelper::LoadFileToArray(Bytes, *FilePath, 0);
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

		if (!IsValid(CurrentConvaiCharacter) || CurrentConvaiCharacter->IsPendingKill())
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

		FVector CurrentCharacterLocation = CurrentConvaiCharacter->GetComponentLocation();
		if (PlaneView)
		{
			if (Radius > 0 && FVector::DistSquared2D(CurrentCharacterLocation, CameraLocation) > Radius * Radius)
				continue;
		}
		else
		{
			if (Radius > 0 && FVector::DistSquared(CurrentCharacterLocation, CameraLocation) > Radius * Radius)
				continue;
		}

		FVector DirCameraToCharacter = CurrentCharacterLocation - CameraLocation;
		if (PlaneView)
			DirCameraToCharacter.Z = 0;


		DirCameraToCharacter.Normalize();
		float CurrentFocuseDot = FVector::DotProduct(DirCameraToCharacter, CameraForward);

		if (CurrentFocuseDot > FocuseDotThresshold)
		{
			FocuseDotThresshold = CurrentFocuseDot;
			ConvaiCharacter = CurrentConvaiCharacter;
			Found = true;
			//UE_LOG(ConvaiUtilsLog, Log, TEXT("GetLookedAtCharacter: Found! %s = %f"), *CurrentConvaiCharacter->GetFullName(), FocuseDotThresshold);

		}
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


namespace
{

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
				SoundWave->Duration = WaveInfo.SampleDataSize / BytesDataPerSecond;
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

}

USoundWave* UConvaiUtils::DecodeMP3(TArray<uint8> InMP3Bytes) {

	//return nullptr;
	 //Getting basic audio information
	float Duration;
	int NumOfChannels;
	int SampleRate;

	char* OutputBytes = nullptr;
	unsigned long DataSize = AudioDecodeMP3((char*)InMP3Bytes.GetData(), InMP3Bytes.Num(), OutputBytes, Duration, NumOfChannels, SampleRate);

	if (DataSize <= 0)
		return nullptr;

	TArray<uint8> OutDecodedBytes = TArray<uint8>((uint8*)OutputBytes, DataSize);

	//UE_LOG(ConvaiUtilsLog, Warning, TEXT("%d %d %d %d"), DataSize, Duration, NumOfChannels, SampleRate);
	return UConvaiUtils::PCMDataToSoundWav(OutDecodedBytes, NumOfChannels, SampleRate);

}

void UConvaiUtils::PCMDataToWav(TArray<uint8> InPCMBytes, TArray<uint8>& OutWaveFileData, int NumChannels, int SampleRate)
{
	SerializeWaveFile(OutWaveFileData, InPCMBytes.GetData(), InPCMBytes.Num(), NumChannels, SampleRate);
}

USoundWave* UConvaiUtils::PCMDataToSoundWav(TArray<uint8> InPCMBytes, int NumChannels, int SampleRate)
{
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
