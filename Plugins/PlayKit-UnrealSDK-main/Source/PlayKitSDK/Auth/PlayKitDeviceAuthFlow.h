// Copyright PlayKit. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Interfaces/IHttpRequest.h"
#include "PlayKitDeviceAuthFlow.generated.h"

/**
 * Device Auth Flow Status
 */
UENUM(BlueprintType)
enum class EDeviceAuthStatus : uint8
{
	Idle				UMETA(DisplayName = "Idle"),
	Pending				UMETA(DisplayName = "Pending - Waiting for user authorization"),
	Polling				UMETA(DisplayName = "Polling - Checking authorization status"),
	Success				UMETA(DisplayName = "Success - Authorization completed"),
	Expired				UMETA(DisplayName = "Expired - Device code expired"),
	Cancelled			UMETA(DisplayName = "Cancelled - User cancelled"),
	Error				UMETA(DisplayName = "Error - Authorization failed")
};

/**
 * Device Auth Result Structure
 */
USTRUCT(BlueprintType)
struct FDeviceAuthResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly)
	FString AccessToken;

	UPROPERTY(BlueprintReadOnly)
	FString RefreshToken;

	UPROPERTY(BlueprintReadOnly)
	FString UserId;

	UPROPERTY(BlueprintReadOnly)
	FString PlayerToken;

	UPROPERTY(BlueprintReadOnly)
	int32 ExpiresIn = 0;

	UPROPERTY(BlueprintReadOnly)
	FString ErrorMessage;
};

// Delegate: Auth status changed
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDeviceAuthStatusChanged, EDeviceAuthStatus, OldStatus, EDeviceAuthStatus, NewStatus);

// Delegate: Auth success
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDeviceAuthSuccess, FDeviceAuthResult, Result);

// Delegate: Auth error
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDeviceAuthError, FString, ErrorCode, FString, ErrorMessage);

// Delegate: Auth URL ready (for displaying to user)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDeviceAuthUrlReady, FString, AuthUrl, FString, UserCode);

/**
 * PlayKit Device Auth Flow
 * Implements PKCE-based device authorization for desktop/console applications
 */
UCLASS(BlueprintType, Blueprintable)
class PLAYKITSDK_API UPlayKitDeviceAuthFlow : public UObject
{
	GENERATED_BODY()

public:
	UPlayKitDeviceAuthFlow();

	/**
	 * Start the device authorization flow
	 * @param GameId - The game ID for this application
	 * @param Scope - Authorization scope (default: "player:play")
	 */
	UFUNCTION(BlueprintCallable, Category="PlayKit|DeviceAuth")
	void StartAuthFlow(const FString& GameId, const FString& Scope = TEXT("player:play"));

	/**
	 * Cancel the ongoing authorization flow
	 */
	UFUNCTION(BlueprintCallable, Category="PlayKit|DeviceAuth")
	void CancelAuthFlow();

	/**
	 * Get the current status of the auth flow
	 */
	UFUNCTION(BlueprintPure, Category="PlayKit|DeviceAuth")
	EDeviceAuthStatus GetStatus() const { return Status; }

	/**
	 * Get the authorization URL (valid after OnAuthUrlReady)
	 */
	UFUNCTION(BlueprintPure, Category="PlayKit|DeviceAuth")
	FString GetAuthUrl() const { return AuthUrl; }

	/**
	 * Get the user code (valid after OnAuthUrlReady)
	 */
	UFUNCTION(BlueprintPure, Category="PlayKit|DeviceAuth")
	FString GetUserCode() const { return UserCode; }

	/**
	 * Check if the flow is currently active
	 */
	UFUNCTION(BlueprintPure, Category="PlayKit|DeviceAuth")
	bool IsActive() const { return Status == EDeviceAuthStatus::Pending || Status == EDeviceAuthStatus::Polling; }

public:
	// Event: Status changed
	UPROPERTY(BlueprintAssignable, Category="PlayKit|DeviceAuth")
	FOnDeviceAuthStatusChanged OnStatusChanged;

	// Event: Authorization successful
	UPROPERTY(BlueprintAssignable, Category="PlayKit|DeviceAuth")
	FOnDeviceAuthSuccess OnAuthSuccess;

	// Event: Authorization failed
	UPROPERTY(BlueprintAssignable, Category="PlayKit|DeviceAuth")
	FOnDeviceAuthError OnAuthError;

	// Event: Auth URL ready for display
	UPROPERTY(BlueprintAssignable, Category="PlayKit|DeviceAuth")
	FOnDeviceAuthUrlReady OnAuthUrlReady;

private:
	// PKCE helper functions
	FString GenerateCodeVerifier();
	FString GenerateCodeChallenge(const FString& CodeVerifier);
	FString Base64UrlEncode(const TArray<uint8>& Data);

	// HTTP request handlers
	void RequestDeviceCode();
	void HandleDeviceCodeResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void StartPolling();
	void PollForToken();
	void HandleTokenResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void ExchangeForPlayerToken(const FString& AccessToken);
	void HandlePlayerTokenResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	// Status update
	void SetStatus(EDeviceAuthStatus NewStatus);
	void CompleteWithError(const FString& ErrorCode, const FString& ErrorMessage);
	void CompleteWithSuccess(const FDeviceAuthResult& Result);

	// Cleanup
	void Cleanup();

private:
	UPROPERTY()
	EDeviceAuthStatus Status = EDeviceAuthStatus::Idle;

	// Configuration
	FString GameId;
	FString Scope;
	FString BaseUrl = TEXT("https://api.playkit.ai");

	// PKCE
	FString CodeVerifier;
	FString CodeChallenge;

	// Device code response
	FString DeviceCode;
	FString UserCode;
	FString AuthUrl;
	int32 PollingInterval = 5; // seconds
	int32 ExpiresIn = 300; // seconds

	// Token response
	FString AccessToken;
	FString RefreshToken;

	// HTTP requests
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> CurrentHttpRequest;

	// Timer
	FTimerHandle PollingTimerHandle;
	FTimerHandle ExpirationTimerHandle;

	// World reference for timers
	TWeakObjectPtr<UWorld> WorldPtr;
};
