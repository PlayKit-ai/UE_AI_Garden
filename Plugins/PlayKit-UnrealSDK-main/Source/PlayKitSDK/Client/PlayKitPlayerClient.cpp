// Copyright PlayKit. All Rights Reserved.

#include "PlayKitPlayerClient.h"
#include "PlayKitSettings.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"

UPlayKitPlayerClient::UPlayKitPlayerClient()
{
}

void UPlayKitPlayerClient::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTemp, Log, TEXT("[PlayKit] PlayerClient subsystem initialized"));
}

void UPlayKitPlayerClient::Deinitialize()
{
	// Cancel any pending requests
	if (CurrentRequest.IsValid())
	{
		CurrentRequest->CancelRequest();
		CurrentRequest.Reset();
	}

	UE_LOG(LogTemp, Log, TEXT("[PlayKit] PlayerClient subsystem deinitialized"));
	Super::Deinitialize();
}

UPlayKitPlayerClient* UPlayKitPlayerClient::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance)
	{
		return nullptr;
	}

	return GameInstance->GetSubsystem<UPlayKitPlayerClient>();
}

bool UPlayKitPlayerClient::HasValidToken() const
{
	UPlayKitSettings* Settings = UPlayKitSettings::Get();
	if (!Settings)
	{
		return false;
	}
	return !Settings->GetPlayerToken().IsEmpty();
}

TSharedRef<IHttpRequest, ESPMode::ThreadSafe> UPlayKitPlayerClient::CreateAuthenticatedRequest(const FString& Url, const FString& Verb)
{
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(Verb);
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	UPlayKitSettings* Settings = UPlayKitSettings::Get();
	if (Settings)
	{
		FString Token = Settings->GetPlayerToken();
		if (Token.IsEmpty() && Settings->HasDeveloperToken())
		{
			Token = Settings->GetDeveloperToken();
		}
		if (!Token.IsEmpty())
		{
			Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *Token));
		}
	}

	return Request;
}

void UPlayKitPlayerClient::GetPlayerInfo()
{
	UPlayKitSettings* Settings = UPlayKitSettings::Get();
	if (!Settings)
	{
		BroadcastError(TEXT("Settings not found"));
		return;
	}

	FString Url = FString::Printf(TEXT("%s/api/external/player-info"), *Settings->GetBaseUrl());

	CurrentRequest = CreateAuthenticatedRequest(Url);
	CurrentRequest->OnProcessRequestComplete().BindUObject(this, &UPlayKitPlayerClient::HandlePlayerInfoResponse);

	UE_LOG(LogTemp, Log, TEXT("[PlayKit] Getting player info from: %s"), *Url);
	CurrentRequest->ProcessRequest();
}

void UPlayKitPlayerClient::HandlePlayerInfoResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	CurrentRequest.Reset();

	if (!bWasSuccessful || !Response.IsValid())
	{
		BroadcastError(TEXT("Network request failed"));
		return;
	}

	int32 ResponseCode = Response->GetResponseCode();
	FString ResponseContent = Response->GetContentAsString();

	if (ResponseCode < 200 || ResponseCode >= 300)
	{
		BroadcastError(FString::Printf(TEXT("HTTP %d: %s"), ResponseCode, *ResponseContent));
		return;
	}

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		BroadcastError(TEXT("Failed to parse response"));
		return;
	}

	JsonObject->TryGetStringField(TEXT("userId"), CachedPlayerInfo.UserId);
	JsonObject->TryGetNumberField(TEXT("credits"), CachedPlayerInfo.Credits);
	JsonObject->TryGetStringField(TEXT("nickname"), CachedPlayerInfo.Nickname);

	UE_LOG(LogTemp, Log, TEXT("[PlayKit] Player info: %s, Credits: %.2f"), *CachedPlayerInfo.UserId, CachedPlayerInfo.Credits);
	OnPlayerInfoUpdated.Broadcast(CachedPlayerInfo);
}

