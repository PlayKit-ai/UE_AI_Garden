// Copyright PlayKit. All Rights Reserved.

#include "PlayKitChatClient.h"
#include "PlayKitSettings.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

UPlayKitChatClient::UPlayKitChatClient()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPlayKitChatClient::BeginPlay()
{
	Super::BeginPlay();

	// Load default model from settings if not set in editor
	if (ModelName.IsEmpty())
	{
		UPlayKitSettings* Settings = UPlayKitSettings::Get();
		if (Settings && !Settings->DefaultChatModel.IsEmpty())
		{
			ModelName = Settings->DefaultChatModel;
		}
		else
		{
			ModelName = TEXT("gpt-4o-mini");
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[PlayKit] ChatClient component initialized with model: %s"), *ModelName);
}

void UPlayKitChatClient::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelRequest();
	Super::EndPlay(EndPlayReason);
}

FString UPlayKitChatClient::BuildRequestUrl() const
{
	UPlayKitSettings* Settings = UPlayKitSettings::Get();
	if (!Settings)
	{
		return TEXT("");
	}
	return FString::Printf(TEXT("%s/ai/%s/v2/chat"), *Settings->GetBaseUrl(), *Settings->GameId);
}

TSharedRef<IHttpRequest, ESPMode::ThreadSafe> UPlayKitChatClient::CreateAuthenticatedRequest(const FString& Url)
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

void UPlayKitChatClient::GenerateText(const FString& Prompt)
{
	FPlayKitChatConfig Config;

	// Add system prompt if configured
	if (!SystemPrompt.IsEmpty())
	{
		Config.Messages.Add(FPlayKitChatMessage(TEXT("system"), SystemPrompt));
	}

	Config.Messages.Add(FPlayKitChatMessage(TEXT("user"), Prompt));
	Config.Temperature = Temperature;
	Config.MaxTokens = MaxTokens;

	GenerateTextAdvanced(Config);
}

void UPlayKitChatClient::GenerateTextAdvanced(const FPlayKitChatConfig& Config)
{
	SendChatRequest(Config, false);
}

void UPlayKitChatClient::GenerateTextStream(const FString& Prompt)
{
	FPlayKitChatConfig Config;

	// Add system prompt if configured
	if (!SystemPrompt.IsEmpty())
	{
		Config.Messages.Add(FPlayKitChatMessage(TEXT("system"), SystemPrompt));
	}

	Config.Messages.Add(FPlayKitChatMessage(TEXT("user"), Prompt));
	Config.Temperature = Temperature;
	Config.MaxTokens = MaxTokens;

	GenerateTextStreamAdvanced(Config);
}

void UPlayKitChatClient::GenerateTextStreamAdvanced(const FPlayKitChatConfig& Config)
{
	SendChatRequest(Config, true);
}

void UPlayKitChatClient::SendChatRequest(const FPlayKitChatConfig& Config, bool bStream)
{
	if (bIsProcessing)
	{
		BroadcastError(TEXT("REQUEST_IN_PROGRESS"), TEXT("A request is already in progress"));
		return;
	}

	FString Url = BuildRequestUrl();
	if (Url.IsEmpty())
	{
		BroadcastError(TEXT("CONFIG_ERROR"), TEXT("Failed to build request URL"));
		return;
	}

	bIsProcessing = true;
	StreamBuffer.Empty();
	AccumulatedContent.Empty();
	LastProcessedOffset = 0;

	// Build request body
	TSharedPtr<FJsonObject> RequestBody = MakeShared<FJsonObject>();
	RequestBody->SetStringField(TEXT("model"), ModelName);
	RequestBody->SetNumberField(TEXT("temperature"), Config.Temperature);
	RequestBody->SetBoolField(TEXT("stream"), bStream);

	if (Config.MaxTokens > 0)
	{
		RequestBody->SetNumberField(TEXT("max_tokens"), Config.MaxTokens);
	}

	// Add messages
	TArray<TSharedPtr<FJsonValue>> MessagesArray;
	for (const FPlayKitChatMessage& Message : Config.Messages)
	{
		TSharedPtr<FJsonObject> MessageObj = MakeShared<FJsonObject>();
		MessageObj->SetStringField(TEXT("role"), Message.Role);
		MessageObj->SetStringField(TEXT("content"), Message.Content);
		if (!Message.ToolCallId.IsEmpty())
		{
			MessageObj->SetStringField(TEXT("tool_call_id"), Message.ToolCallId);
		}
		MessagesArray.Add(MakeShared<FJsonValueObject>(MessageObj));
	}
	RequestBody->SetArrayField(TEXT("messages"), MessagesArray);

	// Serialize
	FString RequestBodyStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBodyStr);
	FJsonSerializer::Serialize(RequestBody.ToSharedRef(), Writer);

	UE_LOG(LogTemp, Log, TEXT("[PlayKit] Request body: %s"), *RequestBodyStr.Left(500));

	// Create and send request
	CurrentRequest = CreateAuthenticatedRequest(Url);
	CurrentRequest->SetContentAsString(RequestBodyStr);

	if (bStream)
	{
		UE_LOG(LogTemp, Log, TEXT("[PlayKit] Using STREAMING mode"));
		CurrentRequest->OnRequestProgress64().BindUObject(this, &UPlayKitChatClient::HandleStreamProgress);
		CurrentRequest->OnProcessRequestComplete().BindUObject(this, &UPlayKitChatClient::HandleStreamComplete);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[PlayKit] Using NON-STREAMING mode"));
		CurrentRequest->OnProcessRequestComplete().BindUObject(this, &UPlayKitChatClient::HandleChatResponse);
	}

	UE_LOG(LogTemp, Log, TEXT("[PlayKit] Sending chat request to: %s"), *Url);
	CurrentRequest->ProcessRequest();
}

