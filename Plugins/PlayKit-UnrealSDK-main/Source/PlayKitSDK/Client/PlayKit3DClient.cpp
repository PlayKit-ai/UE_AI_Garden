// Copyright PlayKit. All Rights Reserved.

#include "PlayKit3DClient.h"
#include "PlayKitSettings.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "TimerManager.h"

UPlayKit3DClient::UPlayKit3DClient()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPlayKit3DClient::BeginPlay()
{
	Super::BeginPlay();

	// Load default model from settings if not set
	if (ModelName.IsEmpty())
	{
		UPlayKitSettings* Settings = UPlayKitSettings::Get();
		if (Settings && !Settings->Default3DModel.IsEmpty())
		{
			ModelName = Settings->Default3DModel;
		}
		else
		{
			ModelName = TEXT("default-3d-model");
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[PlayKit] 3DClient initialized with model: %s"), *ModelName);
}

void UPlayKit3DClient::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Critical: Clean up timers and HTTP requests
	StopPolling();
	if (CurrentRequest.IsValid())
	{
		CurrentRequest->CancelRequest();
		CurrentRequest.Reset();
	}

	Super::EndPlay(EndPlayReason);
}

//========== Public Interface Methods ==========//

void UPlayKit3DClient::Generate3D(const FString& Prompt)
{
	FPlayKit3DConfig Config;
	Config.Prompt = Prompt;
	Config.bTexture = true;
	Config.bPBR = bDefaultPBR;
	Config.TextureQuality = DefaultTextureQuality;
	Config.GeometryQuality = DefaultGeometryQuality;
	Config.FaceLimit = DefaultFaceLimit;
	Config.bAutoSize = bDefaultAutoSize;
	Config.ModelVersion = ModelVersion;

	Generate3DAdvanced(Config);
}

void UPlayKit3DClient::Generate3DWithNegative(const FString& Prompt, const FString& NegativePrompt)
{
	FPlayKit3DConfig Config;
	Config.Prompt = Prompt;
	Config.NegativePrompt = NegativePrompt;
	Config.bTexture = true;
	Config.bPBR = bDefaultPBR;
	Config.TextureQuality = DefaultTextureQuality;
	Config.GeometryQuality = DefaultGeometryQuality;
	Config.FaceLimit = DefaultFaceLimit;
	Config.bAutoSize = bDefaultAutoSize;
	Config.ModelVersion = ModelVersion;

	Generate3DAdvanced(Config);
}

void UPlayKit3DClient::Generate3DAdvanced(const FPlayKit3DConfig& Config)
{
	if (bIsProcessing)
	{
		BroadcastError(TEXT("REQUEST_IN_PROGRESS"), TEXT("A generation task is already in progress"));
		return;
	}

	if (Config.Prompt.IsEmpty())
	{
		BroadcastError(TEXT("INVALID_PROMPT"), TEXT("Prompt cannot be empty"));
		return;
	}

	CreateTask(Config);
}

void UPlayKit3DClient::CancelTask()
{
	if (CurrentRequest.IsValid())
	{
		CurrentRequest->CancelRequest();
		CurrentRequest.Reset();
	}

	StopPolling();
	CleanupCurrentTask();

	UE_LOG(LogTemp, Log, TEXT("[PlayKit] 3D generation task cancelled"));
}

void UPlayKit3DClient::QueryTaskStatus(const FString& TaskId)
{
	if (TaskId.IsEmpty())
	{
		BroadcastError(TEXT("INVALID_TASK_ID"), TEXT("Task ID is empty"));
		return;
	}

	// Temporarily set as current task for polling
	CurrentTaskId = TaskId;
	PollTaskStatus();
}

//========== Task Creation ==========//

void UPlayKit3DClient::CreateTask(const FPlayKit3DConfig& Config)
{
	UPlayKitSettings* Settings = UPlayKitSettings::Get();
	if (!Settings)
	{
		BroadcastError(TEXT("CONFIG_ERROR"), TEXT("Settings not found"));
		return;
	}

	FString Url = BuildCreateUrl();
	if (Url.IsEmpty())
	{
		BroadcastError(TEXT("CONFIG_ERROR"), TEXT("Failed to build request URL"));
		return;
	}

	bIsProcessing = true;

	// Build request body
	TSharedPtr<FJsonObject> RequestBody = MakeShared<FJsonObject>();
	RequestBody->SetStringField(TEXT("model"), ModelName);
	RequestBody->SetStringField(TEXT("prompt"), Config.Prompt);

	if (!Config.NegativePrompt.IsEmpty())
	{
		RequestBody->SetStringField(TEXT("negative_prompt"), Config.NegativePrompt);
	}

	if (!Config.ModelVersion.IsEmpty())
	{
		RequestBody->SetStringField(TEXT("model_version"), Config.ModelVersion);
	}

	RequestBody->SetBoolField(TEXT("texture"), Config.bTexture);
	RequestBody->SetBoolField(TEXT("pbr"), Config.bPBR);
	RequestBody->SetStringField(TEXT("texture_quality"), QualityToString(Config.TextureQuality));
	RequestBody->SetStringField(TEXT("geometry_quality"), QualityToString(Config.GeometryQuality));

	if (Config.TextureSeed >= 0)
	{
		RequestBody->SetNumberField(TEXT("texture_seed"), Config.TextureSeed);
	}

	if (Config.FaceLimit > 0)
	{
		RequestBody->SetNumberField(TEXT("face_limit"), Config.FaceLimit);
	}

	RequestBody->SetBoolField(TEXT("auto_size"), Config.bAutoSize);
	RequestBody->SetBoolField(TEXT("quad"), Config.bQuad);
	RequestBody->SetBoolField(TEXT("smart_low_poly"), Config.bSmartLowPoly);

	// Serialize
	FString RequestBodyStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBodyStr);
	FJsonSerializer::Serialize(RequestBody.ToSharedRef(), Writer);

	CurrentRequest = CreateAuthenticatedRequest(Url);
	CurrentRequest->SetContentAsString(RequestBodyStr);
	CurrentRequest->OnProcessRequestComplete().BindUObject(this, &UPlayKit3DClient::HandleCreateTaskResponse);

	UE_LOG(LogTemp, Log, TEXT("[PlayKit] Creating 3D generation task: %s"), *Url);
	CurrentRequest->ProcessRequest();
}