void UPlayKitPlayerClient::SetNickname(const FString& Nickname)
{
	if (Nickname.IsEmpty())
	{
		BroadcastError(TEXT("Nickname cannot be empty"));
		return;
	}

	FString TrimmedNickname = Nickname.TrimStartAndEnd();
	if (TrimmedNickname.Len() > 16)
	{
		BroadcastError(TEXT("Nickname must be 16 characters or less"));
		return;
	}

	UPlayKitSettings* Settings = UPlayKitSettings::Get();
	if (!Settings)
	{
		BroadcastError(TEXT("Settings not found"));
		return;
	}

	FString Url = FString::Printf(TEXT("%s/api/external/set-game-player-nickname"), *Settings->GetBaseUrl());

	TSharedPtr<FJsonObject> RequestBody = MakeShared<FJsonObject>();
	RequestBody->SetStringField(TEXT("nickname"), TrimmedNickname);

	FString RequestBodyStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBodyStr);
	FJsonSerializer::Serialize(RequestBody.ToSharedRef(), Writer);

	CurrentRequest = CreateAuthenticatedRequest(Url, TEXT("POST"));
	CurrentRequest->SetContentAsString(RequestBodyStr);
	CurrentRequest->OnProcessRequestComplete().BindUObject(this, &UPlayKitPlayerClient::HandleSetNicknameResponse);

	UE_LOG(LogTemp, Log, TEXT("[PlayKit] Setting nickname: %s"), *TrimmedNickname);
	CurrentRequest->ProcessRequest();
}

void UPlayKitPlayerClient::HandleSetNicknameResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	CurrentRequest.Reset();

	if (!bWasSuccessful || !Response.IsValid())
	{
		BroadcastError(TEXT("Network request failed"));
		return;
	}

	int32 ResponseCode = Response->GetResponseCode();
	FString ResponseContent = Response->GetContentAsString();

	if (ResponseCode < 200 || ResponseCode >= 300)
	{
		BroadcastError(FString::Printf(TEXT("HTTP %d: %s"), ResponseCode, *ResponseContent));
		return;
	}

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		BroadcastError(TEXT("Failed to parse response"));
		return;
	}

	bool bSuccess = false;
	JsonObject->TryGetBoolField(TEXT("success"), bSuccess);

	if (bSuccess)
	{
		FString NewNickname;
		JsonObject->TryGetStringField(TEXT("nickname"), NewNickname);
		CachedPlayerInfo.Nickname = NewNickname;

		UE_LOG(LogTemp, Log, TEXT("[PlayKit] Nickname set: %s"), *NewNickname);
		OnPlayerInfoUpdated.Broadcast(CachedPlayerInfo);
	}
	else
	{
		const TSharedPtr<FJsonObject>* ErrorObjPtr;
		if (JsonObject->TryGetObjectField(TEXT("error"), ErrorObjPtr) && ErrorObjPtr)
		{
			FString ErrorMessage;
			(*ErrorObjPtr)->TryGetStringField(TEXT("message"), ErrorMessage);
			BroadcastError(ErrorMessage);
		}
		else
		{
			BroadcastError(TEXT("Failed to set nickname"));
		}
	}
}

void UPlayKitPlayerClient::RefreshDailyCredits()
{
	UPlayKitSettings* Settings = UPlayKitSettings::Get();
	if (!Settings)
	{
		BroadcastError(TEXT("Settings not found"));
		return;
	}

	FString Url = FString::Printf(TEXT("%s/api/external/refresh-daily-credits"), *Settings->GetBaseUrl());

	CurrentRequest = CreateAuthenticatedRequest(Url, TEXT("POST"));
	CurrentRequest->SetContentAsString(TEXT("{}"));
	CurrentRequest->OnProcessRequestComplete().BindUObject(this, &UPlayKitPlayerClient::HandleDailyCreditsResponse);

	UE_LOG(LogTemp, Log, TEXT("[PlayKit] Refreshing daily credits"));
	CurrentRequest->ProcessRequest();
}

void UPlayKitPlayerClient::HandleDailyCreditsResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	CurrentRequest.Reset();

	if (!bWasSuccessful || !Response.IsValid())
	{
		BroadcastError(TEXT("Network request failed"));
		return;
	}

	int32 ResponseCode = Response->GetResponseCode();
	FString ResponseContent = Response->GetContentAsString();

	if (ResponseCode < 200 || ResponseCode >= 300)
	{
		BroadcastError(FString::Printf(TEXT("HTTP %d: %s"), ResponseCode, *ResponseContent));
		return;
	}

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		BroadcastError(TEXT("Failed to parse response"));
		return;
	}

	FPlayKitDailyCreditsResult Result;
	JsonObject->TryGetBoolField(TEXT("success"), Result.bSuccess);
	JsonObject->TryGetBoolField(TEXT("refreshed"), Result.bRefreshed);
	JsonObject->TryGetStringField(TEXT("message"), Result.Message);
	JsonObject->TryGetNumberField(TEXT("balanceBefore"), Result.BalanceBefore);
	JsonObject->TryGetNumberField(TEXT("balanceAfter"), Result.BalanceAfter);
	JsonObject->TryGetNumberField(TEXT("amountAdded"), Result.AmountAdded);

	if (Result.bRefreshed)
	{
		CachedPlayerInfo.Credits = Result.BalanceAfter;
		OnPlayerInfoUpdated.Broadcast(CachedPlayerInfo);
	}

	UE_LOG(LogTemp, Log, TEXT("[PlayKit] Daily credits: %s"), *Result.Message);
	OnDailyCreditsRefreshed.Broadcast(Result);
}

