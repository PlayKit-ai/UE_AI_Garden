// Copyright PlayKit. All Rights Reserved.

#include "PlayKitSTTClient.h"
#include "PlayKitSettings.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"

UPlayKitSTTClient::UPlayKitSTTClient()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPlayKitSTTClient::BeginPlay()
{
	Super::BeginPlay();

	if (ModelName.IsEmpty())
	{
		ModelName = TEXT("whisper-large");
	}

	UE_LOG(LogTemp, Log, TEXT("[PlayKit] STTClient component initialized with model: %s"), *ModelName);
}

void UPlayKitSTTClient::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelRequest();
	Super::EndPlay(EndPlayReason);
}

TSharedRef<IHttpRequest, ESPMode::ThreadSafe> UPlayKitSTTClient::CreateAuthenticatedRequest(const FString& Url)
{
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(TEXT("POST"));

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

void UPlayKitSTTClient::TranscribeFile(const FString& FilePath)
{
	TranscribeFileWithLanguage(FilePath, Language);
}

void UPlayKitSTTClient::TranscribeFileWithLanguage(const FString& FilePath, const FString& InLanguage)
{
	if (bIsProcessing)
	{
		BroadcastError(TEXT("REQUEST_IN_PROGRESS"), TEXT("A request is already in progress"));
		return;
	}

	if (FilePath.IsEmpty())
	{
		BroadcastError(TEXT("INVALID_PATH"), TEXT("File path cannot be empty"));
		return;
	}

	TArray<uint8> FileData;
	if (!FFileHelper::LoadFileToArray(FileData, *FilePath))
	{
		BroadcastError(TEXT("FILE_ERROR"), FString::Printf(TEXT("Failed to load file: %s"), *FilePath));
		return;
	}

	FString FileName = FPaths::GetCleanFilename(FilePath);
	SendTranscriptionRequest(FileData, FileName, InLanguage);
}

void UPlayKitSTTClient::TranscribeAudioData(const TArray<uint8>& AudioData, const FString& FileName)
{
	if (bIsProcessing)
	{
		BroadcastError(TEXT("REQUEST_IN_PROGRESS"), TEXT("A request is already in progress"));
		return;
	}

	if (AudioData.Num() == 0)
	{
		BroadcastError(TEXT("INVALID_DATA"), TEXT("Audio data cannot be empty"));
		return;
	}

	SendTranscriptionRequest(AudioData, FileName, Language);
}

void UPlayKitSTTClient::SendTranscriptionRequest(const TArray<uint8>& AudioData, const FString& FileName, const FString& InLanguage)
{
	UPlayKitSettings* Settings = UPlayKitSettings::Get();
	if (!Settings)
	{
		BroadcastError(TEXT("CONFIG_ERROR"), TEXT("Settings not found"));
		return;
	}

	FString Url = FString::Printf(TEXT("%s/ai/%s/v2/audio/transcriptions"), *Settings->GetBaseUrl(), *Settings->GameId);

	bIsProcessing = true;

	// Build multipart form data
	FString Boundary = FString::Printf(TEXT("----PlayKitBoundary%d"), FMath::RandRange(100000, 999999));

	TArray<uint8> RequestBody;

	// Add model field
	{
		FString FieldHeader = FString::Printf(TEXT("--%s\r\nContent-Disposition: form-data; name=\"model\"\r\n\r\n%s\r\n"), *Boundary, *ModelName);
		RequestBody.Append((uint8*)TCHAR_TO_UTF8(*FieldHeader), FieldHeader.Len());
	}

	// Add language field if specified
	if (!InLanguage.IsEmpty())
	{
		FString FieldHeader = FString::Printf(TEXT("--%s\r\nContent-Disposition: form-data; name=\"language\"\r\n\r\n%s\r\n"), *Boundary, *InLanguage);
		RequestBody.Append((uint8*)TCHAR_TO_UTF8(*FieldHeader), FieldHeader.Len());
	}

	// Add response format
	{
		FString FieldHeader = FString::Printf(TEXT("--%s\r\nContent-Disposition: form-data; name=\"response_format\"\r\n\r\nverbose_json\r\n"), *Boundary);
		RequestBody.Append((uint8*)TCHAR_TO_UTF8(*FieldHeader), FieldHeader.Len());
	}

	// Add file field
	{
		FString ContentType = TEXT("audio/wav");
		if (FileName.EndsWith(TEXT(".mp3")))
		{
			ContentType = TEXT("audio/mpeg");
		}
		else if (FileName.EndsWith(TEXT(".m4a")))
		{
			ContentType = TEXT("audio/mp4");
		}
		else if (FileName.EndsWith(TEXT(".ogg")))
		{
			ContentType = TEXT("audio/ogg");
		}

		FString FileHeader = FString::Printf(TEXT("--%s\r\nContent-Disposition: form-data; name=\"file\"; filename=\"%s\"\r\nContent-Type: %s\r\n\r\n"), *Boundary, *FileName, *ContentType);
		RequestBody.Append((uint8*)TCHAR_TO_UTF8(*FileHeader), FileHeader.Len());
		RequestBody.Append(AudioData);

		FString FileTail = TEXT("\r\n");
		RequestBody.Append((uint8*)TCHAR_TO_UTF8(*FileTail), FileTail.Len());
	}

	// Add closing boundary
	{
		FString ClosingBoundary = FString::Printf(TEXT("--%s--\r\n"), *Boundary);
		RequestBody.Append((uint8*)TCHAR_TO_UTF8(*ClosingBoundary), ClosingBoundary.Len());
	}

	CurrentRequest = CreateAuthenticatedRequest(Url);
	CurrentRequest->SetHeader(TEXT("Content-Type"), FString::Printf(TEXT("multipart/form-data; boundary=%s"), *Boundary));
	CurrentRequest->SetContent(RequestBody);
	CurrentRequest->OnProcessRequestComplete().BindUObject(this, &UPlayKitSTTClient::HandleTranscriptionResponse);

	UE_LOG(LogTemp, Log, TEXT("[PlayKit] Sending transcription request to: %s"), *Url);
	CurrentRequest->ProcessRequest();
}

void UPlayKitSTTClient::HandleTranscriptionResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	bIsProcessing = false;
	CurrentRequest.Reset();

	if (!bWasSuccessful || !Response.IsValid())
	{
		BroadcastError(TEXT("NETWORK_ERROR"), TEXT("Network request failed"));
		return;
	}

	int32 ResponseCode = Response->GetResponseCode();
	FString ResponseContent = Response->GetContentAsString();

	if (ResponseCode < 200 || ResponseCode >= 300)
	{
		UE_LOG(LogTemp, Error, TEXT("[PlayKit] STT error %d: %s"), ResponseCode, *ResponseContent);
		BroadcastError(FString::FromInt(ResponseCode), ResponseContent);
		return;
	}

	// Parse response
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		BroadcastError(TEXT("PARSE_ERROR"), TEXT("Failed to parse response"));
		return;
	}

	FPlayKitTranscriptionResult Result;
	Result.bSuccess = true;

	JsonObject->TryGetStringField(TEXT("text"), Result.Text);
	JsonObject->TryGetStringField(TEXT("language"), Result.Language);
	JsonObject->TryGetNumberField(TEXT("duration"), Result.DurationSeconds);

	// Parse segments
	const TArray<TSharedPtr<FJsonValue>>* SegmentsArray;
	if (JsonObject->TryGetArrayField(TEXT("segments"), SegmentsArray))
	{
		for (const TSharedPtr<FJsonValue>& SegmentValue : *SegmentsArray)
		{
			TSharedPtr<FJsonObject> SegmentObj = SegmentValue->AsObject();
			if (SegmentObj)
			{
				FPlayKitTranscriptionSegment Segment;
				SegmentObj->TryGetNumberField(TEXT("start"), Segment.Start);
				SegmentObj->TryGetNumberField(TEXT("end"), Segment.End);
				SegmentObj->TryGetStringField(TEXT("text"), Segment.Text);
				Result.Segments.Add(Segment);
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[PlayKit] Transcription complete: %s"), *Result.Text.Left(100));
	OnTranscriptionComplete.Broadcast(Result);
}

void UPlayKitSTTClient::CancelRequest()
{
	if (CurrentRequest.IsValid())
	{
		CurrentRequest->CancelRequest();
		CurrentRequest.Reset();
	}
	bIsProcessing = false;
}

void UPlayKitSTTClient::BroadcastError(const FString& ErrorCode, const FString& ErrorMessage)
{
	UE_LOG(LogTemp, Error, TEXT("[PlayKit] STT error [%s]: %s"), *ErrorCode, *ErrorMessage);
	OnError.Broadcast(ErrorCode, ErrorMessage);

	// Also broadcast a failed result
	FPlayKitTranscriptionResult FailedResult;
	FailedResult.bSuccess = false;
	FailedResult.ErrorMessage = ErrorMessage;
	OnTranscriptionComplete.Broadcast(FailedResult);
}
