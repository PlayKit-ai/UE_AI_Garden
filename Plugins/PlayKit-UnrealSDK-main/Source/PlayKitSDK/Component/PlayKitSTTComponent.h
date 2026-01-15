// Copyright PlayKit. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "AudioCaptureComponent.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Sound/SoundSubmix.h"
#include "Tool/PlayKitTool.h"
#include "PlayKitSTTComponent.generated.h"

USTRUCT(BlueprintType)
struct FPlayKitTranscriptionRequest
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString model;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString language;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString prompt;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float temperature = 1.0f;
};

USTRUCT(BlueprintType)
struct FPlayKitTranscriptionResponse
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString text;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString language;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float durationInSeconds;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPlayKitTranscriptionRespondedDelegate, FPlayKitTranscriptionResponse, Response);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPlayKitTranscriptionErrorDelegate, FString, Error, FString, Code);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PLAYKITSDK_API UPlayKitSTTComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayKitSTTComponent();

protected:
	virtual void BeginPlay() override;

public:
	UFUNCTION(BlueprintCallable, Category="PlayKit|STT")
	void StartRecording();

	UFUNCTION(BlueprintCallable, Category="PlayKit|STT")
	void StopRecording();

	/** Start transcription with custom request options */
	UFUNCTION(BlueprintCallable, Category="PlayKit|STT")
	void StartTranscription(const FPlayKitTranscriptionRequest& Request);

	/** Start transcription with default options (uses component's ModelName) */
	UFUNCTION(BlueprintCallable, Category="PlayKit|STT")
	void StartTranscriptionSimple();

	UFUNCTION(BlueprintPure, Category="PlayKit|STT")
	FString GetLastSavedFilePath() const { return LastSavedFilePath; }

public:
	UPROPERTY(BlueprintAssignable, Category = "PlayKit|STT|Delegate")
	FPlayKitTranscriptionRespondedDelegate OnPlayKitTranscriptionResponded;

	UPROPERTY(BlueprintAssignable, Category = "PlayKit|STT|Delegate")
	FPlayKitTranscriptionErrorDelegate OnPlayKitTranscriptionError;

public:
	//========== Configuration Properties (Edit in Details Panel) ==========//

	/** The AI model to use for transcription */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayKit|STT")
	FString ModelName;

private:
	UAudioCaptureComponent* CaptureComponent;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayKit|STT", meta=(AllowPrivateAccess="true"))
	USoundSubmix* RecordingSubmix = nullptr;

	FString LastSavedFilePath;

	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> CurrentHttpRequest;

	FString GetTranscriptionUrl() const;
	FString GetAuthToken() const;
	void UploadRecordingJson(const FPlayKitTranscriptionRequest& Request);
	void HandleTranscriptionResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
};
