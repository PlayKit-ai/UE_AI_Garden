// Copyright PlayKit. All Rights Reserved.


#include "PlayKitAuthSubsystem.h"

#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/BufferArchive.h"


void UPlayKitAuthSubsystem::BeginDestroy()
{
	Super::BeginDestroy();

	// Destroy all requests
	if(RequestCodeHttpRequest.IsValid())
	{
		RequestCodeHttpRequest->CancelRequest();
		RequestCodeHttpRequest.Reset();
	}
}

UPlayKitAuthSubsystem* UPlayKitAuthSubsystem::Get()
{
	return
		GEngine->GameViewport->GetGameInstance()->GetSubsystem<UPlayKitAuthSubsystem>();
}

//-----------------------------------------
// Purpose: Send verification code
//-----------------------------------------
void UPlayKitAuthSubsystem::RequestCode(const FString _Address, ELoginType _LoginType,
                                      FOnRequestCodeCompleted _OnRequestCodeCompleted)
{
	UPlayKitAuthSubsystem* subsystem =
		GEngine->GameViewport->GetGameInstance()->GetSubsystem<UPlayKitAuthSubsystem>();

	check(subsystem);

	if(subsystem->RequestCodeHttpRequest.IsValid())
	{
		const EHttpRequestStatus::Type status =
			subsystem->RequestCodeHttpRequest->GetStatus();

		// If processing previous request, do nothing
		if(status == EHttpRequestStatus::Processing)
			return ;
	}


	const FString url = subsystem->BaseURL / "api/auth/send-code";


	///////////////// Set header /////////////////

	subsystem->RequestCodeHttpRequest = FHttpModule::Get().CreateRequest();


	subsystem->RequestCodeHttpRequest->SetURL(url);
	subsystem->RequestCodeHttpRequest->SetVerb("POST");
	subsystem->RequestCodeHttpRequest->SetHeader("Content-Type", "application/json");

	///////////////// Set body /////////////////

	TSharedPtr<FJsonObject> jsonObject = MakeShared<FJsonObject>();
	jsonObject->SetStringField("identifier", _Address);
	jsonObject->SetStringField("type",
	                           _LoginType == ELoginType::Email ? "email" : "phone");
	FString jsonString;
	const TSharedRef<TJsonWriter<>> writer = TJsonWriterFactory<>::Create(&jsonString);
	FJsonSerializer::Serialize(jsonObject.ToSharedRef(), writer);
	subsystem->RequestCodeHttpRequest->SetContentAsString(jsonString);


	///////////////// Set callback delegate /////////////////

	subsystem->RequestCodeHttpRequest->OnProcessRequestComplete().BindLambda(
		[WeakThis = TWeakObjectPtr(subsystem),_OnRequestCodeCompleted,_LoginType]
		(FHttpRequestPtr _Request, const FHttpResponsePtr& _Response, bool _bWasSuccessful)
		{
			if(!WeakThis.IsValid())
				return ;

			// Network error
			if(!_bWasSuccessful || !_Response.IsValid())
			{
				(void)_OnRequestCodeCompleted.ExecuteIfBound(ERequestCodeStatus::NetworkError);
				return;
			}

			// Parse response
			TSharedPtr<FJsonObject> jsonObject;
			TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(_Response->GetContentAsString());

			if(!FJsonSerializer::Deserialize(reader, jsonObject))
			{
				(void)_OnRequestCodeCompleted.ExecuteIfBound(ERequestCodeStatus::UnknownError);
				return;
			}

			const int32 responseCode = _Response->GetResponseCode();

			// If 400 or 500
			if(responseCode == 400 || responseCode == 500)
			{
				const FString code = jsonObject->GetStringField(TEXT("code"));

				// Invalid phone or email format
				if(code.Equals("VALIDATION_ERROR"))
				{
					(void)_OnRequestCodeCompleted.ExecuteIfBound(
						_LoginType == ELoginType::Email?
						ERequestCodeStatus::InvalidEmail : ERequestCodeStatus::InvalidPhone);
					return ;
				}
				// Usually also invalid phone or email format
				if(code.Equals("PROVIDER_ERROR"))
				{
					(void)_OnRequestCodeCompleted.ExecuteIfBound(
						_LoginType == ELoginType::Email?
						ERequestCodeStatus::InvalidEmail : ERequestCodeStatus::InvalidPhone);
					return ;
				}
				// Missing phone or email
				if(code.Equals("MISSING_PARAMETERS"))
				{
					(void)_OnRequestCodeCompleted.ExecuteIfBound(
						ERequestCodeStatus::MissingParam);
					return ;
				}
			}
			// 429 too many requests
			if(responseCode == 429)
			{
				(void)_OnRequestCodeCompleted.ExecuteIfBound(ERequestCodeStatus::TooMany);
				return ;
			}
			// If 200
			if(responseCode == 200)
			{
				WeakThis.Get()->RequestCodeSessionId = jsonObject->GetStringField(TEXT("sessionId"));

				if(WeakThis.Get()->RequestCodeSessionId.IsEmpty())
				{
					(void)_OnRequestCodeCompleted.ExecuteIfBound(ERequestCodeStatus::NetworkError);
					return ;
				}

				// Success, waiting for verification code
				(void)_OnRequestCodeCompleted.ExecuteIfBound(ERequestCodeStatus::Success);
				return ;
			}

			// Unknown error
			(void)_OnRequestCodeCompleted.ExecuteIfBound(ERequestCodeStatus::UnknownError);
			return ;
		});

	////////////////// Send request //////////////////
	subsystem->RequestCodeHttpRequest->ProcessRequest();
}


