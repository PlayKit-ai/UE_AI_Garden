// Copyright PlayKit. All Rights Reserved.

#include "PlayKitSTTComponent.h"
#include "PlayKitSettings.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Sound/SoundWave.h"
#include "Misc/Base64.h"
#include "Containers/StringConv.h"

FString UPlayKitSTTComponent::GetTranscriptionUrl() const
{
	UPlayKitSettings* Settings = UPlayKitSettings::Get();
	if (Settings)
	{
		return FString::Printf(TEXT("%s/ai/%s/v2/audio/transcriptions"), *Settings->GetBaseUrl(), *Settings->GameId);
	}
	return TEXT("https://playkit.ai/ai/v2/audio/transcriptions");
}

FString UPlayKitSTTComponent::GetAuthToken() const
{
	UPlayKitSettings* Settings = UPlayKitSettings::Get();
	if (Settings)
	{
		if (Settings->HasDeveloperToken() && !Settings->bIgnoreDeveloperToken)
		{
			return Settings->GetDeveloperToken();
		}
		return Settings->GetPlayerToken();
	}
	return TEXT("");
}

UPlayKitSTTComponent::UPlayKitSTTComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	CaptureComponent = CreateDefaultSubobject<UAudioCaptureComponent>(TEXT("AudioCaptureComponent"));
	UE_LOG(LogTemp, Log, TEXT("[STT] Constructor: AudioCaptureComponent created"));
	if (CaptureComponent)
	{
		CaptureComponent->bAutoActivate = false;
		UE_LOG(LogTemp, Log, TEXT("[STT] AudioCaptureComponent auto-activate disabled"));
		CaptureComponent->SoundSubmix = RecordingSubmix;
		UE_LOG(LogTemp, Log, TEXT("[STT] AudioCaptureComponent submix set: %s"), RecordingSubmix ? *RecordingSubmix->GetName() : TEXT("null"));
	}
}

void UPlayKitSTTComponent::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogTemp, Log, TEXT("[STT] BeginPlay"));
}

