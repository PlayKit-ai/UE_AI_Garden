// Copyright PlayKit. All Rights Reserved.

#include "PlayKitDeviceAuthFlow.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Base64.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GenericPlatform/GenericPlatformProcess.h"

// SHA256 implementation (RFC 6234)
namespace
{
	static const uint32 K256[64] = {
		0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
		0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
		0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
		0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
		0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
		0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
		0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
		0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
	};

	inline uint32 RotR(uint32 x, int n) { return (x >> n) | (x << (32 - n)); }
	inline uint32 Ch(uint32 x, uint32 y, uint32 z) { return (x & y) ^ (~x & z); }
	inline uint32 Maj(uint32 x, uint32 y, uint32 z) { return (x & y) ^ (x & z) ^ (y & z); }
	inline uint32 Sigma0(uint32 x) { return RotR(x, 2) ^ RotR(x, 13) ^ RotR(x, 22); }
	inline uint32 Sigma1(uint32 x) { return RotR(x, 6) ^ RotR(x, 11) ^ RotR(x, 25); }
	inline uint32 sigma0(uint32 x) { return RotR(x, 7) ^ RotR(x, 18) ^ (x >> 3); }
	inline uint32 sigma1(uint32 x) { return RotR(x, 17) ^ RotR(x, 19) ^ (x >> 10); }

	void ComputeSHA256(const uint8* Data, uint64 DataLen, uint8 OutHash[32])
	{
		uint32 H[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };

		uint64 BitLen = DataLen * 8;
		uint64 PaddedLen = ((DataLen + 9 + 63) / 64) * 64;
		TArray<uint8> Padded;
		Padded.SetNumZeroed(PaddedLen);
		FMemory::Memcpy(Padded.GetData(), Data, DataLen);
		Padded[DataLen] = 0x80;
		for (int i = 0; i < 8; i++) Padded[PaddedLen - 1 - i] = (BitLen >> (i * 8)) & 0xff;

		for (uint64 Offset = 0; Offset < PaddedLen; Offset += 64)
		{
			uint32 W[64];
			for (int i = 0; i < 16; i++)
				W[i] = (Padded[Offset + i*4] << 24) | (Padded[Offset + i*4+1] << 16) | (Padded[Offset + i*4+2] << 8) | Padded[Offset + i*4+3];
			for (int i = 16; i < 64; i++)
				W[i] = sigma1(W[i-2]) + W[i-7] + sigma0(W[i-15]) + W[i-16];

			uint32 a = H[0], b = H[1], c = H[2], d = H[3], e = H[4], f = H[5], g = H[6], h = H[7];
			for (int i = 0; i < 64; i++)
			{
				uint32 T1 = h + Sigma1(e) + Ch(e, f, g) + K256[i] + W[i];
				uint32 T2 = Sigma0(a) + Maj(a, b, c);
				h = g; g = f; f = e; e = d + T1; d = c; c = b; b = a; a = T1 + T2;
			}
			H[0] += a; H[1] += b; H[2] += c; H[3] += d; H[4] += e; H[5] += f; H[6] += g; H[7] += h;
		}

		for (int i = 0; i < 8; i++)
		{
			OutHash[i*4] = (H[i] >> 24) & 0xff;
			OutHash[i*4+1] = (H[i] >> 16) & 0xff;
			OutHash[i*4+2] = (H[i] >> 8) & 0xff;
			OutHash[i*4+3] = H[i] & 0xff;
		}
	}
}

UPlayKitDeviceAuthFlow::UPlayKitDeviceAuthFlow()
{
}

void UPlayKitDeviceAuthFlow::StartAuthFlow(const FString& InGameId, const FString& InScope)
{
	if (IsActive())
	{
		UE_LOG(LogTemp, Warning, TEXT("[DeviceAuth] Auth flow already in progress"));
		return;
	}

	// Store configuration
	GameId = InGameId;
	Scope = InScope;

	// Generate PKCE codes
	CodeVerifier = GenerateCodeVerifier();
	CodeChallenge = GenerateCodeChallenge(CodeVerifier);

	UE_LOG(LogTemp, Log, TEXT("[DeviceAuth] Starting auth flow for GameId: %s"), *GameId);
	UE_LOG(LogTemp, Log, TEXT("[DeviceAuth] Code Verifier: %s"), *CodeVerifier);
	UE_LOG(LogTemp, Log, TEXT("[DeviceAuth] Code Challenge: %s"), *CodeChallenge);

	// Get world reference for timers
	if (GEngine && GEngine->GameViewport)
	{
		WorldPtr = GEngine->GameViewport->GetWorld();
	}

	SetStatus(EDeviceAuthStatus::Pending);
	RequestDeviceCode();
}