//-----------------------------------------
// Purpose: Verify code
//-----------------------------------------
void UPlayKitAuthSubsystem::VerifyCode(const FString _Code, FOnVerifyCodeCompleted _OnVerifyCodeCompleted)
{
	UPlayKitAuthSubsystem* subsystem =
		GEngine->GameViewport->GetGameInstance()->GetSubsystem<UPlayKitAuthSubsystem>();

	check(subsystem);

	if(subsystem->VerifyCodeHttpRequest.IsValid())
	{
		const EHttpRequestStatus::Type status =
			subsystem->VerifyCodeHttpRequest->GetStatus();

		// If processing previous request, do nothing
		if(status == EHttpRequestStatus::Processing)
			return ;
	}

	const FString url = subsystem->BaseURL / "/api/auth/verify-code";

	///////////////// Set header /////////////////
	subsystem->VerifyCodeHttpRequest = FHttpModule::Get().CreateRequest();

	subsystem->VerifyCodeHttpRequest->SetURL(url);
	subsystem->VerifyCodeHttpRequest->SetVerb("POST");
	subsystem->VerifyCodeHttpRequest->SetHeader("Content-Type", "application/json");
	///////////////// Set body /////////////////

	TSharedPtr<FJsonObject> jsonObject = MakeShared<FJsonObject>();
	jsonObject->SetStringField("sessionId", subsystem->RequestCodeSessionId);
	jsonObject->SetStringField("code", _Code);

	FString jsonString;
	const TSharedRef<TJsonWriter<>> writer = TJsonWriterFactory<>::Create(&jsonString);
	FJsonSerializer::Serialize(jsonObject.ToSharedRef(), writer);
	subsystem->VerifyCodeHttpRequest->SetContentAsString(jsonString);

	///////////////// Set callback delegate /////////////////

	subsystem->VerifyCodeHttpRequest->OnProcessRequestComplete().BindLambda(
		[WeakThis = TWeakObjectPtr(subsystem),_OnVerifyCodeCompleted]
		(FHttpRequestPtr _Request, const FHttpResponsePtr& _Response, bool _bWasSuccessful)
		{
			if(!WeakThis.IsValid())
				return ;

			// Network error
			if(!_bWasSuccessful || !_Response.IsValid())
			{
				(void)_OnVerifyCodeCompleted.ExecuteIfBound(EVerifyCodeStatus::NetworkError);
				return;
			}

			// Parse response
			TSharedPtr<FJsonObject> newJsonObject;
			const TSharedRef<TJsonReader<>> reader =
				TJsonReaderFactory<>::Create(_Response->GetContentAsString());

			if(!FJsonSerializer::Deserialize(reader, newJsonObject))
			{
				(void)_OnVerifyCodeCompleted.ExecuteIfBound(EVerifyCodeStatus::UnknownError);
				return;
			}

			const int32 responseCode = _Response->GetResponseCode();

			// If 400 - invalid code
			if(responseCode == 400 )
			{
				FString message = newJsonObject->GetStringField(TEXT("message"));

				// Invalid verification code
				if(message.Equals("Invalid verification code"))
				{
					(void)_OnVerifyCodeCompleted.ExecuteIfBound(
						EVerifyCodeStatus::InvalidCode);
					return ;
				}

				// Code expired
				if(message.Contains("expired"))
				{
					(void)_OnVerifyCodeCompleted.ExecuteIfBound(
						EVerifyCodeStatus::Expired);
					return ;
				}

				// Other cases - invalid code
				(void)_OnVerifyCodeCompleted.ExecuteIfBound(
					EVerifyCodeStatus::InvalidCode);
				return ;
			}

			// 429 too many requests
			if(responseCode == 429)
			{
				(void)_OnVerifyCodeCompleted.ExecuteIfBound(
					EVerifyCodeStatus::TooMany);
				return ;
			}
			// If 200
			if(responseCode == 200)
			{
				FString userId = newJsonObject->GetStringField(TEXT("userId"));

				FString globalToken = newJsonObject->GetStringField(TEXT("globalToken"));

				(void)_OnVerifyCodeCompleted.ExecuteIfBound(
					EVerifyCodeStatus::Success);
				// Get player token
				WeakThis->GetPlayerToken(globalToken, _OnVerifyCodeCompleted);
				return ;
			}

			// Unknown error
			(void)_OnVerifyCodeCompleted.ExecuteIfBound(EVerifyCodeStatus::UnknownError);
			return ;
		});

	////////////////// Send request //////////////////
	subsystem->VerifyCodeHttpRequest->ProcessRequest();
}

