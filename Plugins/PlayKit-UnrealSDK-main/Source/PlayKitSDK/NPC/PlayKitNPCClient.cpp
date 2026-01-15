// Copyright PlayKit. All Rights Reserved.

#include "PlayKitNPCClient.h"
#include "PlayKitSettings.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Tool/PlayKitTool.h"

UPlayKitNPCClient::UPlayKitNPCClient()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPlayKitNPCClient::BeginPlay()
{
	Super::BeginPlay();
}

void UPlayKitNPCClient::Setup(const FString& ModelName)
{
	UPlayKitSettings* Settings = UPlayKitSettings::Get();
	if (Settings)
	{
		Model = ModelName.IsEmpty() ? Settings->DefaultChatModel : ModelName;
		bIsSetup = true;
		UE_LOG(LogTemp, Log, TEXT("[PlayKit] NPCClient setup with model: %s"), *Model);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[PlayKit] NPCClient setup failed - Settings not found"));
	}
}

void UPlayKitNPCClient::SetPlayerToken(const FString& Token)
{
	PlayerToken = Token;
}

void UPlayKitNPCClient::SetModel(const FString& ModelName)
{
	Model = ModelName;
}

//========== Settings Helpers ==========//

FString UPlayKitNPCClient::GetBaseUrl() const
{
	UPlayKitSettings* Settings = UPlayKitSettings::Get();
	return Settings ? Settings->GetBaseUrl() : TEXT("https://api.playkit.ai");
}

FString UPlayKitNPCClient::GetGameId() const
{
	UPlayKitSettings* Settings = UPlayKitSettings::Get();
	return Settings ? Settings->GameId : TEXT("");
}

FString UPlayKitNPCClient::GetAuthToken() const
{
	// Use override token if set
	if (!PlayerToken.IsEmpty())
	{
		return PlayerToken;
	}

	// Otherwise use SDK token
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

void UPlayKitNPCClient::SetCharacterDesign(const FString& Design)
{
	CharacterDesign = Design;
}

//========== Memory System ==========//

void UPlayKitNPCClient::SetMemory(const FString& MemoryName, const FString& MemoryContent)
{
	if (MemoryContent.IsEmpty())
	{
		Memories.Remove(MemoryName);
	}
	else
	{
		Memories.Add(MemoryName, MemoryContent);
	}
}

FString UPlayKitNPCClient::GetMemory(const FString& MemoryName) const
{
	const FString* Found = Memories.Find(MemoryName);
	return Found ? *Found : FString();
}

TArray<FString> UPlayKitNPCClient::GetMemoryNames() const
{
	TArray<FString> Names;
	Memories.GetKeys(Names);
	return Names;
}

void UPlayKitNPCClient::ClearMemories()
{
	Memories.Empty();
}

//========== Conversation ==========//

void UPlayKitNPCClient::Talk(const FString& Message)
{
	if (bIsTalking)
	{
		OnError.Broadcast(TEXT("BUSY"), TEXT("NPC is already processing a message"));
		return;
	}

	if (GetAuthToken().IsEmpty())
	{
		OnError.Broadcast(TEXT("NOT_AUTHENTICATED"), TEXT("No auth token available"));
		return;
	}

	PendingUserMessage = Message;
	bIsTalking = true;
	SendChatRequest(false);
}

void UPlayKitNPCClient::TalkStream(const FString& Message)
{
	if (bIsTalking)
	{
		OnError.Broadcast(TEXT("BUSY"), TEXT("NPC is already processing a message"));
		return;
	}

	if (GetAuthToken().IsEmpty())
	{
		OnError.Broadcast(TEXT("NOT_AUTHENTICATED"), TEXT("No auth token available"));
		return;
	}

	PendingUserMessage = Message;
	bIsTalking = true;
	StreamBuffer.Empty();
	SendChatRequest(true);
}

TSharedRef<IHttpRequest, ESPMode::ThreadSafe> UPlayKitNPCClient::CreateAuthenticatedRequest(const FString& Url)
{
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	FString Token = GetAuthToken();
	if (!Token.IsEmpty())
	{
		Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *Token));
	}

	return Request;
}