void UPlayKitDeviceAuthFlow::CancelAuthFlow()
{
	if (!IsActive())
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[DeviceAuth] Auth flow cancelled by user"));
	Cleanup();
	SetStatus(EDeviceAuthStatus::Cancelled);
}

FString UPlayKitDeviceAuthFlow::GenerateCodeVerifier()
{
	// Generate 32 random bytes
	TArray<uint8> RandomBytes;
	RandomBytes.SetNum(32);
	for (int32 i = 0; i < 32; i++)
	{
		RandomBytes[i] = static_cast<uint8>(FMath::RandRange(0, 255));
	}

	return Base64UrlEncode(RandomBytes);
}

FString UPlayKitDeviceAuthFlow::GenerateCodeChallenge(const FString& InCodeVerifier)
{
	// SHA256 hash of the code verifier
	FString Utf8Verifier = InCodeVerifier;
	auto Utf8Data = StringCast<UTF8CHAR>(*Utf8Verifier);
	const uint8* DataPtr = reinterpret_cast<const uint8*>(Utf8Data.Get());
	int32 DataLen = Utf8Data.Length();

	uint8 HashResult[32];
	ComputeSHA256(DataPtr, DataLen, HashResult);

	TArray<uint8> HashArray;
	HashArray.Append(HashResult, 32);

	return Base64UrlEncode(HashArray);
}

FString UPlayKitDeviceAuthFlow::Base64UrlEncode(const TArray<uint8>& Data)
{
	FString Base64 = FBase64::Encode(Data);
	// Convert to URL-safe base64
	Base64.ReplaceInline(TEXT("+"), TEXT("-"));
	Base64.ReplaceInline(TEXT("/"), TEXT("_"));
	// Remove padding
	Base64.ReplaceInline(TEXT("="), TEXT(""));
	return Base64;
}

void UPlayKitDeviceAuthFlow::RequestDeviceCode()
{
	const FString Url = BaseUrl / TEXT("api/auth/device/code");

	CurrentHttpRequest = FHttpModule::Get().CreateRequest();
	CurrentHttpRequest->SetURL(Url);
	CurrentHttpRequest->SetVerb(TEXT("POST"));
	CurrentHttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	// Build request body
	TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();
	JsonObject->SetStringField(TEXT("client_id"), GameId);
	JsonObject->SetStringField(TEXT("scope"), Scope);
	JsonObject->SetStringField(TEXT("code_challenge"), CodeChallenge);
	JsonObject->SetStringField(TEXT("code_challenge_method"), TEXT("S256"));

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
	CurrentHttpRequest->SetContentAsString(JsonString);

	UE_LOG(LogTemp, Log, TEXT("[DeviceAuth] Requesting device code from: %s"), *Url);

	CurrentHttpRequest->OnProcessRequestComplete().BindUObject(
		this, &UPlayKitDeviceAuthFlow::HandleDeviceCodeResponse);
	CurrentHttpRequest->ProcessRequest();
}