//----------------------------------------
// Purpose: Get player token
//----------------------------------------
void UPlayKitAuthSubsystem::GetPlayerToken(const FString& _GlobalToken, FOnVerifyCodeCompleted _OnVerifyCodeCompleted)
{
	UPlayKitAuthSubsystem* subsystem =
		GEngine->GameViewport->GetGameInstance()->GetSubsystem<UPlayKitAuthSubsystem>();

	check(subsystem);

	if(subsystem->GetPlayerTokenHttpRequest.IsValid())
	{
		const EHttpRequestStatus::Type status =
			subsystem->GetPlayerTokenHttpRequest->GetStatus();

		// If processing previous request, do nothing
		if(status == EHttpRequestStatus::Processing)
		{
			(void)_OnVerifyCodeCompleted.ExecuteIfBound(EVerifyCodeStatus::TooMany);
			return;
		}

	}

	const FString url = subsystem->BaseURL / "/api/external/exchange-jwt";

	///////////////// Set header /////////////////
	subsystem->GetPlayerTokenHttpRequest = FHttpModule::Get().CreateRequest();

	subsystem->GetPlayerTokenHttpRequest->SetURL(url);
	subsystem->GetPlayerTokenHttpRequest->SetVerb("POST");

	FString authorization = "Bearer " + _GlobalToken;
	subsystem->GetPlayerTokenHttpRequest->SetHeader("Authorization", authorization);
	subsystem->GetPlayerTokenHttpRequest->SetHeader("Content-Type", "application/json");
	///////////////// Set body /////////////////
	TSharedPtr<FJsonObject> jsonObject = MakeShared<FJsonObject>();
	jsonObject->SetStringField("jwt", _GlobalToken);

	FString jsonString;
	const TSharedRef<TJsonWriter<>> writer = TJsonWriterFactory<>::Create(&jsonString);
	FJsonSerializer::Serialize(jsonObject.ToSharedRef(), writer);
	subsystem->GetPlayerTokenHttpRequest->SetContentAsString(jsonString);

	///////////////// Set callback delegate /////////////////
	subsystem->GetPlayerTokenHttpRequest->OnProcessRequestComplete().BindLambda(
		[WeakThis = TWeakObjectPtr(subsystem),_OnVerifyCodeCompleted]
		(FHttpRequestPtr _Request, const FHttpResponsePtr& _Response, bool _bWasSuccessful)
		{
			if(!WeakThis.IsValid())
				return ;
			// Network error
			if(!_bWasSuccessful || !_Response.IsValid())
			{
				(void)_OnVerifyCodeCompleted.ExecuteIfBound(EVerifyCodeStatus::NetworkError);
				return;
			}
			// Parse response
			TSharedPtr<FJsonObject> newJsonObject;
			const TSharedRef<TJsonReader<>> reader =
				TJsonReaderFactory<>::Create(_Response->GetContentAsString());
			if(!FJsonSerializer::Deserialize(reader, newJsonObject))
			{
				(void)_OnVerifyCodeCompleted.ExecuteIfBound(EVerifyCodeStatus::UnknownError);
				return;
			}
			const int32 responseCode = _Response->GetResponseCode();
			// If not 200, verification failed
			if(responseCode != 200)
			{
				(void)_OnVerifyCodeCompleted.ExecuteIfBound(EVerifyCodeStatus::UnknownError);
				return ;
			}
			// If 200, success
			FPlayerTokenInfo playerTokenInfo;
			playerTokenInfo.UserId		= newJsonObject->GetStringField(TEXT("userId"));
			playerTokenInfo.PlayerToken	= newJsonObject->GetStringField(TEXT("playerToken"));
			playerTokenInfo.ExpiresAt	= newJsonObject->GetStringField(TEXT("expiresAt"));
			// Save token
			WeakThis->SaveToken(playerTokenInfo);

			(void)_OnVerifyCodeCompleted.ExecuteIfBound(EVerifyCodeStatus::GetPlayerToken);
		});
	subsystem->GetPlayerTokenHttpRequest->ProcessRequest();
}

