// Copyright PlayKit. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/IHttpRequest.h"
#include "PlayKitTypes.h"
#include "PlayKitChatClient.generated.h"

/**
 * PlayKit Chat Client Component
 * Provides AI text generation and chat functionality.
 *
 * Features:
 * - Text generation (non-streaming)
 * - Streaming text generation
 * - Structured output generation
 * - Tool calling support
 *
 * Usage:
 * 1. Add this component to any Actor in the editor
 * 2. Configure properties in the Details panel (ModelName, Temperature, etc.)
 * 3. Bind to events using the "+" button (OnChatResponse, OnStreamChunk, etc.)
 * 4. Call GenerateText() or GenerateTextStream() to start generation
 */
UCLASS(ClassGroup=(PlayKit), meta=(BlueprintSpawnableComponent))
class PLAYKITSDK_API UPlayKitChatClient : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayKitChatClient();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	//========== Configuration Properties (Edit in Details Panel) ==========//

	/** The AI model to use for chat */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayKit|Chat")
	FString ModelName = TEXT("gpt-4o-mini");

	/** Temperature for response generation (0.0-2.0). Higher = more creative */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayKit|Chat", meta=(ClampMin="0.0", ClampMax="2.0"))
	float Temperature = 0.7f;

	/** Maximum tokens to generate (0 = no limit) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayKit|Chat", meta=(ClampMin="0"))
	int32 MaxTokens = 0;

	/** Default system prompt */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayKit|Chat", meta=(MultiLine=true))
	FString SystemPrompt;

	//========== Events (Click "+" to bind in Blueprint) ==========//

	/** Fired when chat response is received (non-streaming) */
	UPROPERTY(BlueprintAssignable, Category="PlayKit|Chat")
	FOnChatResponse OnChatResponse;

	/** Fired for each chunk in streaming mode */
	UPROPERTY(BlueprintAssignable, Category="PlayKit|Chat")
	FOnChatStreamChunk OnStreamChunk;

	/** Fired when streaming completes with full content */
	UPROPERTY(BlueprintAssignable, Category="PlayKit|Chat")
	FOnChatStreamComplete OnStreamComplete;

	/** Fired on error */
	UPROPERTY(BlueprintAssignable, Category="PlayKit|Chat")
	FOnChatError OnError;

	/** Delegate for structured output response */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStructuredResponse, bool, bSuccess, const FString&, JsonResult);

	UPROPERTY(BlueprintAssignable, Category="PlayKit|Chat|Structured")
	FOnStructuredResponse OnStructuredResponse;

	//========== Status ==========//

	/** Check if a request is currently in progress */
	UFUNCTION(BlueprintPure, Category="PlayKit|Chat")
	bool IsProcessing() const { return bIsProcessing; }

	//========== Configuration Methods ==========//

	/** Set the model name at runtime */
	UFUNCTION(BlueprintCallable, Category="PlayKit|Chat")
	void SetModelName(const FString& InModelName) { ModelName = InModelName; }

	/** Set the temperature at runtime */
	UFUNCTION(BlueprintCallable, Category="PlayKit|Chat")
	void SetTemperature(float InTemperature) { Temperature = FMath::Clamp(InTemperature, 0.0f, 2.0f); }

	//========== Text Generation ==========//

	/**
	 * Generate text from a simple prompt (non-streaming).
	 * Uses the component's configured ModelName and Temperature.
	 * @param Prompt The user's message
	 */
	UFUNCTION(BlueprintCallable, Category="PlayKit|Chat", meta=(DisplayName="Generate Text"))
	void GenerateText(const FString& Prompt);

	/**
	 * Generate text with full configuration (non-streaming).
	 * @param Config Chat configuration with messages and settings
	 */
	UFUNCTION(BlueprintCallable, Category="PlayKit|Chat", meta=(DisplayName="Generate Text (Advanced)"))
	void GenerateTextAdvanced(const FPlayKitChatConfig& Config);

	/**
	 * Generate text with streaming response.
	 * Each chunk fires OnStreamChunk, completion fires OnStreamComplete.
	 * @param Prompt The user's message
	 */
	UFUNCTION(BlueprintCallable, Category="PlayKit|Chat", meta=(DisplayName="Generate Text Stream"))
	void GenerateTextStream(const FString& Prompt);

	/**
	 * Generate text with streaming and full configuration.
	 * @param Config Chat configuration with messages and settings
	 */
	UFUNCTION(BlueprintCallable, Category="PlayKit|Chat", meta=(DisplayName="Generate Text Stream (Advanced)"))
	void GenerateTextStreamAdvanced(const FPlayKitChatConfig& Config);

	//========== Structured Output ==========//

	/**
	 * Generate a structured JSON object based on a schema.
	 * The response will be a valid JSON object matching your schema.
	 * @param Prompt The generation prompt
	 * @param SchemaJson JSON schema defining the output structure
	 */
	UFUNCTION(BlueprintCallable, Category="PlayKit|Chat|Structured", meta=(DisplayName="Generate Structured"))
	void GenerateStructured(const FString& Prompt, const FString& SchemaJson);

	//========== Cancel ==========//

	/** Cancel any in-progress request */
	UFUNCTION(BlueprintCallable, Category="PlayKit|Chat")
	void CancelRequest();

private:
	void SendChatRequest(const FPlayKitChatConfig& Config, bool bStream);
	void HandleChatResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void HandleStreamProgress(FHttpRequestPtr Request, uint64 BytesSent, uint64 BytesReceived);
	void HandleStreamComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void HandleStructuredResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	FString BuildRequestUrl() const;
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> CreateAuthenticatedRequest(const FString& Url);
	FPlayKitChatResponse ParseChatResponse(const FString& ResponseContent);
	void BroadcastError(const FString& ErrorCode, const FString& ErrorMessage);

private:
	bool bIsProcessing = false;

	// Streaming state
	FString StreamBuffer;
	FString AccumulatedContent;
	int32 LastProcessedOffset = 0;

	// Current request
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> CurrentRequest;
};
