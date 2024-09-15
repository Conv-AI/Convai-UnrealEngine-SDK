#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "VisionInterface.h"
#include "ImageCaptureComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class CONVAI_API UConvaiSingleImageCaptureComponent : public USceneComponent, public IConvaiVisionInterface
{
	GENERATED_BODY()

public:
	// Constructor
	UConvaiSingleImageCaptureComponent();

protected:
	// A simple UTexture2D variable exposed to Blueprint
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Convai|Vision")
	UTexture2D* CapturedImage;

	// Current state of the vision component
	EVisionState CurrentState;

	// The last error message
	FString LastErrorMessage;

	// The last error code
	int LastErrorCode;

	// Helper function to get image data as raw bytes
	bool GetRawImageData(TArray<uint8>& OutData, int& width, int& height);

public:
	// IConvaiVisionInterface implementation

	/** Starts the image capture */
	UFUNCTION(BlueprintCallable, Category = "Convai|Vision")
	virtual void Start() override;

	/** Stops the image capture */
	UFUNCTION(BlueprintCallable, Category = "Convai|Vision")
	virtual void Stop() override;

	/** Returns the current vision state */
	UFUNCTION(BlueprintCallable, Category = "Convai|Vision")
	virtual EVisionState GetState() const override;

	/** Sets the max FPS (not relevant here, so we return -1) */
	virtual void SetMaxFPS(int MaxFPS) override;

	/** Returns the max FPS (not relevant here, so we return -1) */
	virtual int GetMaxFPS() const override;

	/** Always returns false as there's no compressed format readily available */
	virtual bool IsCompressedDataAvailable() const override;

	/** Always returns false as there's no compressed format readily available */
	virtual bool GetCompressedData(int& width, int& height, TArray<uint8>& data) override;

	/** Captures a compressed image using a specific compression ratio (JPEG) */
	virtual bool CaptureCompressed(int& width, int& height, TArray<uint8>& data, float ForceCompressionRatio) override;

	/** Captures a raw image (width and height of the image are set, and data array is populated) */
	virtual bool CaptureRaw(int& width, int& height, TArray<uint8>& data) override;

	/** Returns the texture of the captured image */
	UFUNCTION(BlueprintCallable, Category = "Convai|Vision")
	virtual UTexture* GetImageTexture(ETextureSourceType& TextureSourceType) override;

	/** Returns the last error message */
	UFUNCTION(BlueprintCallable, Category = "Convai|Vision")
	virtual FString GetLastErrorMessage() const override;

	/** Returns the last error code */
	UFUNCTION(BlueprintCallable, Category = "Convai|Vision")
	virtual int GetLastErrorCode() const override;
};