FString UPlayKitNPCClient::BuildSystemPrompt() const
{
	FString Prompt = CharacterDesign;

	// Add memories to context
	if (Memories.Num() > 0)
	{
		Prompt += TEXT("\n\n[Current Memories]\n");
		for (const auto& Pair : Memories)
		{
			Prompt += FString::Printf(TEXT("- %s: %s\n"), *Pair.Key, *Pair.Value);
		}
	}

	return Prompt;
}

void UPlayKitNPCClient::SendChatRequest(bool bStream)
{
	const FString Url = FString::Printf(TEXT("%s/ai/%s/v2/chat"), *GetBaseUrl(), *GetGameId());
	CurrentRequest = CreateAuthenticatedRequest(Url);

	// Build messages array
	TArray<TSharedPtr<FJsonValue>> MessagesArray;

	// System message
	FString SystemPrompt = BuildSystemPrompt();
	if (!SystemPrompt.IsEmpty())
	{
		TSharedPtr<FJsonObject> SystemMsg = MakeShared<FJsonObject>();
		SystemMsg->SetStringField(TEXT("role"), TEXT("system"));
		SystemMsg->SetStringField(TEXT("content"), SystemPrompt);
		MessagesArray.Add(MakeShared<FJsonValueObject>(SystemMsg));
	}

	// History messages
	for (const FNPCMessage& Msg : ConversationHistory)
	{
		TSharedPtr<FJsonObject> HistoryMsg = MakeShared<FJsonObject>();
		HistoryMsg->SetStringField(TEXT("role"), Msg.Role);
		HistoryMsg->SetStringField(TEXT("content"), Msg.Content);
		MessagesArray.Add(MakeShared<FJsonValueObject>(HistoryMsg));
	}

	// Current user message
	TSharedPtr<FJsonObject> UserMsg = MakeShared<FJsonObject>();
	UserMsg->SetStringField(TEXT("role"), TEXT("user"));
	UserMsg->SetStringField(TEXT("content"), PendingUserMessage);
	MessagesArray.Add(MakeShared<FJsonValueObject>(UserMsg));

	// Build request body
	TSharedPtr<FJsonObject> RequestBody = MakeShared<FJsonObject>();
	RequestBody->SetStringField(TEXT("model"), Model);
	RequestBody->SetArrayField(TEXT("messages"), MessagesArray);
	RequestBody->SetNumberField(TEXT("temperature"), Temperature);
	RequestBody->SetBoolField(TEXT("stream"), bStream);

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(RequestBody.ToSharedRef(), Writer);
	CurrentRequest->SetContentAsString(JsonString);

	if (bStream)
	{
		CurrentRequest->OnRequestProgress64().BindUObject(
			this, &UPlayKitNPCClient::HandleStreamProgress);
	}

	CurrentRequest->OnProcessRequestComplete().BindUObject(
		this, &UPlayKitNPCClient::HandleChatResponse);

	UE_LOG(LogTemp, Log, TEXT("[NPCClient] Sending chat request, stream=%s"), bStream ? TEXT("true") : TEXT("false"));
	CurrentRequest->ProcessRequest();
}

void UPlayKitNPCClient::HandleStreamProgress(FHttpRequestPtr Request, uint64 BytesSent, uint64 BytesReceived)
{
	if (!Request.IsValid() || !Request->GetResponse().IsValid())
	{
		return;
	}

	FString Content = Request->GetResponse()->GetContentAsString();
	if (Content.Len() <= StreamBuffer.Len())
	{
		return;
	}

	// Get new content
	FString NewContent = Content.RightChop(StreamBuffer.Len());
	StreamBuffer = Content;

	// Parse SSE format
	TArray<FString> Lines;
	NewContent.ParseIntoArray(Lines, TEXT("\n"), true);

	for (const FString& Line : Lines)
	{
		if (Line.StartsWith(TEXT("data: ")))
		{
			FString Data = Line.RightChop(6);
			if (Data == TEXT("[DONE]"))
			{
				continue;
			}

			TSharedPtr<FJsonObject> JsonObject;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Data);
			if (FJsonSerializer::Deserialize(Reader, JsonObject))
			{
				const TArray<TSharedPtr<FJsonValue>>* Choices;
				if (JsonObject->TryGetArrayField(TEXT("choices"), Choices) && Choices->Num() > 0)
				{
					TSharedPtr<FJsonObject> Choice = (*Choices)[0]->AsObject();
					const TSharedPtr<FJsonObject>* DeltaPtr;
					if (Choice->TryGetObjectField(TEXT("delta"), DeltaPtr) && DeltaPtr)
					{
						FString ChunkContent;
						if ((*DeltaPtr)->TryGetStringField(TEXT("content"), ChunkContent))
						{
							OnStreamChunk.Broadcast(ChunkContent);
						}
					}
				}
			}
		}
	}
}