void UPlayKitChatClient::HandleChatResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	UE_LOG(LogTemp, Log, TEXT("[PlayKit] HandleChatResponse called - bWasSuccessful: %d"), bWasSuccessful);

	bIsProcessing = false;
	CurrentRequest.Reset();

	if (!bWasSuccessful || !Response.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[PlayKit] Response invalid or unsuccessful"));
		BroadcastError(TEXT("NETWORK_ERROR"), TEXT("Network request failed"));
		return;
	}

	int32 ResponseCode = Response->GetResponseCode();
	FString ResponseContent = Response->GetContentAsString();

	UE_LOG(LogTemp, Log, TEXT("[PlayKit] Response code: %d, Content length: %d"), ResponseCode, ResponseContent.Len());
	UE_LOG(LogTemp, Log, TEXT("[PlayKit] Response: %s"), *ResponseContent.Left(500));

	if (ResponseCode < 200 || ResponseCode >= 300)
	{
		UE_LOG(LogTemp, Error, TEXT("[PlayKit] Chat error %d: %s"), ResponseCode, *ResponseContent);
		BroadcastError(FString::FromInt(ResponseCode), ResponseContent);
		return;
	}

	FPlayKitChatResponse ChatResponse = ParseChatResponse(ResponseContent);
	UE_LOG(LogTemp, Log, TEXT("[PlayKit] Parsed response - Success: %d, Content: %s"), ChatResponse.bSuccess, *ChatResponse.Content.Left(200));
	UE_LOG(LogTemp, Log, TEXT("[PlayKit] OnChatResponse delegate bound: %s"), OnChatResponse.IsBound() ? TEXT("YES") : TEXT("NO"));
	OnChatResponse.Broadcast(ChatResponse);
	UE_LOG(LogTemp, Log, TEXT("[PlayKit] OnChatResponse.Broadcast completed"));
}