void UPlayKitSTTComponent::StartRecording()
{
	if (GetWorld())
	{
		UE_LOG(LogTemp, Log, TEXT("[STT] StartRecording: World=%s"), *GetWorld()->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[STT] StartRecording: World is null"));
	}
	if (!RecordingSubmix)
	{
		UE_LOG(LogTemp, Warning, TEXT("[STT] StartRecording: RecordingSubmix is not set"));
		OnPlayKitTranscriptionError.Broadcast(TEXT("Recording submix not configured"), TEXT("SUBMIX_NOT_SET"));
	}
	if (CaptureComponent)
	{
		CaptureComponent->SoundSubmix = RecordingSubmix;
		CaptureComponent->Activate(true);
		UE_LOG(LogTemp, Log, TEXT("[STT] AudioCaptureComponent activated: %s"), CaptureComponent->IsActive() ? TEXT("true") : TEXT("false"));
	}
	UAudioMixerBlueprintLibrary::StartRecordingOutput(this, 600.0f, RecordingSubmix);
	UE_LOG(LogTemp, Log, TEXT("[STT] StartRecordingOutput invoked"));
}

void UPlayKitSTTComponent::StopRecording()
{
	UE_LOG(LogTemp, Log, TEXT("[STT] StopRecording called"));
	FString SaveDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CaptureSound"));
	SaveDir = FPaths::ConvertRelativePathToFull(SaveDir);
	const bool bDirOk = IFileManager::Get().MakeDirectory(*SaveDir, true);
	UE_LOG(LogTemp, Log, TEXT("[STT] Ensure SaveDir: %s (created=%s)"), *SaveDir, bDirOk ? TEXT("true") : TEXT("false"));
	const FString Timestamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
	const FString FileNameNoExt = FString::Printf(TEXT("capture_%s"), *Timestamp);

	if (CaptureComponent)
	{
		CaptureComponent->Deactivate();
		UE_LOG(LogTemp, Log, TEXT("[STT] AudioCaptureComponent deactivated: %s"), CaptureComponent->IsActive() ? TEXT("true") : TEXT("false"));
	}

	UAudioMixerBlueprintLibrary::StopRecordingOutput(this, EAudioRecordingExportType::WavFile, FileNameNoExt, SaveDir, RecordingSubmix);
	LastSavedFilePath = FPaths::ConvertRelativePathToFull(FPaths::Combine(SaveDir, FileNameNoExt + TEXT(".wav")));
	bool bFileExists = IFileManager::Get().FileExists(*LastSavedFilePath);
	if (!bFileExists)
	{
		for (int i=0; i<20 && !bFileExists; i++)
		{
			FPlatformProcess::Sleep(0.05f);
			bFileExists = IFileManager::Get().FileExists(*LastSavedFilePath);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[STT] Recording saved: %s (exists=%s)"), *LastSavedFilePath, bFileExists ? TEXT("true") : TEXT("false"));
}

void UPlayKitSTTComponent::StartTranscription(const FString& AuthToken, const FPlayKitTranscriptionRequest& Request)
{
	UE_LOG(LogTemp, Log, TEXT("[STT] StartTranscription called, LastSavedFilePath=%s"), *LastSavedFilePath);
	if (LastSavedFilePath.IsEmpty() || !IFileManager::Get().FileExists(*LastSavedFilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("[STT] StartTranscription aborted: recording file not ready"));
		OnPlayKitTranscriptionError.Broadcast(TEXT("Recording file not ready"), TEXT("FILE_NOT_READY"));
		return;
	}
	UploadRecordingJson(AuthToken, Request);
}

void UPlayKitSTTComponent::UploadRecordingBinary(const FString& AuthToken)
{
	if (LastSavedFilePath.IsEmpty())
	{
		OnPlayKitTranscriptionError.Broadcast(TEXT("No recording file"), TEXT("NO_FILE"));
		UE_LOG(LogTemp, Error, TEXT("[STT] UploadRecordingBinary: LastSavedFilePath is empty"));
		return;
	}

	TArray<uint8> FileData;
	if (!FFileHelper::LoadFileToArray(FileData, *LastSavedFilePath))
	{
		OnPlayKitTranscriptionError.Broadcast(TEXT("Load file failed"), TEXT("LOAD_FAILED"));
		UE_LOG(LogTemp, Error, TEXT("[STT] UploadRecordingBinary: Load file failed: %s"), *LastSavedFilePath);
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("[STT] UploadRecordingBinary: Read %d bytes from %s"), FileData.Num(), *LastSavedFilePath);
	{
		FString B64 = FBase64::Encode(FileData);
		const int32 MaxLogLen = 8192;
		const FString Head = B64.Left(MaxLogLen);
		UE_LOG(LogTemp, Log, TEXT("[STT] WAV base64 length=%d"), B64.Len());
		UE_LOG(LogTemp, Log, TEXT("[STT] WAV base64 (head %d): %s"), MaxLogLen, *Head);
	}

	const FString Url = GetTranscriptionUrl();
	CurrentHttpRequest = FHttpModule::Get().CreateRequest();
	CurrentHttpRequest->SetURL(Url);
	CurrentHttpRequest->SetVerb(TEXT("POST"));

	const FString Authorization = "Bearer " + AuthToken;
	CurrentHttpRequest->SetHeader(TEXT("Content-Type"), TEXT("audio/wav"));
	CurrentHttpRequest->SetHeader(TEXT("Authorization"), Authorization);

	CurrentHttpRequest->SetContent(FileData);

	CurrentHttpRequest->OnProcessRequestComplete().BindUObject(this, &UPlayKitSTTComponent::HandleTranscriptionResponse);
	UE_LOG(LogTemp, Log, TEXT("[STT] UploadRecordingBinary: Request sent to %s"), *Url);
	CurrentHttpRequest->ProcessRequest();
}

void UPlayKitSTTComponent::UploadRecordingJson(const FString& AuthToken, const FPlayKitTranscriptionRequest& Request)
{
	if (LastSavedFilePath.IsEmpty())
	{
		OnPlayKitTranscriptionError.Broadcast(TEXT("No recording file"), TEXT("NO_FILE"));
		UE_LOG(LogTemp, Error, TEXT("[STT] UploadRecordingJson: LastSavedFilePath is empty"));
		return;
	}
	TArray<uint8> FileData;
	if (!FFileHelper::LoadFileToArray(FileData, *LastSavedFilePath))
	{
		OnPlayKitTranscriptionError.Broadcast(TEXT("Load file failed"), TEXT("LOAD_FAILED"));
		UE_LOG(LogTemp, Error, TEXT("[STT] UploadRecordingJson: Load file failed: %s"), *LastSavedFilePath);
		return;
	}
	FString Model = Request.model.IsEmpty() ? TEXT("whisper-large") : Request.model;
	FString Lang = Request.language.IsEmpty() ? TEXT("en") : Request.language;
	FString Prompt = Request.prompt;
	FString AudioB64 = FBase64::Encode(FileData);
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
	JsonObject->SetStringField(TEXT("model"), Model);
	JsonObject->SetStringField(TEXT("audio"), AudioB64);
	JsonObject->SetStringField(TEXT("language"), Lang);
	JsonObject->SetStringField(TEXT("prompt"), Prompt);
	const FString JsonString = UPlayKitTool::JsonObjectToString(JsonObject);
	UE_LOG(LogTemp, Log, TEXT("[STT] Request JSON:\n%s"), *UPlayKitTool::JsonObjectToString(JsonObject, true));
	const FString Url = GetTranscriptionUrl();
	CurrentHttpRequest = FHttpModule::Get().CreateRequest();
	CurrentHttpRequest->SetURL(Url);
	CurrentHttpRequest->SetVerb(TEXT("POST"));
	const FString Authorization = "Bearer " + AuthToken;
	CurrentHttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	CurrentHttpRequest->SetHeader(TEXT("Authorization"), Authorization);
	CurrentHttpRequest->SetContentAsString(JsonString);
	CurrentHttpRequest->OnProcessRequestComplete().BindUObject(this, &UPlayKitSTTComponent::HandleTranscriptionResponse);
	UE_LOG(LogTemp, Log, TEXT("[STT] UploadRecordingJson: Request sent to %s"), *Url);
	CurrentHttpRequest->ProcessRequest();
}

void UPlayKitSTTComponent::HandleTranscriptionResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if(!Response.IsValid() || !Request.IsValid())
	{
		OnPlayKitTranscriptionError.Broadcast(TEXT("Request failed"), TEXT("REQUEST_FAILED"));
		UE_LOG(LogTemp, Error, TEXT("[STT] HandleTranscriptionResponse: Invalid response/request"));
		return;
	}

	const int Code = Response->GetResponseCode();
	UE_LOG(LogTemp, Log, TEXT("[STT] HandleTranscriptionResponse: HTTP %d"), Code);
	if(Code != 200)
	{
		if (Code == 400)
		{
			TSharedPtr<FJsonObject> ErrObj;
			if (UPlayKitTool::StringToJsonObject(Response->GetContentAsString(), ErrObj, true))
			{
				FString ErrMsg;
				FString ErrCode;
				const TSharedPtr<FJsonValue> ErrorValue = ErrObj->TryGetField(TEXT("error"));
				if (ErrorValue.IsValid())
				{
					if (ErrorValue->Type == EJson::String)
					{
						ErrMsg = ErrorValue->AsString();
					}
					else if (ErrorValue->Type == EJson::Object)
					{
						TSharedPtr<FJsonObject> ErrorObj = ErrorValue->AsObject();
						if (ErrorObj.IsValid())
						{
							if (!ErrorObj->TryGetStringField(TEXT("message"), ErrMsg))
							{
								ErrMsg = UPlayKitTool::JsonObjectToString(ErrorObj, true);
							}
							ErrorObj->TryGetStringField(TEXT("code"), ErrCode);
						}
					}
				}
				if (!ErrObj->TryGetStringField(TEXT("code"), ErrCode))
				{
					if (ErrCode.IsEmpty())
					{
						ErrCode = TEXT("HTTP_400");
					}
				}
				if (ErrMsg.IsEmpty())
				{
					ErrMsg = UPlayKitTool::JsonObjectToString(ErrObj, true);
				}
				OnPlayKitTranscriptionError.Broadcast(ErrMsg, ErrCode);
				UE_LOG(LogTemp, Error, TEXT("[STT] HTTP 400 Error: %s (%s)"), *ErrMsg, *ErrCode);
			}
			else
			{
				OnPlayKitTranscriptionError.Broadcast(TEXT("Bad Request"), TEXT("HTTP_400"));
				UE_LOG(LogTemp, Error, TEXT("[STT] HTTP 400: Bad Request, body parse failed"));
			}
		}
		else
		{
			OnPlayKitTranscriptionError.Broadcast(FString::Printf(TEXT("HTTP %d: %s"), Code, *Response->GetContentAsString()), TEXT("HTTP_ERROR"));
			UE_LOG(LogTemp, Error, TEXT("[STT] HTTP Error %d: %s"), Code, *Response->GetContentAsString());
		}
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[STT] Response JSON: %s"), *Response->GetContentAsString());
	TSharedPtr<FJsonObject> JsonObject;
	UPlayKitTool::StringToJsonObject(Response->GetContentAsString(), JsonObject, true);

	FPlayKitTranscriptionResponse Transcription;
	Transcription.text = JsonObject->GetStringField(TEXT("text"));
	Transcription.language = JsonObject->GetStringField(TEXT("language"));
	Transcription.durationInSeconds = static_cast<float>(JsonObject->GetNumberField(TEXT("durationInSeconds")));

	OnPlayKitTranscriptionResponded.Broadcast(Transcription);
	UE_LOG(LogTemp, Log, TEXT("[STT] Transcription success: text=\"%s\", language=%s, duration=%.2fs"), *Transcription.text, *Transcription.language, Transcription.durationInSeconds);
}