void UPlayKitNPCClient::HandleChatResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	bIsTalking = false;

	FNPCResponse NPCResponse;

	if (!bWasSuccessful || !Response.IsValid())
	{
		NPCResponse.bSuccess = false;
		NPCResponse.ErrorMessage = TEXT("Network error");
		OnResponse.Broadcast(NPCResponse);
		OnError.Broadcast(TEXT("NETWORK_ERROR"), TEXT("Failed to get response"));
		return;
	}

	const int32 ResponseCode = Response->GetResponseCode();
	UE_LOG(LogTemp, Log, TEXT("[NPCClient] Chat response: HTTP %d"), ResponseCode);

	if (ResponseCode != 200)
	{
		NPCResponse.bSuccess = false;
		NPCResponse.ErrorMessage = FString::Printf(TEXT("HTTP %d: %s"), ResponseCode, *Response->GetContentAsString());
		OnResponse.Broadcast(NPCResponse);
		OnError.Broadcast(TEXT("HTTP_ERROR"), NPCResponse.ErrorMessage);
		return;
	}

	// For streaming, extract full content from buffer
	if (!StreamBuffer.IsEmpty())
	{
		// Parse final content from stream
		FString FullContent;
		TArray<FString> Lines;
		StreamBuffer.ParseIntoArray(Lines, TEXT("\n"), true);

		for (const FString& Line : Lines)
		{
			if (Line.StartsWith(TEXT("data: ")))
			{
				FString Data = Line.RightChop(6);
				if (Data == TEXT("[DONE]"))
				{
					continue;
				}

				TSharedPtr<FJsonObject> JsonObject;
				TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Data);
				if (FJsonSerializer::Deserialize(Reader, JsonObject))
				{
					const TArray<TSharedPtr<FJsonValue>>* Choices;
					if (JsonObject->TryGetArrayField(TEXT("choices"), Choices) && Choices->Num() > 0)
					{
						TSharedPtr<FJsonObject> Choice = (*Choices)[0]->AsObject();
						const TSharedPtr<FJsonObject>* DeltaPtr;
						if (Choice->TryGetObjectField(TEXT("delta"), DeltaPtr) && DeltaPtr)
						{
							FString ChunkContent;
							if ((*DeltaPtr)->TryGetStringField(TEXT("content"), ChunkContent))
							{
								FullContent += ChunkContent;
							}
						}
					}
				}
			}
		}

		NPCResponse.bSuccess = true;
		NPCResponse.Content = FullContent;

		// Add to history
		ConversationHistory.Add(FNPCMessage(TEXT("user"), PendingUserMessage));
		ConversationHistory.Add(FNPCMessage(TEXT("assistant"), FullContent));

		OnStreamComplete.Broadcast(FullContent);
		OnResponse.Broadcast(NPCResponse);
		StreamBuffer.Empty();
	}
	else
	{
		// Parse non-streaming response
		TSharedPtr<FJsonObject> JsonObject;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
		if (!FJsonSerializer::Deserialize(Reader, JsonObject))
		{
			NPCResponse.bSuccess = false;
			NPCResponse.ErrorMessage = TEXT("Failed to parse response");
			OnResponse.Broadcast(NPCResponse);
			OnError.Broadcast(TEXT("PARSE_ERROR"), NPCResponse.ErrorMessage);
			return;
		}

		// Extract content
		const TArray<TSharedPtr<FJsonValue>>* Choices;
		if (JsonObject->TryGetArrayField(TEXT("choices"), Choices) && Choices->Num() > 0)
		{
			TSharedPtr<FJsonObject> Choice = (*Choices)[0]->AsObject();
			const TSharedPtr<FJsonObject>* MessagePtr;
			if (Choice->TryGetObjectField(TEXT("message"), MessagePtr) && MessagePtr)
			{
				(*MessagePtr)->TryGetStringField(TEXT("content"), NPCResponse.Content);

				// Check for tool calls / actions
				const TArray<TSharedPtr<FJsonValue>>* ToolCalls;
				if ((*MessagePtr)->TryGetArrayField(TEXT("tool_calls"), ToolCalls))
				{
					ParseActionCalls(*MessagePtr, NPCResponse.ActionCalls);
				}
			}
		}

		NPCResponse.bSuccess = true;

		// Add to history
		ConversationHistory.Add(FNPCMessage(TEXT("user"), PendingUserMessage));
		ConversationHistory.Add(FNPCMessage(TEXT("assistant"), NPCResponse.Content));

		// Broadcast action triggers
		for (const FNPCActionCall& ActionCall : NPCResponse.ActionCalls)
		{
			OnActionTriggered.Broadcast(ActionCall);
		}

		OnResponse.Broadcast(NPCResponse);
	}

	// Auto-generate predictions if enabled
	if (bAutoGenerateReplyPredictions)
	{
		GenerateReplyPredictions(PredictionCount);
	}
}

