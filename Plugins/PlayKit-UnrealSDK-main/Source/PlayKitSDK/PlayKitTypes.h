// Copyright PlayKit. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PlayKitTypes.generated.h"

//========== Chat Types ==========//

/**
 * Chat message for conversations
 */
USTRUCT(BlueprintType)
struct PLAYKITSDK_API FPlayKitChatMessage
{
	GENERATED_BODY()

	/** Message role: "system", "user", "assistant", or "tool" */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayKit")
	FString Role;

	/** Message content */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayKit")
	FString Content;

	/** Tool call ID (for tool responses) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayKit")
	FString ToolCallId;

	FPlayKitChatMessage() {}
	FPlayKitChatMessage(const FString& InRole, const FString& InContent)
		: Role(InRole), Content(InContent) {}
};

/**
 * Tool call from the AI
 */
USTRUCT(BlueprintType)
struct PLAYKITSDK_API FPlayKitToolCall
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	FString Id;

	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	FString Type;

	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	FString FunctionName;

	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	FString FunctionArguments;
};

/**
 * Chat completion response
 */
USTRUCT(BlueprintType)
struct PLAYKITSDK_API FPlayKitChatResponse
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	FString Content;

	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	FString FinishReason;

	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	TArray<FPlayKitToolCall> ToolCalls;

	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	FString ErrorMessage;

	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	int32 PromptTokens = 0;

	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	int32 CompletionTokens = 0;

	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	int32 TotalTokens = 0;
};

/**
 * Chat configuration
 */
USTRUCT(BlueprintType)
struct PLAYKITSDK_API FPlayKitChatConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayKit")
	TArray<FPlayKitChatMessage> Messages;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayKit", meta=(ClampMin="0.0", ClampMax="2.0"))
	float Temperature = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayKit")
	int32 MaxTokens = 0; // 0 = no limit
};

//========== Image Types ==========//

/**
 * Generated image result
 */
USTRUCT(BlueprintType)
struct PLAYKITSDK_API FPlayKitGeneratedImage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	bool bSuccess = false;

	/** Base64 encoded image data */
	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	FString ImageBase64;

	/** Original prompt used for generation */
	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	FString OriginalPrompt;

	/** Revised prompt (if modified by AI) */
	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	FString RevisedPrompt;

	/** When the image was generated */
	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	FDateTime GeneratedAt;

	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	FString ErrorMessage;
};

/**
 * Image generation options
 */
USTRUCT(BlueprintType)
struct PLAYKITSDK_API FPlayKitImageOptions
{
	GENERATED_BODY()

	/** Image size (e.g., "1024x1024", "1792x1024") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayKit")
	FString Size = TEXT("1024x1024");

	/** Number of images to generate (1-10) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayKit", meta=(ClampMin="1", ClampMax="10"))
	int32 Count = 1;

	/** Optional seed for reproducible results */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayKit")
	int32 Seed = -1; // -1 = no seed
};

//========== Transcription Types ==========//

/**
 * Transcription segment with timestamps
 */
USTRUCT(BlueprintType)
struct PLAYKITSDK_API FPlayKitTranscriptionSegment
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	float Start = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	float End = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	FString Text;
};

/**
 * Transcription result
 */
USTRUCT(BlueprintType)
struct PLAYKITSDK_API FPlayKitTranscriptionResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	FString Text;

	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	FString Language;

	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	float DurationSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	TArray<FPlayKitTranscriptionSegment> Segments;

	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	FString ErrorMessage;
};

//========== Player Types ==========//

/**
 * Player information
 */
USTRUCT(BlueprintType)
struct PLAYKITSDK_API FPlayKitPlayerInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	FString UserId;

	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	float Credits = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	FString Nickname;
};

/**
 * Daily credits refresh result
 */
USTRUCT(BlueprintType)
struct PLAYKITSDK_API FPlayKitDailyCreditsResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	bool bRefreshed = false;

	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	FString Message;

	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	float BalanceBefore = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	float BalanceAfter = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	float AmountAdded = 0.0f;
};

//========== Common Delegates ==========//

// Chat delegates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChatResponse, FPlayKitChatResponse, Response);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChatStreamChunk, const FString&, Chunk);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChatStreamComplete, const FString&, FullContent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnChatError, const FString&, ErrorCode, const FString&, ErrorMessage);

// Image delegates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnImageGenerated, FPlayKitGeneratedImage, Image);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnImagesGenerated, const TArray<FPlayKitGeneratedImage>&, Images);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnImageError, const FString&, ErrorCode, const FString&, ErrorMessage);

// Transcription delegates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTranscriptionComplete, FPlayKitTranscriptionResult, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTranscriptionError, const FString&, ErrorCode, const FString&, ErrorMessage);