void UPlayKit3DClient::HandleCreateTaskResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	CurrentRequest.Reset();

	if (!bWasSuccessful || !Response.IsValid())
	{
		CleanupCurrentTask();
		BroadcastError(TEXT("NETWORK_ERROR"), TEXT("Network request failed"));
		return;
	}

	int32 ResponseCode = Response->GetResponseCode();
	FString ResponseContent = Response->GetContentAsString();

	if (ResponseCode != 201)
	{
		CleanupCurrentTask();
		UE_LOG(LogTemp, Error, TEXT("[PlayKit] 3D create error %d: %s"), ResponseCode, *ResponseContent);
		BroadcastError(FString::FromInt(ResponseCode), ResponseContent);
		return;
	}

	// Parse response
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		CleanupCurrentTask();
		BroadcastError(TEXT("PARSE_ERROR"), TEXT("Failed to parse response"));
		return;
	}

	// Extract task info
	JsonObject->TryGetStringField(TEXT("task_id"), CurrentTaskId);

	FString StatusStr;
	if (JsonObject->TryGetStringField(TEXT("status"), StatusStr))
	{
		CurrentStatus = ParseStatus(StatusStr);
	}

	JsonObject->TryGetNumberField(TEXT("progress"), CurrentProgress);
	JsonObject->TryGetNumberField(TEXT("poll_interval"), PollIntervalSeconds);

	if (CurrentTaskId.IsEmpty())
	{
		CleanupCurrentTask();
		BroadcastError(TEXT("INVALID_RESPONSE"), TEXT("No task_id in response"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[PlayKit] 3D task created: %s, status: %s, poll_interval: %d"),
		*CurrentTaskId, *StatusStr, PollIntervalSeconds);

	// Broadcast initial progress
	OnProgress.Broadcast(CurrentTaskId, CurrentProgress);

	// Start automatic polling
	StartPolling(PollIntervalSeconds);
}

//========== Polling Management ==========//

void UPlayKit3DClient::StartPolling(int32 IntervalSeconds)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[PlayKit] Cannot start polling - World is null"));
		return;
	}

	// Ensure any existing timer is cleared
	StopPolling();

	// Set up repeating timer
	World->GetTimerManager().SetTimer(
		PollTimerHandle,
		this,
		&UPlayKit3DClient::OnPollTimer,
		static_cast<float>(IntervalSeconds),
		true  // Loop
	);

	UE_LOG(LogTemp, Log, TEXT("[PlayKit] Started polling task %s every %d seconds"),
		*CurrentTaskId, IntervalSeconds);
}