void UPlayKitNPCClient::ParseActionCalls(const TSharedPtr<FJsonObject>& JsonObject, TArray<FNPCActionCall>& OutActionCalls)
{
	const TArray<TSharedPtr<FJsonValue>>* ToolCalls;
	if (!JsonObject->TryGetArrayField(TEXT("tool_calls"), ToolCalls))
	{
		return;
	}

	for (const TSharedPtr<FJsonValue>& ToolCallValue : *ToolCalls)
	{
		TSharedPtr<FJsonObject> ToolCall = ToolCallValue->AsObject();
		if (!ToolCall.IsValid())
		{
			continue;
		}

		FNPCActionCall ActionCall;
		ActionCall.CallId = ToolCall->GetStringField(TEXT("id"));

		const TSharedPtr<FJsonObject>* FunctionPtr;
		if (ToolCall->TryGetObjectField(TEXT("function"), FunctionPtr) && FunctionPtr)
		{
			ActionCall.ActionName = (*FunctionPtr)->GetStringField(TEXT("name"));

			FString ArgumentsStr = (*FunctionPtr)->GetStringField(TEXT("arguments"));
			TSharedPtr<FJsonObject> Arguments;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ArgumentsStr);
			if (FJsonSerializer::Deserialize(Reader, Arguments))
			{
				for (const auto& Pair : Arguments->Values)
				{
					ActionCall.Parameters.Add(Pair.Key, Pair.Value->AsString());
				}
			}
		}

		OutActionCalls.Add(ActionCall);
	}
}

//========== History Management ==========//

void UPlayKitNPCClient::ClearHistory()
{
	ConversationHistory.Empty();
}

bool UPlayKitNPCClient::RevertHistory()
{
	// Remove last user message and assistant response
	if (ConversationHistory.Num() >= 2)
	{
		ConversationHistory.RemoveAt(ConversationHistory.Num() - 1);
		ConversationHistory.RemoveAt(ConversationHistory.Num() - 1);
		return true;
	}
	return false;
}

int32 UPlayKitNPCClient::RevertChatMessages(int32 Count)
{
	int32 Removed = FMath::Min(Count, ConversationHistory.Num());
	for (int32 i = 0; i < Removed; i++)
	{
		ConversationHistory.RemoveAt(ConversationHistory.Num() - 1);
	}
	return Removed;
}

void UPlayKitNPCClient::AppendChatMessage(const FString& Role, const FString& Content)
{
	ConversationHistory.Add(FNPCMessage(Role, Content));
}

FString UPlayKitNPCClient::SaveHistory() const
{
	TArray<TSharedPtr<FJsonValue>> HistoryArray;

	for (const FNPCMessage& Msg : ConversationHistory)
	{
		TSharedPtr<FJsonObject> MsgObj = MakeShared<FJsonObject>();
		MsgObj->SetStringField(TEXT("role"), Msg.Role);
		MsgObj->SetStringField(TEXT("content"), Msg.Content);
		HistoryArray.Add(MakeShared<FJsonValueObject>(MsgObj));
	}

	TSharedPtr<FJsonObject> SaveObj = MakeShared<FJsonObject>();
	SaveObj->SetArrayField(TEXT("history"), HistoryArray);
	SaveObj->SetStringField(TEXT("characterDesign"), CharacterDesign);

	// Save memories
	TSharedPtr<FJsonObject> MemoriesObj = MakeShared<FJsonObject>();
	for (const auto& Pair : Memories)
	{
		MemoriesObj->SetStringField(Pair.Key, Pair.Value);
	}
	SaveObj->SetObjectField(TEXT("memories"), MemoriesObj);

	return UPlayKitTool::JsonObjectToString(SaveObj);
}