void UPlayKitDeviceAuthFlow::HandleDeviceCodeResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!bWasSuccessful || !Response.IsValid())
	{
		CompleteWithError(TEXT("NETWORK_ERROR"), TEXT("Failed to request device code"));
		return;
	}

	const int32 ResponseCode = Response->GetResponseCode();
	UE_LOG(LogTemp, Log, TEXT("[DeviceAuth] Device code response: HTTP %d"), ResponseCode);

	if (ResponseCode != 200)
	{
		CompleteWithError(TEXT("HTTP_ERROR"), FString::Printf(TEXT("HTTP %d: %s"), ResponseCode, *Response->GetContentAsString()));
		return;
	}

	// Parse response
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		CompleteWithError(TEXT("PARSE_ERROR"), TEXT("Failed to parse device code response"));
		return;
	}

	// Extract fields
	DeviceCode = JsonObject->GetStringField(TEXT("device_code"));
	UserCode = JsonObject->GetStringField(TEXT("user_code"));
	AuthUrl = JsonObject->GetStringField(TEXT("verification_uri"));
	PollingInterval = JsonObject->GetIntegerField(TEXT("interval"));
	ExpiresIn = JsonObject->GetIntegerField(TEXT("expires_in"));

	if (DeviceCode.IsEmpty() || AuthUrl.IsEmpty())
	{
		CompleteWithError(TEXT("INVALID_RESPONSE"), TEXT("Missing required fields in device code response"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[DeviceAuth] Device code received. User code: %s, Auth URL: %s"), *UserCode, *AuthUrl);

	// Notify that auth URL is ready
	OnAuthUrlReady.Broadcast(AuthUrl, UserCode);

	// Open browser automatically
	FString FullUrl = AuthUrl;
	if (!UserCode.IsEmpty())
	{
		FullUrl = FString::Printf(TEXT("%s?user_code=%s"), *AuthUrl, *UserCode);
	}
	FPlatformProcess::LaunchURL(*FullUrl, nullptr, nullptr);

	// Start polling
	StartPolling();
}

void UPlayKitDeviceAuthFlow::StartPolling()
{
	SetStatus(EDeviceAuthStatus::Polling);

	if (!WorldPtr.IsValid())
	{
		CompleteWithError(TEXT("WORLD_ERROR"), TEXT("World not available for timers"));
		return;
	}

	UWorld* World = WorldPtr.Get();

	// Set expiration timer
	World->GetTimerManager().SetTimer(
		ExpirationTimerHandle,
		[this]()
		{
			UE_LOG(LogTemp, Warning, TEXT("[DeviceAuth] Device code expired"));
			Cleanup();
			SetStatus(EDeviceAuthStatus::Expired);
			OnAuthError.Broadcast(TEXT("EXPIRED"), TEXT("Device code expired. Please start again."));
		},
		static_cast<float>(ExpiresIn),
		false
	);

	// Start polling timer
	World->GetTimerManager().SetTimer(
		PollingTimerHandle,
		[this]()
		{
			PollForToken();
		},
		static_cast<float>(PollingInterval),
		true,
		static_cast<float>(PollingInterval) // Initial delay
	);

	UE_LOG(LogTemp, Log, TEXT("[DeviceAuth] Started polling every %d seconds, expires in %d seconds"), PollingInterval, ExpiresIn);
}

void UPlayKitDeviceAuthFlow::PollForToken()
{
	if (Status != EDeviceAuthStatus::Polling)
	{
		return;
	}

	const FString Url = BaseUrl / TEXT("api/auth/device/token");

	CurrentHttpRequest = FHttpModule::Get().CreateRequest();
	CurrentHttpRequest->SetURL(Url);
	CurrentHttpRequest->SetVerb(TEXT("POST"));
	CurrentHttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	// Build request body
	TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();
	JsonObject->SetStringField(TEXT("client_id"), GameId);
	JsonObject->SetStringField(TEXT("device_code"), DeviceCode);
	JsonObject->SetStringField(TEXT("grant_type"), TEXT("urn:ietf:params:oauth:grant-type:device_code"));
	JsonObject->SetStringField(TEXT("code_verifier"), CodeVerifier);

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
	CurrentHttpRequest->SetContentAsString(JsonString);

	CurrentHttpRequest->OnProcessRequestComplete().BindUObject(
		this, &UPlayKitDeviceAuthFlow::HandleTokenResponse);
	CurrentHttpRequest->ProcessRequest();
}

void UPlayKitDeviceAuthFlow::HandleTokenResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!bWasSuccessful || !Response.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[DeviceAuth] Token poll failed - network error, will retry"));
		return; // Continue polling
	}

	const int32 ResponseCode = Response->GetResponseCode();
	UE_LOG(LogTemp, Log, TEXT("[DeviceAuth] Token response: HTTP %d"), ResponseCode);

	// Parse response
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
	if (!FJsonSerializer::Deserialize(Reader, JsonObject))
	{
		UE_LOG(LogTemp, Warning, TEXT("[DeviceAuth] Failed to parse token response"));
		return; // Continue polling
	}

	if (ResponseCode == 200)
	{
		// Success - got tokens
		AccessToken = JsonObject->GetStringField(TEXT("access_token"));
		RefreshToken = JsonObject->GetStringField(TEXT("refresh_token"));

		if (AccessToken.IsEmpty())
		{
			CompleteWithError(TEXT("INVALID_TOKEN"), TEXT("Received empty access token"));
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("[DeviceAuth] Access token received, exchanging for player token"));

		// Exchange for player token
		ExchangeForPlayerToken(AccessToken);
		return;
	}

	if (ResponseCode == 400)
	{
		// Check error type
		FString Error = JsonObject->GetStringField(TEXT("error"));

		if (Error == TEXT("authorization_pending"))
		{
			// User hasn't authorized yet - continue polling
			UE_LOG(LogTemp, Log, TEXT("[DeviceAuth] Authorization pending, continuing to poll"));
			return;
		}
		else if (Error == TEXT("slow_down"))
		{
			// Slow down polling
			PollingInterval += 5;
			UE_LOG(LogTemp, Log, TEXT("[DeviceAuth] Slowing down polling to %d seconds"), PollingInterval);
			return;
		}
		else if (Error == TEXT("expired_token"))
		{
			Cleanup();
			SetStatus(EDeviceAuthStatus::Expired);
			OnAuthError.Broadcast(TEXT("EXPIRED"), TEXT("Device code expired"));
			return;
		}
		else if (Error == TEXT("access_denied"))
		{
			Cleanup();
			SetStatus(EDeviceAuthStatus::Error);
			OnAuthError.Broadcast(TEXT("ACCESS_DENIED"), TEXT("User denied authorization"));
			return;
		}
	}

	// Other error
	FString ErrorMessage = JsonObject->GetStringField(TEXT("error_description"));
	if (ErrorMessage.IsEmpty())
	{
		ErrorMessage = Response->GetContentAsString();
	}
	CompleteWithError(TEXT("TOKEN_ERROR"), ErrorMessage);
}