void UPlayKit3DClient::StopPolling()
{
	UWorld* World = GetWorld();
	if (World && PollTimerHandle.IsValid())
	{
		World->GetTimerManager().ClearTimer(PollTimerHandle);
		PollTimerHandle.Invalidate();
		UE_LOG(LogTemp, Log, TEXT("[PlayKit] Stopped polling"));
	}
}

void UPlayKit3DClient::OnPollTimer()
{
	// This is called by the timer - invoke poll
	PollTaskStatus();
}

void UPlayKit3DClient::PollTaskStatus()
{
	if (CurrentTaskId.IsEmpty())
	{
		StopPolling();
		return;
	}

	FString Url = BuildPollUrl(CurrentTaskId);

	CurrentRequest = CreateAuthenticatedRequest(Url);
	CurrentRequest->SetVerb(TEXT("GET"));  // Override to GET for polling
	CurrentRequest->OnProcessRequestComplete().BindUObject(this, &UPlayKit3DClient::HandlePollResponse);

	UE_LOG(LogTemp, Verbose, TEXT("[PlayKit] Polling task status: %s"), *Url);
	CurrentRequest->ProcessRequest();
}

void UPlayKit3DClient::HandlePollResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	CurrentRequest.Reset();

	if (!bWasSuccessful || !Response.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayKit] Poll request failed, will retry on next interval"));
		return; // Don't stop polling on network errors
	}

	int32 ResponseCode = Response->GetResponseCode();
	FString ResponseContent = Response->GetContentAsString();

	if (ResponseCode != 200)
	{
		StopPolling();
		CleanupCurrentTask();
		BroadcastError(FString::FromInt(ResponseCode), ResponseContent);
		return;
	}

	// Parse response
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayKit] Failed to parse poll response"));
		return;
	}

	// Track old status for change detection
	EPlayKit3DTaskStatus OldStatus = CurrentStatus;
	int32 OldProgress = CurrentProgress;

	// Parse task data
	FString StatusStr;
	if (JsonObject->TryGetStringField(TEXT("status"), StatusStr))
	{
		CurrentStatus = ParseStatus(StatusStr);
	}

	JsonObject->TryGetNumberField(TEXT("progress"), CurrentProgress);

	int32 NewPollInterval = PollIntervalSeconds;
	if (JsonObject->TryGetNumberField(TEXT("poll_interval"), NewPollInterval))
	{
		if (NewPollInterval != PollIntervalSeconds)
		{
			PollIntervalSeconds = NewPollInterval;
			// Restart timer with new interval
			StartPolling(PollIntervalSeconds);
		}
	}

	// Broadcast status change if changed
	if (CurrentStatus != OldStatus)
	{
		UE_LOG(LogTemp, Log, TEXT("[PlayKit] Task %s status changed: %d -> %d"),
			*CurrentTaskId, (int32)OldStatus, (int32)CurrentStatus);
		OnStatusChanged.Broadcast(CurrentTaskId, OldStatus, CurrentStatus);
	}

	// Broadcast progress if changed
	if (CurrentProgress != OldProgress)
	{
		UE_LOG(LogTemp, Log, TEXT("[PlayKit] Task %s progress: %d%%"), *CurrentTaskId, CurrentProgress);
		OnProgress.Broadcast(CurrentTaskId, CurrentProgress);
	}

	// Handle terminal states
	if (CurrentStatus == EPlayKit3DTaskStatus::Success)
	{
		StopPolling();

		FPlayKit3DResponse Result;
		Result.bSuccess = true;
		Result.Task.TaskId = CurrentTaskId;
		Result.Task.Status = CurrentStatus;
		Result.Task.Progress = 100;

		JsonObject->TryGetNumberField(TEXT("created_at"), Result.Task.CreatedAt);
		JsonObject->TryGetNumberField(TEXT("completed_at"), Result.Task.CompletedAt);

		// Parse output
		const TSharedPtr<FJsonObject>* OutputPtr;
		if (JsonObject->TryGetObjectField(TEXT("output"), OutputPtr) && OutputPtr)
		{
			(*OutputPtr)->TryGetStringField(TEXT("model"), Result.Task.Output.ModelUrl);
			(*OutputPtr)->TryGetStringField(TEXT("pbr_model"), Result.Task.Output.PBRModelUrl);
			(*OutputPtr)->TryGetStringField(TEXT("rendered_image"), Result.Task.Output.RenderedImageUrl);
			Result.Task.Output.GeneratedAt = FDateTime::UtcNow();

			// Log warning about URL expiration
			UE_LOG(LogTemp, Warning, TEXT("[PlayKit] Model URLs will expire in 5 minutes! Download immediately."));
			UE_LOG(LogTemp, Log, TEXT("[PlayKit] Model URL: %s"), *Result.Task.Output.ModelUrl);
			if (!Result.Task.Output.PBRModelUrl.IsEmpty())
			{
				UE_LOG(LogTemp, Log, TEXT("[PlayKit] PBR Model URL: %s"), *Result.Task.Output.PBRModelUrl);
			}
		}

		CleanupCurrentTask();
		OnCompleted.Broadcast(Result);
	}
	else if (CurrentStatus == EPlayKit3DTaskStatus::Failed ||
			 CurrentStatus == EPlayKit3DTaskStatus::Banned ||
			 CurrentStatus == EPlayKit3DTaskStatus::Expired)
	{
		StopPolling();

		FString ErrorCode = TEXT("GENERATION_FAILED");
		FString ErrorMessage = StatusStr;

		// Try to extract detailed error
		const TSharedPtr<FJsonObject>* ErrorPtr;
		if (JsonObject->TryGetObjectField(TEXT("error"), ErrorPtr) && ErrorPtr)
		{
			(*ErrorPtr)->TryGetStringField(TEXT("code"), ErrorCode);
			(*ErrorPtr)->TryGetStringField(TEXT("message"), ErrorMessage);
		}

		CleanupCurrentTask();
		BroadcastError(ErrorCode, ErrorMessage);
	}
	// If still queued/running, polling continues automatically
}