bool UPlayKitNPCClient::LoadHistory(const FString& SaveData)
{
	TSharedPtr<FJsonObject> SaveObj;
	if (!UPlayKitTool::StringToJsonObject(SaveData, SaveObj, true))
	{
		return false;
	}

	// Load history
	ConversationHistory.Empty();
	const TArray<TSharedPtr<FJsonValue>>* HistoryArray;
	if (SaveObj->TryGetArrayField(TEXT("history"), HistoryArray))
	{
		for (const TSharedPtr<FJsonValue>& MsgValue : *HistoryArray)
		{
			TSharedPtr<FJsonObject> MsgObj = MsgValue->AsObject();
			if (MsgObj.IsValid())
			{
				FNPCMessage Msg;
				Msg.Role = MsgObj->GetStringField(TEXT("role"));
				Msg.Content = MsgObj->GetStringField(TEXT("content"));
				ConversationHistory.Add(Msg);
			}
		}
	}

	// Load character design
	SaveObj->TryGetStringField(TEXT("characterDesign"), CharacterDesign);

	// Load memories
	Memories.Empty();
	const TSharedPtr<FJsonObject>* MemoriesObjPtr;
	if (SaveObj->TryGetObjectField(TEXT("memories"), MemoriesObjPtr) && MemoriesObjPtr)
	{
		for (const auto& Pair : (*MemoriesObjPtr)->Values)
		{
			Memories.Add(Pair.Key, Pair.Value->AsString());
		}
	}

	return true;
}

//========== Action Results ==========//

void UPlayKitNPCClient::ReportActionResult(const FString& CallId, const FString& Result)
{
	PendingActionResults.Add(CallId, Result);
}

void UPlayKitNPCClient::ReportActionResults(const TMap<FString, FString>& Results)
{
	for (const auto& Pair : Results)
	{
		PendingActionResults.Add(Pair.Key, Pair.Value);
	}
}

//========== Reply Predictions ==========//

void UPlayKitNPCClient::GenerateReplyPredictions(int32 Count)
{
	if (GetAuthToken().IsEmpty())
	{
		OnError.Broadcast(TEXT("NOT_AUTHENTICATED"), TEXT("No auth token available"));
		return;
	}

	if (ConversationHistory.Num() < 2)
	{
		OnError.Broadcast(TEXT("NO_HISTORY"), TEXT("Not enough conversation history to generate predictions"));
		return;
	}

	// Get last NPC message
	FString LastNPCMessage = GetLastNPCMessage();
	if (LastNPCMessage.IsEmpty())
	{
		OnError.Broadcast(TEXT("NO_NPC_MESSAGE"), TEXT("No NPC message found to generate predictions from"));
		return;
	}

	const FString Url = FString::Printf(TEXT("%s/ai/%s/v2/chat"), *GetBaseUrl(), *GetGameId());
	PredictionsRequest = CreateAuthenticatedRequest(Url);

	// Build the prompt with detailed context (aligned with Unity SDK)
	FString RecentHistory = BuildRecentHistoryString();

	FString PromptContent = FString::Printf(
		TEXT("Based on the conversation history below, generate exactly %d natural and contextually appropriate responses that the player might say next.\n\n")
		TEXT("Context:\n")
		TEXT("- This is a conversation between a player and an NPC in a game\n")
		TEXT("- The NPC just said: \"%s\"\n\n")
		TEXT("Conversation history:\n%s\n\n")
		TEXT("Requirements:\n")
		TEXT("1. Each response should be 1-2 sentences maximum\n")
		TEXT("2. Responses should be diverse in tone and intent\n")
		TEXT("3. Include a mix of questions, statements, and action-oriented responses\n")
		TEXT("4. Responses should feel natural for a player character\n\n")
		TEXT("Output ONLY a JSON array of %d strings, nothing else:\n")
		TEXT("[\"response1\", \"response2\", \"response3\"]"),
		Count, *LastNPCMessage, *RecentHistory, Count
	);

	// Build messages array - only the prompt, no history needed in messages
	TArray<TSharedPtr<FJsonValue>> MessagesArray;
	TSharedPtr<FJsonObject> PredictMsg = MakeShared<FJsonObject>();
	PredictMsg->SetStringField(TEXT("role"), TEXT("user"));
	PredictMsg->SetStringField(TEXT("content"), PromptContent);
	MessagesArray.Add(MakeShared<FJsonValueObject>(PredictMsg));

	// Build request body - USE FAST MODEL
	UPlayKitSettings* Settings = UPlayKitSettings::Get();
	FString FastModelName = Settings ? Settings->FastModel : TEXT("gpt-4o-mini");

	TSharedPtr<FJsonObject> RequestBody = MakeShared<FJsonObject>();
	RequestBody->SetStringField(TEXT("model"), FastModelName);
	RequestBody->SetArrayField(TEXT("messages"), MessagesArray);
	RequestBody->SetNumberField(TEXT("temperature"), 0.8f);

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(RequestBody.ToSharedRef(), Writer);
	PredictionsRequest->SetContentAsString(JsonString);

	PredictionsRequest->OnProcessRequestComplete().BindUObject(
		this, &UPlayKitNPCClient::HandlePredictionsResponse);

	UE_LOG(LogTemp, Log, TEXT("[NPCClient] Generating %d predictions using model: %s"), Count, *FastModelName);
	PredictionsRequest->ProcessRequest();
}

