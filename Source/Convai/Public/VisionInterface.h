// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ConvaiDefinitions.h"
#include "Engine/Texture.h"
#include "VisionInterface.generated.h"


UENUM(BlueprintType)
enum class ETextureSourceType : uint8
{
	Texture2D UMETA(DisplayName = "Texture2D"),
	RenderTarget2D UMETA(DisplayName = "RenderTarget2D")
};

UENUM(BlueprintType)
enum class EVisionState : uint8
{
	Stopped UMETA(DisplayName = "Stopped"),        // Vision system is inactive and stopped
	Starting UMETA(DisplayName = "Starting"),      // Vision system is initializing
	Capturing UMETA(DisplayName = "Capturing"),    // Vision system is capturing data
	Stopping UMETA(DisplayName = "Stopping"),      // Vision system is in the process of stopping
	Paused UMETA(DisplayName = "Paused"),          // Vision system is paused and not capturing
};

// Delegate to handle state change notifications
DECLARE_DELEGATE_OneParam(FOnVisionStateChanged, EVisionState);

// Delegate to handle when the first frame has been captured
DECLARE_DELEGATE(FOnFirstFrameCaptured);

// Delegate to handle when frames have stopped capturing
DECLARE_DELEGATE(FOnFramesStopped);

UINTERFACE()
class CONVAI_API UConvaiVisionInterface : public UInterface
{
	GENERATED_BODY()
};

class IConvaiVisionInterface
{
	GENERATED_BODY()

public:
	// Delegate called when the vision state changes
	FOnVisionStateChanged OnVisionStateChanged;

	// Delegate called when the first frame is captured
	FOnFirstFrameCaptured OnFirstFrameCaptured;

	// Delegate called when frame capturing stops
	FOnFramesStopped OnFramesStopped;

	/**
	 * Starts the vision component.
	 * This will transition the state from Stopped or Paused to Capturing.
	 */
	virtual void Start() = 0;

	/**
	 * Stops the vision component.
	 * This will transition the state from Capturing or Paused to Stopped.
	 */
	virtual void Stop() = 0;

	/**
	 * Retrieves the current vision state.
	 * @return The current EVisionState of the component.
	 */
	virtual EVisionState GetState() const = 0;

	/**
	 * Sets the maximum frames per second (FPS) that the vision component will capture.
	 * @param MaxFPS The maximum FPS value to set.
	 */
	virtual void SetMaxFPS(int MaxFPS) = 0;

	/**
	 * Gets the current maximum FPS setting.
	 * @return The maximum FPS setting.
	 */
	virtual int GetMaxFPS() const = 0;

	/**
	 * Checks if compressed image data is already available (e.g., received from an external source).
	 * @return True if compressed data is available, false otherwise.
	 */
	virtual bool IsCompressedDataAvailable() const = 0;

	/**
	 * Retrieves the pre-existing compressed image data (if available).
	 * @param width The width of the compressed image.
	 * @param height The height of the compressed image.
	 * @param data The array to store the compressed image data.
	 * @return True if compressed data was successfully retrieved, false otherwise.
	 */
	virtual bool GetCompressedData(int& width, int& height, TArray<uint8>& data) = 0;

	/**
	 * Captures the current frame in a compressed format with a specific compression ratio.
	 * @param width The width of the captured image, populated by the function.
	 * @param height The height of the captured image, populated by the function.
	 * @param data An array to store the compressed image data, populated by the function.
	 * @param ForceCompressionRatio A specific compression ratio to apply during capture.
	 * @return True if capture succeeded, false otherwise.
	 */
	virtual bool CaptureCompressed(int& width, int& height, TArray<uint8>& data, float ForceCompressionRatio) = 0;

	/**
	 * Captures the current frame in raw format (uncompressed).
	 * @param width The width of the captured image, populated by the function.
	 * @param height The height of the captured image, populated by the function.
	 * @param data An array to store the raw image data, populated by the function.
	 * @return True if capture succeeded, false otherwise.
	 */
	virtual bool CaptureRaw(int& width, int& height, TArray<uint8>& data) = 0;

	/**
	 * Retrieves the texture of the captured image based on the source type.
	 * @param TextureSourceType The type of texture source (Texture2D or RenderTarget2D).
	 * @return A pointer to the UTexture containing the captured image.
	 */
	virtual UTexture* GetImageTexture(ETextureSourceType& TextureSourceType) = 0;

	/**
	 * Retrieves the last error message encountered by the vision component.
	 * @return The last error message as a string.
	 */
	virtual FString GetLastErrorMessage() const = 0;

	/**
	 * Retrieves the last error code encountered by the vision component.
	 * @return The last error code.
	 */
	virtual int GetLastErrorCode() const = 0;
};