void UPlayKitChatClient::HandleStreamProgress(FHttpRequestPtr Request, uint64 BytesSent, uint64 BytesReceived)
{
	UE_LOG(LogTemp, Verbose, TEXT("[PlayKit] Stream progress - Sent: %llu, Received: %llu"), BytesSent, BytesReceived);

	if (!Request.IsValid())
	{
		return;
	}

	FHttpResponsePtr Response = Request->GetResponse();
	if (!Response.IsValid())
	{
		return;
	}

	FString CurrentContent = Response->GetContentAsString();

	// Process only new data
	if (CurrentContent.Len() <= LastProcessedOffset)
	{
		return;
	}

	FString NewData = CurrentContent.Mid(LastProcessedOffset);
	UE_LOG(LogTemp, Log, TEXT("[PlayKit] Stream new data: %s"), *NewData.Left(200));
	LastProcessedOffset = CurrentContent.Len();

	// Parse SSE data
	TArray<FString> Lines;
	NewData.ParseIntoArrayLines(Lines);

	for (const FString& Line : Lines)
	{
		if (Line.StartsWith(TEXT("data: ")))
		{
			FString JsonStr = Line.Mid(6).TrimStartAndEnd();

			if (JsonStr == TEXT("[DONE]"))
			{
				continue;
			}

			TSharedPtr<FJsonObject> JsonObject;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
			if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
			{
				// Try UI Message Stream format first (type, delta)
				FString Type;
				if (JsonObject->TryGetStringField(TEXT("type"), Type))
				{
					if (Type == TEXT("text-delta"))
					{
						FString Delta;
						if (JsonObject->TryGetStringField(TEXT("delta"), Delta) && !Delta.IsEmpty())
						{
							AccumulatedContent += Delta;
							OnStreamChunk.Broadcast(Delta);
						}
					}
					// Handle other types like "start", "finish" if needed
					continue;
				}

				// Fallback to legacy OpenAI format (choices, delta)
				const TArray<TSharedPtr<FJsonValue>>* Choices;
				if (JsonObject->TryGetArrayField(TEXT("choices"), Choices) && Choices->Num() > 0)
				{
					TSharedPtr<FJsonObject> Choice = (*Choices)[0]->AsObject();
					const TSharedPtr<FJsonObject>* DeltaPtr;
					if (Choice && Choice->TryGetObjectField(TEXT("delta"), DeltaPtr) && DeltaPtr)
					{
						FString Content;
						if ((*DeltaPtr)->TryGetStringField(TEXT("content"), Content) && !Content.IsEmpty())
						{
							AccumulatedContent += Content;
							OnStreamChunk.Broadcast(Content);
						}
					}
				}
			}
		}
	}
}

