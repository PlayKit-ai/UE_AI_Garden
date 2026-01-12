// Copyright PlayKit. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/IHttpRequest.h"
#include "PlayKitTypes.h"
#include "PlayKitSTTClient.generated.h"

/**
 * PlayKit Speech-to-Text Client Component
 * Provides audio transcription functionality.
 *
 * Features:
 * - Transcribe audio files
 * - Transcribe audio data from memory
 * - Multiple language support
 * - Timestamp segments
 *
 * Usage:
 * 1. Add this component to any Actor in the editor
 * 2. Configure properties in the Details panel (ModelName, Language)
 * 3. Bind to events using the "+" button (OnTranscriptionComplete, OnError)
 * 4. Call TranscribeFile() or TranscribeAudioData() to start transcription
 */
UCLASS(ClassGroup=(PlayKit), meta=(BlueprintSpawnableComponent))
class PLAYKITSDK_API UPlayKitSTTClient : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayKitSTTClient();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	//========== Configuration Properties (Edit in Details Panel) ==========//

	/** The AI model to use for transcription */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayKit|STT")
	FString ModelName = TEXT("whisper-large");

	/** Default language hint for transcription (e.g., "en", "zh", "ja") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayKit|STT")
	FString Language;

	//========== Events (Click "+" to bind in Blueprint) ==========//

	/** Fired when transcription completes */
	UPROPERTY(BlueprintAssignable, Category="PlayKit|STT")
	FOnTranscriptionComplete OnTranscriptionComplete;

	/** Fired on error */
	UPROPERTY(BlueprintAssignable, Category="PlayKit|STT")
	FOnTranscriptionError OnError;

	//========== Status ==========//

	/** Check if a request is currently in progress */
	UFUNCTION(BlueprintPure, Category="PlayKit|STT")
	bool IsProcessing() const { return bIsProcessing; }

	//========== Configuration Methods ==========//

	/** Set the model name at runtime */
	UFUNCTION(BlueprintCallable, Category="PlayKit|STT")
	void SetModelName(const FString& InModelName) { ModelName = InModelName; }

	/** Set the language at runtime */
	UFUNCTION(BlueprintCallable, Category="PlayKit|STT")
	void SetLanguage(const FString& InLanguage) { Language = InLanguage; }

	//========== Transcription ==========//

	/**
	 * Transcribe an audio file.
	 * Uses the component's configured Language property.
	 * @param FilePath Path to the audio file
	 */
	UFUNCTION(BlueprintCallable, Category="PlayKit|STT", meta=(DisplayName="Transcribe File"))
	void TranscribeFile(const FString& FilePath);

	/**
	 * Transcribe an audio file with a specific language.
	 * @param FilePath Path to the audio file
	 * @param InLanguage Language hint for transcription
	 */
	UFUNCTION(BlueprintCallable, Category="PlayKit|STT", meta=(DisplayName="Transcribe File With Language"))
	void TranscribeFileWithLanguage(const FString& FilePath, const FString& InLanguage);

	/**
	 * Transcribe audio data from memory.
	 * @param AudioData Raw audio data (WAV, MP3, etc.)
	 * @param FileName Filename hint for format detection
	 */
	UFUNCTION(BlueprintCallable, Category="PlayKit|STT", meta=(DisplayName="Transcribe Audio Data"))
	void TranscribeAudioData(const TArray<uint8>& AudioData, const FString& FileName = TEXT("audio.wav"));

	/** Cancel any in-progress request */
	UFUNCTION(BlueprintCallable, Category="PlayKit|STT")
	void CancelRequest();

private:
	void SendTranscriptionRequest(const TArray<uint8>& AudioData, const FString& FileName, const FString& InLanguage);
	void HandleTranscriptionResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> CreateAuthenticatedRequest(const FString& Url);
	void BroadcastError(const FString& ErrorCode, const FString& ErrorMessage);

private:
	bool bIsProcessing = false;

	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> CurrentRequest;
};
