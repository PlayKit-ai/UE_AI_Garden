// Copyright PlayKit. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/IHttpRequest.h"
#include "PlayKitNPCClient.generated.h"

/**
 * NPC Message Structure
 */
USTRUCT(BlueprintType)
struct FNPCMessage
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Role; // "system", "user", "assistant"

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Content;

	FNPCMessage() {}
	FNPCMessage(const FString& InRole, const FString& InContent) : Role(InRole), Content(InContent) {}
};

/**
 * NPC Action Call Structure
 */
USTRUCT(BlueprintType)
struct FNPCActionCall
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString CallId;

	UPROPERTY(BlueprintReadOnly)
	FString ActionName;

	UPROPERTY(BlueprintReadOnly)
	TMap<FString, FString> Parameters;
};

/**
 * NPC Response Structure
 */
USTRUCT(BlueprintType)
struct FNPCResponse
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly)
	FString Content;

	UPROPERTY(BlueprintReadOnly)
	TArray<FNPCActionCall> ActionCalls;

	UPROPERTY(BlueprintReadOnly)
	FString ErrorMessage;
};

/**
 * Memory Entry Structure
 */
USTRUCT(BlueprintType)
struct FMemoryEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Content;
};

// Delegates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNPCResponse, FNPCResponse, Response);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNPCStreamChunk, FString, Chunk);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNPCStreamComplete, FString, FullContent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNPCActionTriggered, FNPCActionCall, ActionCall);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnReplyPredictionsGenerated, const TArray<FString>&, Predictions);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnNPCError, FString, ErrorCode, FString, ErrorMessage);

/**
 * PlayKit NPC Client Component
 * Manages NPC conversations with memory, actions, and history
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PLAYKITSDK_API UPlayKitNPCClient : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayKitNPCClient();

protected:
	virtual void BeginPlay() override;

public:
	//========== Initialization ==========//

	/** Setup the NPC client with the SDK. Called by UPlayKitBlueprintLibrary::SetupNPC */
	void Setup(const FString& ModelName = TEXT(""));

	//========== Configuration ==========//

	/** Set the player token for authenticated requests (optional - uses SDK token by default) */
	UFUNCTION(BlueprintCallable, Category="PlayKit|NPC")
	void SetPlayerToken(const FString& Token);

	/** Set the AI model to use */
	UFUNCTION(BlueprintCallable, Category="PlayKit|NPC")
	void SetModel(const FString& ModelName);

	/** Set the character design/personality */
	UFUNCTION(BlueprintCallable, Category="PlayKit|NPC")
	void SetCharacterDesign(const FString& Design);

	/** Get the current character design */
	UFUNCTION(BlueprintPure, Category="PlayKit|NPC")
	FString GetCharacterDesign() const { return CharacterDesign; }

	//========== Memory System ==========//

	/** Set a memory value */
	UFUNCTION(BlueprintCallable, Category="PlayKit|NPC|Memory")
	void SetMemory(const FString& MemoryName, const FString& MemoryContent);

	/** Get a memory value */
	UFUNCTION(BlueprintPure, Category="PlayKit|NPC|Memory")
	FString GetMemory(const FString& MemoryName) const;

	/** Get all memory names */
	UFUNCTION(BlueprintPure, Category="PlayKit|NPC|Memory")
	TArray<FString> GetMemoryNames() const;

	/** Clear all memories */
	UFUNCTION(BlueprintCallable, Category="PlayKit|NPC|Memory")
	void ClearMemories();

	//========== Conversation ==========//

	/** Send a message to the NPC and get a response */
	UFUNCTION(BlueprintCallable, Category="PlayKit|NPC")
	void Talk(const FString& Message);

	/** Send a message with streaming response */
	UFUNCTION(BlueprintCallable, Category="PlayKit|NPC")
	void TalkStream(const FString& Message);

	/** Check if the NPC is currently responding */
	UFUNCTION(BlueprintPure, Category="PlayKit|NPC")
	bool IsTalking() const { return bIsTalking; }

	//========== History Management ==========//

	/** Get the conversation history */
	UFUNCTION(BlueprintPure, Category="PlayKit|NPC|History")
	TArray<FNPCMessage> GetHistory() const { return ConversationHistory; }

	/** Get the number of messages in history */
	UFUNCTION(BlueprintPure, Category="PlayKit|NPC|History")
	int32 GetHistoryLength() const { return ConversationHistory.Num(); }

	/** Clear the conversation history */
	UFUNCTION(BlueprintCallable, Category="PlayKit|NPC|History")
	void ClearHistory();

	/** Revert the last exchange (user message + assistant response) */
	UFUNCTION(BlueprintCallable, Category="PlayKit|NPC|History")
	bool RevertHistory();

	/** Revert multiple messages from history */
	UFUNCTION(BlueprintCallable, Category="PlayKit|NPC|History")
	int32 RevertChatMessages(int32 Count);

	/** Manually append a message to history */
	UFUNCTION(BlueprintCallable, Category="PlayKit|NPC|History")
	void AppendChatMessage(const FString& Role, const FString& Content);

	/** Save history to JSON string */
	UFUNCTION(BlueprintCallable, Category="PlayKit|NPC|History")
	FString SaveHistory() const;

	/** Load history from JSON string */
	UFUNCTION(BlueprintCallable, Category="PlayKit|NPC|History")
	bool LoadHistory(const FString& SaveData);

	//========== Action Results ==========//

	/** Report the result of an action */
	UFUNCTION(BlueprintCallable, Category="PlayKit|NPC|Actions")
	void ReportActionResult(const FString& CallId, const FString& Result);

	/** Report multiple action results */
	UFUNCTION(BlueprintCallable, Category="PlayKit|NPC|Actions")
	void ReportActionResults(const TMap<FString, FString>& Results);

	//========== Reply Predictions ==========//

	/** Generate reply predictions for the player */
	UFUNCTION(BlueprintCallable, Category="PlayKit|NPC|Predictions")
	void GenerateReplyPredictions(int32 Count = 3);