// Player delegates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerInfoUpdated, FPlayKitPlayerInfo, PlayerInfo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerTokenReceived, const FString&, PlayerToken);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDailyCreditsRefreshed, FPlayKitDailyCreditsResult, Result);

//========== 3D Generation Types ==========//

/**
 * 3D generation task status
 */
UENUM(BlueprintType)
enum class EPlayKit3DTaskStatus : uint8
{
	Queued UMETA(DisplayName = "Queued"),
	Running UMETA(DisplayName = "Running"),
	Success UMETA(DisplayName = "Success"),
	Failed UMETA(DisplayName = "Failed"),
	Banned UMETA(DisplayName = "Banned"),
	Expired UMETA(DisplayName = "Expired"),
	Unknown UMETA(DisplayName = "Unknown")
};

/**
 * Quality levels for texture and geometry
 */
UENUM(BlueprintType)
enum class EPlayKit3DQuality : uint8
{
	Standard UMETA(DisplayName = "Standard"),
	Detailed UMETA(DisplayName = "Detailed")
};

/**
 * 3D model generation configuration
 */
USTRUCT(BlueprintType)
struct PLAYKITSDK_API FPlayKit3DConfig
{
	GENERATED_BODY()

	/** Text prompt describing the 3D model */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayKit")
	FString Prompt;

	/** Negative prompt (things to avoid) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayKit")
	FString NegativePrompt;

	/** Model version to use (e.g., "v2.5-20250123") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayKit")
	FString ModelVersion;

	/** Enable texture generation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayKit")
	bool bTexture = true;

	/** Enable PBR materials */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayKit")
	bool bPBR = true;

	/** Texture quality */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayKit")
	EPlayKit3DQuality TextureQuality = EPlayKit3DQuality::Standard;

	/** Geometry quality */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayKit")
	EPlayKit3DQuality GeometryQuality = EPlayKit3DQuality::Standard;

	/** Texture seed for reproducibility (-1 for random) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayKit")
	int32 TextureSeed = -1;

	/** Maximum face count (0 = use default) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayKit", meta=(ClampMin="0", ClampMax="200000"))
	int32 FaceLimit = 0;

	/** Enable automatic size normalization */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayKit")
	bool bAutoSize = false;

	/** Generate quad topology */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayKit")
	bool bQuad = false;

	/** Enable smart low-poly optimization */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayKit")
	bool bSmartLowPoly = false;
};

/**
 * 3D model generation output
 */
USTRUCT(BlueprintType)
struct PLAYKITSDK_API FPlayKit3DOutput
{
	GENERATED_BODY()

	/** URL to the generated 3D model file (GLB format, valid for 5 minutes) */
	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	FString ModelUrl;

	/** URL to the PBR model file (if PBR was enabled) */
	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	FString PBRModelUrl;

	/** URL to the rendered preview image */
	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	FString RenderedImageUrl;

	/** When the model was generated */
	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	FDateTime GeneratedAt;
};

/**
 * 3D generation task information
 */
USTRUCT(BlueprintType)
struct PLAYKITSDK_API FPlayKit3DTask
{
	GENERATED_BODY()

	/** Unique task ID */
	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	FString TaskId;

	/** Current task status */
	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	EPlayKit3DTaskStatus Status = EPlayKit3DTaskStatus::Unknown;

	/** Progress percentage (0-100) */
	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	int32 Progress = 0;

	/** Recommended poll interval in seconds */
	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	int32 PollInterval = 5;

	/** Task creation timestamp (Unix time) */
	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	int64 CreatedAt = 0;

	/** Task completion timestamp (Unix time, if completed) */
	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	int64 CompletedAt = 0;

	/** Output data (valid when Status == Success) */
	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	FPlayKit3DOutput Output;

	/** Error code (if failed) */
	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	FString ErrorCode;

	/** Error message (if failed) */
	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	FString ErrorMessage;
};

/**
 * Complete 3D generation response
 */
USTRUCT(BlueprintType)
struct PLAYKITSDK_API FPlayKit3DResponse
{
	GENERATED_BODY()

	/** Success flag */
	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	bool bSuccess = false;

	/** Task information */
	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	FPlayKit3DTask Task;

	/** Error message (if failed) */
	UPROPERTY(BlueprintReadOnly, Category="PlayKit")
	FString ErrorMessage;
};

// 3D Generation delegates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayKit3DProgress, const FString&, TaskId, int32, Progress);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnPlayKit3DStatusChanged, const FString&, TaskId, EPlayKit3DTaskStatus, OldStatus, EPlayKit3DTaskStatus, NewStatus);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayKit3DCompleted, FPlayKit3DResponse, Response);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayKit3DError, const FString&, ErrorCode, const FString&, ErrorMessage);
