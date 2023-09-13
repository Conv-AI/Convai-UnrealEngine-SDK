// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once

#include "LipSyncInterface.h"
#include "Components/SceneComponent.h"
#include "Containers/Map.h"
#include "ConvaiDefinitions.h"
#include "ConvaiFaceSync.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(ConvaiFaceSyncLog, Log, All);



UCLASS(meta = (BlueprintSpawnableComponent), DisplayName = "Convai Face Sync")
class UConvaiFaceSyncComponent : public USceneComponent, public IConvaiLipSyncExtendedInterface
{
	GENERATED_BODY()
public:
	UConvaiFaceSyncComponent();

	virtual ~UConvaiFaceSyncComponent();

	// UActorComponent interface
	//virtual void BeginPlay() override;
	// virtual void OnRegister() override;
	// virtual void OnUnregister() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	// End UActorComponent interface

	// IConvaiLipSyncInterface
	virtual void ConvaiProcessLipSync(uint8* InPCMData, uint32 InPCMDataSize, uint32 InSampleRate, uint32 InNumChannels) override { return; }
	virtual void ConvaiStopLipSync() override;
	virtual TArray<float> ConvaiGetVisemes() override { return TArray<float>(); }
	virtual TArray<FString> ConvaiGetVisemeNames() override { return TArray<FString>(); }
	// End IConvaiLipSyncInterface interface

	// IConvaiLipSyncExtendedInterface
	virtual void ConvaiProcessLipSyncAdvanced(uint8* InPCMData, uint32 InPCMDataSize, uint32 InSampleRate, uint32 InNumChannels, FAnimationSequence FaceSequence) override;
	virtual bool RequiresPreGeneratedFaceData() override { return true; }
	virtual bool GeneratesVisemesAsBlendshapes() override { return true; }
	virtual TMap<FName, float> ConvaiGetFaceBlendshapes() override { return CurrentBlendShapes; }
	// End IConvaiLipSyncExtendedInterface interface

	bool IsValidSequence(FAnimationSequence Sequence);

	const static TArray<FName> BlendShapesNames;
	const static TMap<FName, float> ZeroFrame;

	UPROPERTY(EditAnywhere, Category = "Convai|LipSync")
	float AnchorValue = 0.5;

private:
	TArray<FAnimationSequence> Sequences;
	float CurrentSequenceTimePassed;
	TMap<FName, float> CurrentBlendShapes;
};