public:
	//========== Events ==========//

	/** Fired when NPC responds */
	UPROPERTY(BlueprintAssignable, Category="PlayKit|NPC")
	FOnNPCResponse OnResponse;

	/** Fired for each chunk in streaming mode */
	UPROPERTY(BlueprintAssignable, Category="PlayKit|NPC")
	FOnNPCStreamChunk OnStreamChunk;

	/** Fired when streaming completes */
	UPROPERTY(BlueprintAssignable, Category="PlayKit|NPC")
	FOnNPCStreamComplete OnStreamComplete;

	/** Fired when NPC triggers an action */
	UPROPERTY(BlueprintAssignable, Category="PlayKit|NPC")
	FOnNPCActionTriggered OnActionTriggered;

	/** Fired when reply predictions are generated */
	UPROPERTY(BlueprintAssignable, Category="PlayKit|NPC")
	FOnReplyPredictionsGenerated OnReplyPredictionsGenerated;

	/** Fired on error */
	UPROPERTY(BlueprintAssignable, Category="PlayKit|NPC")
	FOnNPCError OnError;

public:
	//========== Configuration Properties ==========//

	/** Enable automatic reply prediction generation after each response */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayKit|NPC|Predictions")
	bool bAutoGenerateReplyPredictions = false;

	/** Number of predictions to generate */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayKit|NPC|Predictions", meta=(ClampMin="2", ClampMax="6"))
	int32 PredictionCount = 3;

	/** Temperature for response generation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayKit|NPC", meta=(ClampMin="0.0", ClampMax="2.0"))
	float Temperature = 0.7f;

private:
	// Internal methods
	void SendChatRequest(bool bStream);
	void HandleChatResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void HandleStreamProgress(FHttpRequestPtr Request, uint64 BytesSent, uint64 BytesReceived);
	void HandlePredictionsResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	FString BuildSystemPrompt() const;
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> CreateAuthenticatedRequest(const FString& Url);
	void ParseActionCalls(const TSharedPtr<FJsonObject>& JsonObject, TArray<FNPCActionCall>& OutActionCalls);

	// Reply prediction helpers
	TArray<FString> ParsePredictionsFromJson(const FString& Response);
	TArray<FString> ExtractPredictionsFromText(const FString& Response, int32 ExpectedCount);
	FString BuildRecentHistoryString() const;
	FString GetLastNPCMessage() const;

	// Settings helpers
	FString GetBaseUrl() const;
	FString GetGameId() const;
	FString GetAuthToken() const;

private:
	// Configuration
	FString PlayerToken;  // Optional override, uses SDK token if empty
	FString Model;        // AI model, loaded from Settings if not set
	FString CharacterDesign;
	bool bIsSetup = false;

	// State
	bool bIsTalking = false;
	FString PendingUserMessage;
	FString StreamBuffer;

	// Memory
	TMap<FString, FString> Memories;

	// History
	TArray<FNPCMessage> ConversationHistory;

	// Pending action results
	TMap<FString, FString> PendingActionResults;

	// HTTP
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> CurrentRequest;
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> PredictionsRequest;
};