void UPlayKitNPCClient::HandlePredictionsResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!bWasSuccessful || !Response.IsValid() || Response->GetResponseCode() != 200)
	{
		UE_LOG(LogTemp, Warning, TEXT("[NPCClient] Failed to generate predictions: HTTP error"));
		OnError.Broadcast(TEXT("PREDICTION_ERROR"), TEXT("Failed to generate predictions"));
		return;
	}

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
	if (!FJsonSerializer::Deserialize(Reader, JsonObject))
	{
		UE_LOG(LogTemp, Warning, TEXT("[NPCClient] Failed to parse predictions response JSON"));
		OnError.Broadcast(TEXT("PARSE_ERROR"), TEXT("Failed to parse predictions response"));
		return;
	}

	TArray<FString> Predictions;

	const TArray<TSharedPtr<FJsonValue>>* Choices;
	if (JsonObject->TryGetArrayField(TEXT("choices"), Choices) && Choices->Num() > 0)
	{
		TSharedPtr<FJsonObject> Choice = (*Choices)[0]->AsObject();
		const TSharedPtr<FJsonObject>* MessagePtr;
		if (Choice->TryGetObjectField(TEXT("message"), MessagePtr) && MessagePtr)
		{
			FString Content;
			if ((*MessagePtr)->TryGetStringField(TEXT("content"), Content))
			{
				// Primary: Try to parse as JSON array
				Predictions = ParsePredictionsFromJson(Content);

				// Fallback: If JSON parsing failed or returned empty, try text extraction
				if (Predictions.Num() == 0)
				{
					UE_LOG(LogTemp, Log, TEXT("[NPCClient] JSON parsing failed, trying text extraction fallback"));
					Predictions = ExtractPredictionsFromText(Content, PredictionCount);
				}
			}
		}
	}

	if (Predictions.Num() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[NPCClient] Generated %d reply predictions"), Predictions.Num());
		OnReplyPredictionsGenerated.Broadcast(Predictions);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[NPCClient] No predictions could be extracted from response"));
		OnError.Broadcast(TEXT("PARSE_ERROR"), TEXT("Failed to extract predictions from response"));
	}
}

//========== Reply Prediction Helpers ==========//

