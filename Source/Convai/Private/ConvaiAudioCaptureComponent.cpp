// Fill out your copyright notice in the Description page of Project Settings.


#include "ConvaiAudioCaptureComponent.h"
#include "ConvaiDefinitions.h"
//#include "Engine/GameEngine.h"

bool UConvaiAudioCaptureComponent::Init(int32& SampleRate)
{

	Super::Init(SampleRate);
	//SampleRate = ConvaiConstants::VoiceCaptureSampleRate;
	//NumChannels = 1;
	//GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, FString::Printf(TEXT("Init() in ConvAudioCaptureComponent.cpp, %d"), SampleRate));
    //UE_LOG(LogTemp, Warning, TEXT("Init() in ConvAudioCaptureComponent.cpp, %d "), SampleRate);
	return true;

}

void UConvaiAudioCaptureComponent::PrintDebug()
{
    UE_LOG(LogTemp, Warning, TEXT("PrintDebug() NumChannels: %i "), NumChannels);

}
