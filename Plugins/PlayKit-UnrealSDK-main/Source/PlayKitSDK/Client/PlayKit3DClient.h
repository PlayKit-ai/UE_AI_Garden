// Copyright PlayKit. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/IHttpRequest.h"
#include "PlayKitTypes.h"
#include "PlayKit3DClient.generated.h"

/**
 * PlayKit 3D Model Client Component
 * Provides AI-powered 3D model generation functionality with automatic polling.
 *
 * Features:
 * - Text-to-3D model generation
 * - Automatic task polling with progress updates
 * - Status change notifications
 * - Configurable quality and optimization settings
 *
 * Important Notes:
 * - Model URLs expire after 5 minutes - download them immediately
 * - Task polling is automatic - no manual intervention needed
 * - Only one generation task can run at a time per component
 *
 * Usage:
 * 1. Add this component to any Actor in the editor
 * 2. Configure default properties in the Details panel (ModelName, DefaultQuality, etc.)
 * 3. Bind to events: OnCompleted, OnProgress, OnError
 * 4. Call Generate3D() with a prompt to start generation
 * 5. Wait for OnCompleted event and download model URLs immediately
 */
UCLASS(ClassGroup=(PlayKit), meta=(BlueprintSpawnableComponent))
class PLAYKITSDK_API UPlayKit3DClient : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayKit3DClient();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	//========== Configuration Properties (Edit in Details Panel) ==========//

	/** The AI model to use for 3D generation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayKit|3D")
	FString ModelName = TEXT("default-3d-model");

	/** Model version (leave empty for latest) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayKit|3D")
	FString ModelVersion = TEXT("v2.5-20250123");

	/** Default texture quality */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayKit|3D")
	EPlayKit3DQuality DefaultTextureQuality = EPlayKit3DQuality::Standard;

	/** Default geometry quality */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayKit|3D")
	EPlayKit3DQuality DefaultGeometryQuality = EPlayKit3DQuality::Standard;

	/** Enable PBR materials by default */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayKit|3D")
	bool bDefaultPBR = true;

	/** Default face limit (0 = use server default) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayKit|3D", meta=(ClampMin="0", ClampMax="200000"))
	int32 DefaultFaceLimit = 50000;

	/** Enable automatic size normalization */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayKit|3D")
	bool bDefaultAutoSize = false;

	//========== Events (Click "+" to bind in Blueprint) ==========//

	/** Fired when generation task completes successfully */
	UPROPERTY(BlueprintAssignable, Category="PlayKit|3D")
	FOnPlayKit3DCompleted OnCompleted;

	/** Fired periodically with task progress updates */
	UPROPERTY(BlueprintAssignable, Category="PlayKit|3D")
	FOnPlayKit3DProgress OnProgress;

	/** Fired when task status changes */
	UPROPERTY(BlueprintAssignable, Category="PlayKit|3D")
	FOnPlayKit3DStatusChanged OnStatusChanged;

	/** Fired on error */
	UPROPERTY(BlueprintAssignable, Category="PlayKit|3D")
	FOnPlayKit3DError OnError;

	//========== Status ==========//

	/** Check if a generation task is currently in progress */
	UFUNCTION(BlueprintPure, Category="PlayKit|3D")
	bool IsProcessing() const { return bIsProcessing; }

	/** Get the current task ID (empty if no task) */
	UFUNCTION(BlueprintPure, Category="PlayKit|3D")
	FString GetCurrentTaskId() const { return CurrentTaskId; }

	/** Get the current task status */
	UFUNCTION(BlueprintPure, Category="PlayKit|3D")
	EPlayKit3DTaskStatus GetCurrentStatus() const { return CurrentStatus; }

	/** Get the current progress (0-100) */
	UFUNCTION(BlueprintPure, Category="PlayKit|3D")
	int32 GetCurrentProgress() const { return CurrentProgress; }

	//========== 3D Model Generation ==========//

	/**
	 * Generate a 3D model from a text prompt.
	 * Uses component's default configuration (quality, PBR, etc.).
	 * Automatically polls for completion and fires events.
	 *
	 * @param Prompt Text description of the desired 3D model
	 */
	UFUNCTION(BlueprintCallable, Category="PlayKit|3D", meta=(DisplayName="Generate 3D Model"))
	void Generate3D(const FString& Prompt);

	/**
	 * Generate a 3D model with negative prompt.
	 *
	 * @param Prompt Text description of the desired 3D model
	 * @param NegativePrompt Things to avoid in the generation
	 */
	UFUNCTION(BlueprintCallable, Category="PlayKit|3D", meta=(DisplayName="Generate 3D Model (With Negative)"))
	void Generate3DWithNegative(const FString& Prompt, const FString& NegativePrompt);

	/**
	 * Generate a 3D model with full configuration.
	 *
	 * @param Config Complete generation configuration
	 */
	UFUNCTION(BlueprintCallable, Category="PlayKit|3D", meta=(DisplayName="Generate 3D Model (Advanced)"))
	void Generate3DAdvanced(const FPlayKit3DConfig& Config);

	/**
	 * Cancel the current generation task and stop polling.
	 */
	UFUNCTION(BlueprintCallable, Category="PlayKit|3D")
	void CancelTask();

	/**
	 * Manually query task status (normally automatic polling handles this).
	 *
	 * @param TaskId The task ID to query
	 */
	UFUNCTION(BlueprintCallable, Category="PlayKit|3D")
	void QueryTaskStatus(const FString& TaskId);

private:
	// HTTP request management
	void CreateTask(const FPlayKit3DConfig& Config);
	void PollTaskStatus();
	void HandleCreateTaskResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void HandlePollResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	// Polling management
	void StartPolling(int32 IntervalSeconds);
	void StopPolling();
	void OnPollTimer();

	// Parsing and utilities
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> CreateAuthenticatedRequest(const FString& Url);
	FString BuildCreateUrl() const;
	FString BuildPollUrl(const FString& TaskId) const;
	EPlayKit3DTaskStatus ParseStatus(const FString& StatusString) const;
	FString QualityToString(EPlayKit3DQuality Quality) const;
	void BroadcastError(const FString& ErrorCode, const FString& ErrorMessage);
	void CleanupCurrentTask();

private:
	// State tracking
	bool bIsProcessing = false;
	FString CurrentTaskId;
	EPlayKit3DTaskStatus CurrentStatus = EPlayKit3DTaskStatus::Unknown;
	int32 CurrentProgress = 0;

	// HTTP
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> CurrentRequest;

	// Polling timer
	FTimerHandle PollTimerHandle;
	int32 PollIntervalSeconds = 5;
};