void UPlayKitChatClient::HandleStreamComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	UE_LOG(LogTemp, Log, TEXT("[PlayKit] HandleStreamComplete called - bWasSuccessful: %d"), bWasSuccessful);

	bIsProcessing = false;
	CurrentRequest.Reset();

	if (!bWasSuccessful)
	{
		UE_LOG(LogTemp, Error, TEXT("[PlayKit] Stream request failed"));
		BroadcastError(TEXT("NETWORK_ERROR"), TEXT("Stream request failed"));
		return;
	}

	if (Response.IsValid())
	{
		int32 ResponseCode = Response->GetResponseCode();
		UE_LOG(LogTemp, Log, TEXT("[PlayKit] Stream complete - Response code: %d"), ResponseCode);
		if (ResponseCode < 200 || ResponseCode >= 300)
		{
			UE_LOG(LogTemp, Error, TEXT("[PlayKit] Stream error: %s"), *Response->GetContentAsString());
			BroadcastError(FString::FromInt(ResponseCode), Response->GetContentAsString());
			return;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[PlayKit] Stream complete - Accumulated content length: %d"), AccumulatedContent.Len());
	OnStreamComplete.Broadcast(AccumulatedContent);
}

void UPlayKitChatClient::GenerateStructured(const FString& Prompt, const FString& SchemaJson)
{
	if (bIsProcessing)
	{
		OnStructuredResponse.Broadcast(false, TEXT("{\"error\": \"Request already in progress\"}"));
		return;
	}

	// Use the same /v2/chat endpoint with schema parameters
	FString Url = BuildRequestUrl();
	if (Url.IsEmpty())
	{
		OnStructuredResponse.Broadcast(false, TEXT("{\"error\": \"Failed to build request URL\"}"));
		return;
	}

	bIsProcessing = true;

	// Build messages array
	TArray<TSharedPtr<FJsonValue>> MessagesArray;

	if (!SystemPrompt.IsEmpty())
	{
		TSharedPtr<FJsonObject> SystemMsg = MakeShared<FJsonObject>();
		SystemMsg->SetStringField(TEXT("role"), TEXT("system"));
		SystemMsg->SetStringField(TEXT("content"), SystemPrompt);
		MessagesArray.Add(MakeShared<FJsonValueObject>(SystemMsg));
	}

	TSharedPtr<FJsonObject> UserMsg = MakeShared<FJsonObject>();
	UserMsg->SetStringField(TEXT("role"), TEXT("user"));
	UserMsg->SetStringField(TEXT("content"), Prompt);
	MessagesArray.Add(MakeShared<FJsonValueObject>(UserMsg));

	// Build request body using v2 chat format with schema
	TSharedPtr<FJsonObject> RequestBody = MakeShared<FJsonObject>();
	RequestBody->SetStringField(TEXT("model"), ModelName);
	RequestBody->SetArrayField(TEXT("messages"), MessagesArray);
	RequestBody->SetBoolField(TEXT("stream"), false);
	RequestBody->SetNumberField(TEXT("temperature"), Temperature);
	RequestBody->SetStringField(TEXT("output"), TEXT("object"));
	RequestBody->SetStringField(TEXT("schemaName"), TEXT("response"));
	RequestBody->SetStringField(TEXT("schemaDescription"), TEXT(""));

	// Parse schema JSON
	TSharedPtr<FJsonObject> SchemaObject;
	TSharedRef<TJsonReader<>> SchemaReader = TJsonReaderFactory<>::Create(SchemaJson);
	if (FJsonSerializer::Deserialize(SchemaReader, SchemaObject) && SchemaObject.IsValid())
	{
		RequestBody->SetObjectField(TEXT("schema"), SchemaObject);
	}
	else
	{
		bIsProcessing = false;
		OnStructuredResponse.Broadcast(false, TEXT("{\"error\": \"Invalid schema JSON\"}"));
		return;
	}

	// Serialize
	FString RequestBodyStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBodyStr);
	FJsonSerializer::Serialize(RequestBody.ToSharedRef(), Writer);

	CurrentRequest = CreateAuthenticatedRequest(Url);
	CurrentRequest->SetContentAsString(RequestBodyStr);
	CurrentRequest->OnProcessRequestComplete().BindUObject(this, &UPlayKitChatClient::HandleStructuredResponse);

	UE_LOG(LogTemp, Log, TEXT("[PlayKit] Sending structured request to: %s"), *Url);
	CurrentRequest->ProcessRequest();
}

void UPlayKitChatClient::HandleStructuredResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	bIsProcessing = false;
	CurrentRequest.Reset();

	if (!bWasSuccessful || !Response.IsValid())
	{
		OnStructuredResponse.Broadcast(false, TEXT("{\"error\": \"Network request failed\"}"));
		return;
	}

	int32 ResponseCode = Response->GetResponseCode();
	FString ResponseContent = Response->GetContentAsString();

	if (ResponseCode < 200 || ResponseCode >= 300)
	{
		OnStructuredResponse.Broadcast(false, ResponseContent);
		return;
	}

	// Parse response to extract the object
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);
	if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
	{
		const TSharedPtr<FJsonObject>* ObjectResultPtr;
		if (JsonObject->TryGetObjectField(TEXT("object"), ObjectResultPtr) && ObjectResultPtr)
		{
			FString ResultStr;
			TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultStr);
			FJsonSerializer::Serialize((*ObjectResultPtr).ToSharedRef(), Writer);
			OnStructuredResponse.Broadcast(true, ResultStr);
			return;
		}
	}

	OnStructuredResponse.Broadcast(true, ResponseContent);
}