TArray<FString> UPlayKitNPCClient::ParsePredictionsFromJson(const FString& Response)
{
	TArray<FString> Predictions;

	// Find JSON array boundaries
	int32 StartIndex = Response.Find(TEXT("["));
	int32 EndIndex = Response.Find(TEXT("]"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);

	if (StartIndex == INDEX_NONE || EndIndex == INDEX_NONE || EndIndex <= StartIndex)
	{
		UE_LOG(LogTemp, Warning, TEXT("[NPCClient] Could not find JSON array in prediction response"));
		return Predictions;
	}

	FString JsonArray = Response.Mid(StartIndex, EndIndex - StartIndex + 1);

	// Manual JSON array parsing (similar to Unity SDK approach)
	bool bInString = false;
	bool bEscaped = false;
	FString CurrentString;

	for (int32 i = 1; i < JsonArray.Len() - 1; i++)
	{
		TCHAR c = JsonArray[i];

		if (bEscaped)
		{
			CurrentString.AppendChar(c);
			bEscaped = false;
			continue;
		}

		if (c == TEXT('\\'))
		{
			bEscaped = true;
			continue;
		}

		if (c == TEXT('"'))
		{
			if (bInString)
			{
				// End of string
				FString Trimmed = CurrentString.TrimStartAndEnd();
				if (!Trimmed.IsEmpty())
				{
					Predictions.Add(Trimmed);
				}
				CurrentString.Empty();
			}
			bInString = !bInString;
			continue;
		}

		if (bInString)
		{
			CurrentString.AppendChar(c);
		}
	}

	return Predictions;
}

TArray<FString> UPlayKitNPCClient::ExtractPredictionsFromText(const FString& Response, int32 ExpectedCount)
{
	TArray<FString> Predictions;
	TArray<FString> Lines;
	Response.ParseIntoArray(Lines, TEXT("\n"), true);

	for (const FString& Line : Lines)
	{
		FString Trimmed = Line.TrimStartAndEnd();

		// Skip empty lines and JSON brackets
		if (Trimmed.IsEmpty() || Trimmed == TEXT("[") || Trimmed == TEXT("]"))
		{
			continue;
		}

		FString Cleaned = Trimmed;

		// Remove common prefixes like "1.", "2.", etc.
		if (Cleaned.Len() > 2 && FChar::IsDigit(Cleaned[0]) && Cleaned[1] == TEXT('.'))
		{
			Cleaned = Cleaned.RightChop(2).TrimStartAndEnd();
		}
		// Remove "- " prefix
		else if (Cleaned.StartsWith(TEXT("- ")))
		{
			Cleaned = Cleaned.RightChop(2).TrimStartAndEnd();
		}

		// Remove surrounding quotes
		if (Cleaned.StartsWith(TEXT("\"")) && Cleaned.EndsWith(TEXT("\"")))
		{
			Cleaned = Cleaned.Mid(1, Cleaned.Len() - 2);
		}

		// Remove trailing comma
		if (Cleaned.EndsWith(TEXT(",")))
		{
			Cleaned = Cleaned.LeftChop(1).TrimStartAndEnd();
		}

		// Remove surrounding quotes again (in case of nested)
		if (Cleaned.StartsWith(TEXT("\"")) && Cleaned.EndsWith(TEXT("\"")))
		{
			Cleaned = Cleaned.Mid(1, Cleaned.Len() - 2);
		}

		if (!Cleaned.IsEmpty() && Predictions.Num() < ExpectedCount)
		{
			Predictions.Add(Cleaned);
		}
	}

	return Predictions;
}

FString UPlayKitNPCClient::BuildRecentHistoryString() const
{
	TArray<FString> RecentMessages;
	int32 Count = 0;
	const int32 MaxMessages = 6;  // Unity SDK uses last 6 non-system messages

	// Iterate from end to get most recent messages
	for (int32 i = ConversationHistory.Num() - 1; i >= 0 && Count < MaxMessages; i--)
	{
		const FNPCMessage& Msg = ConversationHistory[i];
		if (Msg.Role != TEXT("system"))
		{
			RecentMessages.Insert(FString::Printf(TEXT("%s: %s"), *Msg.Role, *Msg.Content), 0);
			Count++;
		}
	}

	return FString::Join(RecentMessages, TEXT("\n"));
}

FString UPlayKitNPCClient::GetLastNPCMessage() const
{
	// Find the last assistant message
	for (int32 i = ConversationHistory.Num() - 1; i >= 0; i--)
	{
		if (ConversationHistory[i].Role == TEXT("assistant"))
		{
			return ConversationHistory[i].Content;
		}
	}
	return FString();
}
