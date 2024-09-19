#include "ImageCaptureComponent.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Runtime/Launch/Resources/Version.h"

#include "Modules/ModuleManager.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFilemanager.h"
#include "Engine/Texture2D.h"
#include "Rendering/Texture2DResource.h" // For FTexture2DResource
#include "ImageUtils.h"                  // For FImageUtils

UConvaiSingleImageCaptureComponent::UConvaiSingleImageCaptureComponent()
{
	// Initialize variables
	CapturedImage = nullptr;
	CurrentState = EVisionState::Stopped;
	LastErrorMessage = TEXT("No Error");
	LastErrorCode = 0;
}

void UConvaiSingleImageCaptureComponent::Start()
{
	// Set the state to Capturing
	CurrentState = EVisionState::Capturing;
	OnVisionStateChanged.ExecuteIfBound(CurrentState);
}

void UConvaiSingleImageCaptureComponent::Stop()
{
	// Set the state to Stopped
	CurrentState = EVisionState::Stopped;
	OnVisionStateChanged.ExecuteIfBound(CurrentState);
}

EVisionState UConvaiSingleImageCaptureComponent::GetState() const
{
	return CurrentState;
}

void UConvaiSingleImageCaptureComponent::SetMaxFPS(int MaxFPS)
{
	// Max FPS is not relevant here, do nothing
}

int UConvaiSingleImageCaptureComponent::GetMaxFPS() const
{
	return -1;  // Not relevant, return -1
}

bool UConvaiSingleImageCaptureComponent::IsCompressedDataAvailable() const
{
	return false;  // Indicating that we do not have a ready compressed format
}

// Extracts raw image data (PNG) and converts it to RGB (if needed)
bool UConvaiSingleImageCaptureComponent::GetRawImageData(TArray<uint8>& OutData, int& width, int& height)
{
	if (!CapturedImage)
	{
		LastErrorMessage = TEXT("No image captured");
		LastErrorCode = 1;
		return false;
	}

	// Get the first mip level (highest resolution) from the captured texture
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
	FTexture2DMipMap& Mip = CapturedImage->GetPlatformData()->Mips[0];
#else
	FTexture2DMipMap& Mip = CapturedImage->PlatformData->Mips[0];
#endif
	width = Mip.SizeX;
	height = Mip.SizeY;

	// Lock the bulk data to access the raw pixel data
	void* RawData = Mip.BulkData.Lock(LOCK_READ_ONLY);
	
	const int32 NumPixels = width * height;

	// Check the pixel format and process accordingly
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
	if (CapturedImage->GetPlatformData()->PixelFormat == EPixelFormat::PF_B8G8R8A8 ||
		CapturedImage->GetPlatformData()->PixelFormat == EPixelFormat::PF_R8G8B8A8)
#else
	if (CapturedImage->PlatformData->PixelFormat == EPixelFormat::PF_B8G8R8A8 ||
		CapturedImage->PlatformData->PixelFormat == EPixelFormat::PF_R8G8B8A8)
#endif
	{
		// Prepare output array, 3 bytes per pixel (RGB)
		OutData.SetNumUninitialized(NumPixels * 4);

		uint8* SrcPtr = static_cast<uint8*>(RawData); // Source pointer for raw texture data
		uint8* DstPtr = OutData.GetData();           // Destination pointer for RGB data

		FMemory::Memcpy(DstPtr, SrcPtr, OutData.Num());

		//for (int32 i = 0; i < NumPixels; ++i)
		//{
		//	if (CapturedImage->PlatformData->PixelFormat == EPixelFormat::PF_B8G8R8A8)
		//	{
		//		FMemory::Memcpy(DstPtr, SrcPtr, OutData.Num());
		//	}
		//	else if (CapturedImage->PlatformData->PixelFormat == EPixelFormat::PF_R8G8B8A8)
		//	{
		//		// Convert R8G8B8A8 (RGBA) to RGB
		//		*DstPtr++ = SrcPtr[0]; // R
		//		*DstPtr++ = SrcPtr[1]; // G
		//		*DstPtr++ = SrcPtr[2]; // B
		//		SrcPtr += 4;            // Skip alpha
		//	}
		//}
	}
	else
	{
		// Unsupported pixel format, log error
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
		LastErrorMessage = FString::Printf(TEXT("Unsupported pixel format: %d"), static_cast<int32>(CapturedImage->GetPlatformData()->PixelFormat));
#else
		LastErrorMessage = FString::Printf(TEXT("Unsupported pixel format: %d"), static_cast<int32>(CapturedImage->PlatformData->PixelFormat));
#endif
		LastErrorCode = 4;
		Mip.BulkData.Unlock();
		return false;
	}

	// Unlock the bulk data after reading
	Mip.BulkData.Unlock();
	return true;
}

bool UConvaiSingleImageCaptureComponent::GetCompressedData(int& width, int& height, TArray<uint8>& data)
{
	return false;
}

bool UConvaiSingleImageCaptureComponent::CaptureCompressed(int& width, int& height, TArray<uint8>& data, float ForceCompressionRatio)
{
	TArray<uint8> RawData;
	if (!GetRawImageData(RawData, width, height))
	{
		return false;
	}

	// Use IImageWrapperModule to compress the raw image data into JPEG with specified compression quality
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);

	if (ImageWrapper.IsValid() && ImageWrapper->SetRaw(RawData.GetData(), RawData.Num(), width, height, ERGBFormat::BGRA, 8))
	{
		const int32 Quality = FMath::Clamp<int32>(ForceCompressionRatio, 1, 100);  // ForceCompressionRatio is expected to be between 0 and 100
		data = ImageWrapper->GetCompressed(Quality);
		return true;
	}

	LastErrorMessage = TEXT("Failed to compress image");
	LastErrorCode = 3;
	return false;
}

bool UConvaiSingleImageCaptureComponent::CaptureRaw(int& width, int& height, TArray<uint8>& data)
{
	return GetRawImageData(data, width, height);
}

UTexture* UConvaiSingleImageCaptureComponent::GetImageTexture(ETextureSourceType& TextureSourceType)
{
	TextureSourceType = ETextureSourceType::Texture2D;
	return CapturedImage;  // Return the captured image texture
}

FString UConvaiSingleImageCaptureComponent::GetLastErrorMessage() const
{
	return LastErrorMessage;
}

int UConvaiSingleImageCaptureComponent::GetLastErrorCode() const
{
	return LastErrorCode;
}