FPlayKitChatResponse UPlayKitChatClient::ParseChatResponse(const FString& ResponseContent)
{
	FPlayKitChatResponse Result;

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("Failed to parse response JSON");
		return Result;
	}

	Result.bSuccess = true;

	// Parse choices
	const TArray<TSharedPtr<FJsonValue>>* Choices;
	if (JsonObject->TryGetArrayField(TEXT("choices"), Choices) && Choices->Num() > 0)
	{
		TSharedPtr<FJsonObject> Choice = (*Choices)[0]->AsObject();
		if (Choice)
		{
			Choice->TryGetStringField(TEXT("finish_reason"), Result.FinishReason);

			const TSharedPtr<FJsonObject>* MessagePtr;
			if (Choice->TryGetObjectField(TEXT("message"), MessagePtr) && MessagePtr)
			{
				(*MessagePtr)->TryGetStringField(TEXT("content"), Result.Content);

				// Parse tool calls
				const TArray<TSharedPtr<FJsonValue>>* ToolCalls;
				if ((*MessagePtr)->TryGetArrayField(TEXT("tool_calls"), ToolCalls))
				{
					for (const TSharedPtr<FJsonValue>& ToolCallValue : *ToolCalls)
					{
						TSharedPtr<FJsonObject> ToolCallObj = ToolCallValue->AsObject();
						if (ToolCallObj)
						{
							FPlayKitToolCall ToolCall;
							ToolCallObj->TryGetStringField(TEXT("id"), ToolCall.Id);
							ToolCallObj->TryGetStringField(TEXT("type"), ToolCall.Type);

							const TSharedPtr<FJsonObject>* FunctionPtr;
							if (ToolCallObj->TryGetObjectField(TEXT("function"), FunctionPtr) && FunctionPtr)
							{
								(*FunctionPtr)->TryGetStringField(TEXT("name"), ToolCall.FunctionName);
								(*FunctionPtr)->TryGetStringField(TEXT("arguments"), ToolCall.FunctionArguments);
							}

							Result.ToolCalls.Add(ToolCall);
						}
					}
				}
			}
		}
	}

	// Parse usage
	const TSharedPtr<FJsonObject>* UsagePtr;
	if (JsonObject->TryGetObjectField(TEXT("usage"), UsagePtr) && UsagePtr)
	{
		(*UsagePtr)->TryGetNumberField(TEXT("prompt_tokens"), Result.PromptTokens);
		(*UsagePtr)->TryGetNumberField(TEXT("completion_tokens"), Result.CompletionTokens);
		(*UsagePtr)->TryGetNumberField(TEXT("total_tokens"), Result.TotalTokens);
	}

	return Result;
}

void UPlayKitChatClient::CancelRequest()
{
	if (CurrentRequest.IsValid())
	{
		CurrentRequest->CancelRequest();
		CurrentRequest.Reset();
	}
	bIsProcessing = false;
}

void UPlayKitChatClient::BroadcastError(const FString& ErrorCode, const FString& ErrorMessage)
{
	UE_LOG(LogTemp, Error, TEXT("[PlayKit] Chat error [%s]: %s"), *ErrorCode, *ErrorMessage);
	OnError.Broadcast(ErrorCode, ErrorMessage);

	// Also broadcast a failed response
	FPlayKitChatResponse FailedResponse;
	FailedResponse.bSuccess = false;
	FailedResponse.ErrorMessage = ErrorMessage;
	OnChatResponse.Broadcast(FailedResponse);
}