void UPlayKitDeviceAuthFlow::ExchangeForPlayerToken(const FString& InAccessToken)
{
	const FString Url = BaseUrl / TEXT("api/external/exchange-jwt");

	CurrentHttpRequest = FHttpModule::Get().CreateRequest();
	CurrentHttpRequest->SetURL(Url);
	CurrentHttpRequest->SetVerb(TEXT("POST"));
	CurrentHttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	CurrentHttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *InAccessToken));

	// Build request body
	TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();
	JsonObject->SetStringField(TEXT("jwt"), InAccessToken);

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
	CurrentHttpRequest->SetContentAsString(JsonString);

	CurrentHttpRequest->OnProcessRequestComplete().BindUObject(
		this, &UPlayKitDeviceAuthFlow::HandlePlayerTokenResponse);
	CurrentHttpRequest->ProcessRequest();
}

void UPlayKitDeviceAuthFlow::HandlePlayerTokenResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!bWasSuccessful || !Response.IsValid())
	{
		CompleteWithError(TEXT("NETWORK_ERROR"), TEXT("Failed to exchange for player token"));
		return;
	}

	const int32 ResponseCode = Response->GetResponseCode();
	UE_LOG(LogTemp, Log, TEXT("[DeviceAuth] Player token response: HTTP %d"), ResponseCode);

	if (ResponseCode != 200)
	{
		CompleteWithError(TEXT("EXCHANGE_ERROR"), FString::Printf(TEXT("HTTP %d: %s"), ResponseCode, *Response->GetContentAsString()));
		return;
	}

	// Parse response
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
	if (!FJsonSerializer::Deserialize(Reader, JsonObject))
	{
		CompleteWithError(TEXT("PARSE_ERROR"), TEXT("Failed to parse player token response"));
		return;
	}

	// Build result
	FDeviceAuthResult Result;
	Result.bSuccess = true;
	Result.AccessToken = AccessToken;
	Result.RefreshToken = RefreshToken;
	Result.UserId = JsonObject->GetStringField(TEXT("userId"));
	Result.PlayerToken = JsonObject->GetStringField(TEXT("playerToken"));
	Result.ExpiresIn = JsonObject->GetIntegerField(TEXT("expiresIn"));

	CompleteWithSuccess(Result);
}

void UPlayKitDeviceAuthFlow::SetStatus(EDeviceAuthStatus NewStatus)
{
	if (Status != NewStatus)
	{
		EDeviceAuthStatus OldStatus = Status;
		Status = NewStatus;
		OnStatusChanged.Broadcast(OldStatus, NewStatus);
	}
}

void UPlayKitDeviceAuthFlow::CompleteWithError(const FString& ErrorCode, const FString& ErrorMessage)
{
	UE_LOG(LogTemp, Error, TEXT("[DeviceAuth] Error: %s - %s"), *ErrorCode, *ErrorMessage);
	Cleanup();
	SetStatus(EDeviceAuthStatus::Error);
	OnAuthError.Broadcast(ErrorCode, ErrorMessage);
}

void UPlayKitDeviceAuthFlow::CompleteWithSuccess(const FDeviceAuthResult& Result)
{
	UE_LOG(LogTemp, Log, TEXT("[DeviceAuth] Authorization successful! UserId: %s"), *Result.UserId);
	Cleanup();
	SetStatus(EDeviceAuthStatus::Success);
	OnAuthSuccess.Broadcast(Result);
}

void UPlayKitDeviceAuthFlow::Cleanup()
{
	// Cancel pending request
	if (CurrentHttpRequest.IsValid())
	{
		CurrentHttpRequest->CancelRequest();
		CurrentHttpRequest.Reset();
	}

	// Clear timers
	if (WorldPtr.IsValid())
	{
		UWorld* World = WorldPtr.Get();
		World->GetTimerManager().ClearTimer(PollingTimerHandle);
		World->GetTimerManager().ClearTimer(ExpirationTimerHandle);
	}

	// Clear state
	DeviceCode.Empty();
	UserCode.Empty();
	AuthUrl.Empty();
	CodeVerifier.Empty();
	CodeChallenge.Empty();
	AccessToken.Empty();
	RefreshToken.Empty();
}