//---------------------------------------------------
// Purpose: Start verify cooldown timer
//---------------------------------------------------
void UPlayKitAuthSubsystem::StartVerifyCooldownTimer(int32 _Seconds, FOnVerifyCooldownTimer _OnVerifyCooldownTimer)
{
	UPlayKitAuthSubsystem* subsystem = Get();
	if (!subsystem)
		return;
	ClearVerifyCooldownTimer();

	int32 remainSeconds = _Seconds;
	(void)_OnVerifyCooldownTimer.ExecuteIfBound(remainSeconds);


	GEngine->GameViewport->GetWorld()->GetTimerManager().SetTimer(
		subsystem->VerifyCooldownTimerHandle,
		[WeakThis = TWeakObjectPtr(subsystem),_OnVerifyCooldownTimer, remainSeconds]() mutable
		{
			if(!WeakThis.IsValid())
				return ;

			remainSeconds--;
			(void)_OnVerifyCooldownTimer.ExecuteIfBound(remainSeconds);

			// Time's up - stop timer
			if (remainSeconds <= 0)
			{
				WeakThis->ClearVerifyCooldownTimer();
			}

		},
		1.0f,
		true);
}


//---------------------------------------------------
// Purpose: Stop verify cooldown timer
//---------------------------------------------------
void UPlayKitAuthSubsystem::ClearVerifyCooldownTimer()
{
	UPlayKitAuthSubsystem* subsystem = Get();
	if (!subsystem)
		return;
	GEngine->GameViewport->GetWorld()->GetTimerManager().ClearTimer(subsystem->VerifyCooldownTimerHandle);

}

