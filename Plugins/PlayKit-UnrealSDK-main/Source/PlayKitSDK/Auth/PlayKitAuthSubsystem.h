// Copyright PlayKit. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PlayKitAuthSubsystem.generated.h"


class IHttpRequest;


//////////////// Login  ////////////////
UENUM(Blueprintable)
enum class ERequestCodeStatus : uint8
{
	Success			UMETA(DisplayName = "Request Success"),
	InvalidEmail	UMETA(DisplayName = "Invalid Email Format"),
	InvalidPhone	UMETA(DisplayName = "Invalid Phone Format"),
	MissingParam	UMETA(DisplayName = "Missing Parameter"),
	NetworkError	UMETA(DisplayName = "Network Error"),
	TooMany			UMETA(DisplayName = "Too Many Requests"),
	UnknownError	UMETA(DisplayName = "Unknown Error")
};

UENUM(Blueprintable)
enum class ELoginType : uint8
{
	Email			UMETA(DisplayName = "Email"),
	Phone			UMETA(DisplayName = "Phone")
};

UENUM(Blueprintable)
enum class EVerifyCodeStatus : uint8
{
	GetPlayerToken	UMETA(DisplayName = "Player Token Received"),
	Success			UMETA(DisplayName = "Verification Success"),
	InvalidCode		UMETA(DisplayName = "Invalid Code"),
	Expired			UMETA(DisplayName = "Code Expired"),
	NetworkError	UMETA(DisplayName = "Network Error"),
	UnknownError	UMETA(DisplayName = "Unknown Error"),
	TooMany			UMETA(DisplayName = "Too Many Attempts")
};


// Request code completed delegate
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnRequestCodeCompleted, ERequestCodeStatus, Status);

// Verify cooldown timer delegate
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnVerifyCooldownTimer, int32, RemainingSeconds);

// Verify code completed delegate
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnVerifyCodeCompleted, EVerifyCodeStatus, Status);


//////////////// Player Client  ////////////////
UENUM(Blueprintable)
enum class EUserRole : uint8
{
	Player			UMETA(DisplayName = "Player"),
	Developer		UMETA(DisplayName = "Developer")
};

USTRUCT(BlueprintType)
struct FPlayerTokenInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString		UserId;
	UPROPERTY(BlueprintReadOnly)
	FString		PlayerToken;
	UPROPERTY(BlueprintReadOnly)
	FString		ExpiresAt;


public:
	friend FArchive& operator<<(FArchive& _Ar, FPlayerTokenInfo& _UserTokenInfo)
	{
		_Ar << _UserTokenInfo.UserId;
		_Ar << _UserTokenInfo.PlayerToken;
		_Ar << _UserTokenInfo.ExpiresAt;
		return _Ar;
	}
};



USTRUCT(BlueprintType)
struct FUserClientInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString		UserId;
	UPROPERTY(BlueprintReadOnly)
	FString		GlobalToken;
	UPROPERTY(BlueprintReadOnly)
	FString		UserName;
	UPROPERTY(BlueprintReadOnly)
	EUserRole	Role;
	UPROPERTY(BlueprintReadOnly)
	UTexture2D*	Avatar;
};

// Need re-login delegate
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNeedReLogin);

// Get client info completed delegate
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGetClientInfoCompleted, FUserClientInfo, UserClientInfo);

///////////////////////////////////////////


UCLASS(BlueprintType)
class PLAYKITSDK_API UPlayKitAuthSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()


public:
	virtual void BeginDestroy() override;

public:
	static UPlayKitAuthSubsystem* Get();


public:

	//////////////// Login (Deprecated) ////////////////
	UFUNCTION(BlueprintCallable, Category="PlayKit|Auth|Deprecated",
		meta=(DisplayName="Request Code (Deprecated)", DeprecatedFunction, DeprecationMessage="This function is deprecated. Use Device Auth Flow instead."))
	static void RequestCode(FString _Address, ELoginType _LoginType, FOnRequestCodeCompleted _OnRequestCodeCompleted);

	UFUNCTION(BlueprintCallable, Category="PlayKit|Auth|Deprecated",
		meta=(DisplayName="Verify Code (Deprecated)", DeprecatedFunction, DeprecationMessage="This function is deprecated. Use Device Auth Flow instead."))
	static void VerifyCode(FString _Code,
		FOnVerifyCodeCompleted _OnVerifyCodeCompleted);

	UFUNCTION(meta=(DeprecatedFunction, DeprecationMessage="This function is deprecated. Use Device Auth Flow instead."))
	static void GetPlayerToken(const FString& _GlobalToken, FOnVerifyCodeCompleted _OnVerifyCodeCompleted);

	UFUNCTION(BlueprintCallable, Category="PlayKit|Auth|Deprecated",
		meta=(DisplayName="Start Verify Cooldown Timer (Deprecated)", DeprecatedFunction, DeprecationMessage="This function is deprecated. Use Device Auth Flow instead."))
	static void StartVerifyCooldownTimer(int32 _Seconds, FOnVerifyCooldownTimer _OnVerifyCooldownTimer);

	UFUNCTION(BlueprintCallable, Category="PlayKit|Auth|Deprecated",
		meta=(DisplayName="Clear Verify Cooldown Timer (Deprecated)", DeprecatedFunction, DeprecationMessage="This function is deprecated. Use Device Auth Flow instead."))
	static void ClearVerifyCooldownTimer();
	//////////////// Client  ////////////////
	void SaveToken(FPlayerTokenInfo _PlayerTokenInfo) const;

	UFUNCTION(BlueprintCallable, Category="PlayKit|Auth",
		meta=(DisplayName="Get Player Token Info"))
	static bool GetToken(FPlayerTokenInfo& _OutPlayerTokenInfo, int32 _HoursEarly = 6);

public:
	//////////////// Login  ////////////////

	// Verify cooldown timer handle
	FTimerHandle VerifyCooldownTimerHandle;

	// Request code HttpRequest
	TSharedPtr<IHttpRequest> RequestCodeHttpRequest;

	// Verify code HttpRequest
	TSharedPtr<IHttpRequest> VerifyCodeHttpRequest;

	// Get player token HttpRequest
	TSharedPtr<IHttpRequest> GetPlayerTokenHttpRequest;

	// Get client info completed delegate
	UPROPERTY(BlueprintAssignable, Category="PlayKit|Auth")
	FOnGetClientInfoCompleted OnGetClientInfoCompleted;

	//////////////// Client  ////////////////
	// Need re-login delegate
	UPROPERTY(BlueprintAssignable, Category="PlayKit|Auth")
	FOnNeedReLogin OnNeedReLogin;

public:
	//////////////// Login  ////////////////
	// Request code session ID
	FString RequestCodeSessionId;

	UPROPERTY(BlueprintReadOnly, Category="PlayKit|URLs")
	FString BaseURL = FString("https://playkit.ai");

	//////////////// Client  ////////////////
	const FString PlayerTokenSaveFilePath = FPaths::ProjectSavedDir() / TEXT("PlayKit/PlayerToken.dat");
	FUserClientInfo UserClientInfo;

};