void UPlayKitPlayerClient::ExchangeJWT(const FString& JWT)
{
	if (JWT.IsEmpty())
	{
		BroadcastError(TEXT("JWT cannot be empty"));
		return;
	}

	CurrentJWT = JWT;

	UPlayKitSettings* Settings = UPlayKitSettings::Get();
	if (!Settings)
	{
		BroadcastError(TEXT("Settings not found"));
		return;
	}

	FString Url = FString::Printf(TEXT("%s/api/external/exchange-jwt"), *Settings->GetBaseUrl());

	CurrentRequest = FHttpModule::Get().CreateRequest();
	CurrentRequest->SetURL(Url);
	CurrentRequest->SetVerb(TEXT("POST"));
	CurrentRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	CurrentRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *JWT));
	CurrentRequest->SetContentAsString(TEXT("{}"));
	CurrentRequest->OnProcessRequestComplete().BindUObject(this, &UPlayKitPlayerClient::HandleJWTExchangeResponse);

	UE_LOG(LogTemp, Log, TEXT("[PlayKit] Exchanging JWT for player token"));
	CurrentRequest->ProcessRequest();
}

void UPlayKitPlayerClient::HandleJWTExchangeResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	CurrentRequest.Reset();

	if (!bWasSuccessful || !Response.IsValid())
	{
		BroadcastError(TEXT("Network request failed"));
		return;
	}

	int32 ResponseCode = Response->GetResponseCode();
	FString ResponseContent = Response->GetContentAsString();

	if (ResponseCode < 200 || ResponseCode >= 300)
	{
		BroadcastError(FString::Printf(TEXT("HTTP %d: %s"), ResponseCode, *ResponseContent));
		return;
	}

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		BroadcastError(TEXT("Failed to parse response"));
		return;
	}

	bool bSuccess = false;
	JsonObject->TryGetBoolField(TEXT("success"), bSuccess);

	if (bSuccess)
	{
		FString PlayerToken;
		JsonObject->TryGetStringField(TEXT("playerToken"), PlayerToken);

		// Store the token
		UPlayKitSettings* Settings = UPlayKitSettings::Get();
		if (Settings)
		{
			Settings->SetPlayerToken(PlayerToken);
		}

		UE_LOG(LogTemp, Log, TEXT("[PlayKit] Player token received"));
		OnPlayerTokenReceived.Broadcast(PlayerToken);

		// Automatically fetch player info
		GetPlayerInfo();
	}
	else
	{
		BroadcastError(TEXT("JWT exchange failed"));
	}
}

void UPlayKitPlayerClient::SetPlayerToken(const FString& Token)
{
	UPlayKitSettings* Settings = UPlayKitSettings::Get();
	if (Settings)
	{
		Settings->SetPlayerToken(Token);
		UE_LOG(LogTemp, Log, TEXT("[PlayKit] Player token set manually"));

		// Automatically fetch player info
		GetPlayerInfo();
	}
}

void UPlayKitPlayerClient::ClearPlayerToken()
{
	UPlayKitSettings* Settings = UPlayKitSettings::Get();
	if (Settings)
	{
		Settings->ClearPlayerToken();
	}

	CachedPlayerInfo = FPlayKitPlayerInfo();
	UE_LOG(LogTemp, Log, TEXT("[PlayKit] Player token cleared"));
}

void UPlayKitPlayerClient::BroadcastError(const FString& ErrorMessage)
{
	UE_LOG(LogTemp, Error, TEXT("[PlayKit] Player error: %s"), *ErrorMessage);
	OnError.Broadcast(ErrorMessage);
}