//-----------------------------------------------
// Purpose: Save token
//-----------------------------------------------
void UPlayKitAuthSubsystem::SaveToken(FPlayerTokenInfo _PlayerTokenInfo) const
{
	FBufferArchive toBinary;
	toBinary << _PlayerTokenInfo;

	TArray<uint8> encryptedData;
	encryptedData.Append(toBinary.GetData(), toBinary.Num());


	constexpr uint8 key[32] = {
		'P','L','a','Y','k','I','t','S','D','k','F','o','R','u','N','r',
		'E','a','L','e','N','g','I','n','E','2','0','2','5','U','E','5'
	};

	// Pad to multiple of 16 (PKCS7)
	int32 padding = 16 - (encryptedData.Num() % 16);
	if (padding != 0 && padding != 16)
	{
		for (int32 i = 0; i < padding; i++)
			encryptedData.Add(static_cast<uint8>(padding));
	}


	FAES::EncryptData(encryptedData.GetData(), encryptedData.Num(), key, 32);

	FFileHelper::SaveArrayToFile(encryptedData, *PlayerTokenSaveFilePath);

	// Clean up memory
	toBinary.FlushCache();
	toBinary.Empty();
}

//------------------------------------------------
// Purpose: Get token
//------------------------------------------------
bool UPlayKitAuthSubsystem::GetToken(FPlayerTokenInfo& _OutPlayerTokenInfo, int32 _HoursEarly)
{
	UPlayKitAuthSubsystem* subsystem = Get();
	if (!subsystem)
		return false;


	// If file doesn't exist, return false
	if(!IFileManager::Get().FileExists(*(subsystem->PlayerTokenSaveFilePath)))
		return false;
	// Read file
	TArray<uint8> binaryData;
	bool bIsLoaded =
		FFileHelper::LoadFileToArray(binaryData, *(subsystem->PlayerTokenSaveFilePath));

	// File cannot be read
	if (!bIsLoaded)
		return false;



	constexpr uint8 key[32] = {
		'P','L','a','Y','k','I','t','S','D','k','F','o','R','u','N','r',
		'E','a','L','e','N','g','I','n','E','2','0','2','5','U','E','5'
	};
	if (binaryData.Num() > 0)
	{
		uint8 padding = binaryData.Last();
		if (padding > 0 && padding <= 16 && padding <= binaryData.Num())
		{
			binaryData.RemoveAt(binaryData.Num() - padding, padding, EAllowShrinking::No);
		}
	}


	FAES::DecryptData(binaryData.GetData(), binaryData.Num(), key, 32);



	FMemoryReader fromBinary(binaryData, true);
	fromBinary.Seek(0);
	FPlayerTokenInfo playerTokenInfo;
	fromBinary << playerTokenInfo;

	fromBinary.FlushCache();
	fromBinary.Close();

	// Check if expired
	FString expiresAt = playerTokenInfo.ExpiresAt;

	if(playerTokenInfo.ExpiresAt.IsEmpty())
		return false;

	FDateTime expireTime;
	// Cannot parse - treat as expired
	if (!FDateTime::ParseIso8601(*expiresAt, expireTime))
	{
		return false;
	}

	// Expire 6 hours early
	FDateTime adjustedExpireTime =
		expireTime - FTimespan::FromHours(_HoursEarly);

	FDateTime nowTime = FDateTime::UtcNow();

	// Expired
	if(nowTime > adjustedExpireTime)
		return false;

	_OutPlayerTokenInfo = playerTokenInfo;
	return true;
}