//========== Utility Methods ==========//

TSharedRef<IHttpRequest, ESPMode::ThreadSafe> UPlayKit3DClient::CreateAuthenticatedRequest(const FString& Url)
{
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	UPlayKitSettings* Settings = UPlayKitSettings::Get();
	if (Settings)
	{
		FString Token = Settings->HasDeveloperToken() && !Settings->bIgnoreDeveloperToken
			? Settings->GetDeveloperToken()
			: Settings->GetPlayerToken();
		if (!Token.IsEmpty())
		{
			Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *Token));
		}
	}

	return Request;
}

FString UPlayKit3DClient::BuildCreateUrl() const
{
	UPlayKitSettings* Settings = UPlayKitSettings::Get();
	if (!Settings) return TEXT("");

	return FString::Printf(TEXT("%s/ai/%s/v2/3d"),
		*Settings->GetBaseUrl(),
		*Settings->GameId);
}

FString UPlayKit3DClient::BuildPollUrl(const FString& TaskId) const
{
	UPlayKitSettings* Settings = UPlayKitSettings::Get();
	if (!Settings) return TEXT("");

	return FString::Printf(TEXT("%s/ai/%s/v2/3d/%s"),
		*Settings->GetBaseUrl(),
		*Settings->GameId,
		*TaskId);
}

EPlayKit3DTaskStatus UPlayKit3DClient::ParseStatus(const FString& StatusString) const
{
	if (StatusString == TEXT("queued")) return EPlayKit3DTaskStatus::Queued;
	if (StatusString == TEXT("running")) return EPlayKit3DTaskStatus::Running;
	if (StatusString == TEXT("success")) return EPlayKit3DTaskStatus::Success;
	if (StatusString == TEXT("failed")) return EPlayKit3DTaskStatus::Failed;
	if (StatusString == TEXT("banned")) return EPlayKit3DTaskStatus::Banned;
	if (StatusString == TEXT("expired")) return EPlayKit3DTaskStatus::Expired;
	return EPlayKit3DTaskStatus::Unknown;
}

FString UPlayKit3DClient::QualityToString(EPlayKit3DQuality Quality) const
{
	return Quality == EPlayKit3DQuality::Detailed ? TEXT("detailed") : TEXT("standard");
}

void UPlayKit3DClient::BroadcastError(const FString& ErrorCode, const FString& ErrorMessage)
{
	UE_LOG(LogTemp, Error, TEXT("[PlayKit] 3D error [%s]: %s"), *ErrorCode, *ErrorMessage);
	OnError.Broadcast(ErrorCode, ErrorMessage);
}

void UPlayKit3DClient::CleanupCurrentTask()
{
	bIsProcessing = false;
	CurrentTaskId.Empty();
	CurrentStatus = EPlayKit3DTaskStatus::Unknown;
	CurrentProgress = 0;
}
