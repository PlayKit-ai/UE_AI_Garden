// Copyright PlayKit. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/IHttpRequest.h"
#include "Engine/Texture2D.h"
#include "PlayKitTypes.h"
#include "PlayKitImageClient.generated.h"

/**
 * PlayKit Image Client Component
 * Provides AI image generation functionality.
 *
 * Features:
 * - Single image generation
 * - Batch image generation
 * - Various size options
 * - Base64 to Texture2D conversion
 *
 * Usage:
 * 1. Add this component to any Actor in the editor
 * 2. Configure properties in the Details panel (ModelName, Size, etc.)
 * 3. Bind to events using the "+" button (OnImageGenerated, OnError)
 * 4. Call GenerateImage() with a prompt to start generation
 */
UCLASS(ClassGroup=(PlayKit), meta=(BlueprintSpawnableComponent))
class PLAYKITSDK_API UPlayKitImageClient : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayKitImageClient();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	//========== Configuration Properties (Edit in Details Panel) ==========//

	/** The AI model to use for image generation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayKit|Image")
	FString ModelName = TEXT("dall-e-3");

	/** Default image size (e.g., "1024x1024", "1792x1024", "1024x1792") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayKit|Image")
	FString ImageSize = TEXT("1024x1024");

	/** Image quality (standard or hd) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayKit|Image")
	FString Quality = TEXT("standard");

	/** Number of images to generate (1-10) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayKit|Image", meta=(ClampMin="1", ClampMax="10"))
	int32 ImageCount = 1;

	/** Seed for reproducible results (-1 for random) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayKit|Image")
	int32 Seed = -1;

	//========== Events (Click "+" to bind in Blueprint) ==========//

	/** Fired when a single image is generated */
	UPROPERTY(BlueprintAssignable, Category="PlayKit|Image")
	FOnImageGenerated OnImageGenerated;

	/** Fired when multiple images are generated */
	UPROPERTY(BlueprintAssignable, Category="PlayKit|Image")
	FOnImagesGenerated OnImagesGenerated;

	/** Fired on error */
	UPROPERTY(BlueprintAssignable, Category="PlayKit|Image")
	FOnImageError OnError;

	//========== Status ==========//

	/** Check if a request is currently in progress */
	UFUNCTION(BlueprintPure, Category="PlayKit|Image")
	bool IsProcessing() const { return bIsProcessing; }

	//========== Configuration Methods ==========//

	/** Set the model name at runtime */
	UFUNCTION(BlueprintCallable, Category="PlayKit|Image")
	void SetModelName(const FString& InModelName) { ModelName = InModelName; }

	/** Set the image size at runtime */
	UFUNCTION(BlueprintCallable, Category="PlayKit|Image")
	void SetImageSize(const FString& InSize) { ImageSize = InSize; }

	/** Set the quality at runtime */
	UFUNCTION(BlueprintCallable, Category="PlayKit|Image")
	void SetQuality(const FString& InQuality) { Quality = InQuality; }

	//========== Image Generation ==========//

	/**
	 * Generate an image from a text prompt.
	 * Uses the component's configured properties (ModelName, ImageSize, etc.).
	 * @param Prompt Text description of the desired image
	 */
	UFUNCTION(BlueprintCallable, Category="PlayKit|Image", meta=(DisplayName="Generate Image"))
	void GenerateImage(const FString& Prompt);

	/**
	 * Generate an image with a specific seed for reproducibility.
	 * @param Prompt Text description of the desired image
	 * @param InSeed Seed for reproducible results
	 */
	UFUNCTION(BlueprintCallable, Category="PlayKit|Image", meta=(DisplayName="Generate Image With Seed"))
	void GenerateImageWithSeed(const FString& Prompt, int32 InSeed);

	/**
	 * Generate multiple images with full configuration.
	 * @param Prompt Text description of the desired images
	 * @param Options Generation options
	 */
	UFUNCTION(BlueprintCallable, Category="PlayKit|Image", meta=(DisplayName="Generate Images (Advanced)"))
	void GenerateImagesAdvanced(const FString& Prompt, const FPlayKitImageOptions& Options);

	//========== Utility ==========//

	/**
	 * Convert Base64 encoded image data to a Texture2D.
	 * @param Base64Data Base64 encoded image data
	 * @return The created texture, or nullptr on failure
	 */
	UFUNCTION(BlueprintCallable, Category="PlayKit|Image|Utility", meta=(DisplayName="Base64 to Texture2D"))
	static UTexture2D* Base64ToTexture2D(const FString& Base64Data);

	/** Cancel any in-progress request */
	UFUNCTION(BlueprintCallable, Category="PlayKit|Image")
	void CancelRequest();

private:
	void SendImageRequest(const FString& Prompt, const FPlayKitImageOptions& Options);
	void HandleImageResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> CreateAuthenticatedRequest(const FString& Url);
	void BroadcastError(const FString& ErrorCode, const FString& ErrorMessage);

private:
	bool bIsProcessing = false;
	FString LastPrompt;

	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> CurrentRequest;
};
