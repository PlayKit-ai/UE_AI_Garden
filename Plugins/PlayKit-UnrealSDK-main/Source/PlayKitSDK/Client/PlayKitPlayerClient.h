// Copyright PlayKit. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/IHttpRequest.h"
#include "PlayKitTypes.h"
#include "PlayKitPlayerClient.generated.h"

/**
 * PlayKit Player Client Subsystem
 * Manages player information, credits, and authentication.
 *
 * Features:
 * - Get player information
 * - Check and refresh credits
 * - Set player nickname
 * - JWT token exchange
 *
 * Usage:
 * UPlayKitPlayerClient* PlayerClient = UPlayKitPlayerClient::Get(this);
 * PlayerClient->OnPlayerInfoUpdated.AddDynamic(this, &AMyActor::HandlePlayerInfo);
 * PlayerClient->GetPlayerInfo();
 */
UCLASS()
class PLAYKITSDK_API UPlayKitPlayerClient : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UPlayKitPlayerClient();

	// Subsystem lifecycle
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Get the subsystem instance */
	UFUNCTION(BlueprintCallable, Category="PlayKit|Player", meta=(WorldContext="WorldContextObject"))
	static UPlayKitPlayerClient* Get(const UObject* WorldContextObject);

	//========== Events ==========//

	/** Fired when player info is updated */
	UPROPERTY(BlueprintAssignable, Category="PlayKit|Player")
	FOnPlayerInfoUpdated OnPlayerInfoUpdated;

	/** Fired when player token is received */
	UPROPERTY(BlueprintAssignable, Category="PlayKit|Player")
	FOnPlayerTokenReceived OnPlayerTokenReceived;

	/** Fired when daily credits are refreshed */
	UPROPERTY(BlueprintAssignable, Category="PlayKit|Player")
	FOnDailyCreditsRefreshed OnDailyCreditsRefreshed;

	/** Fired on error */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerError, const FString&, ErrorMessage);
	UPROPERTY(BlueprintAssignable, Category="PlayKit|Player")
	FOnPlayerError OnError;

	//========== Properties ==========//

	/** Check if player has a valid token */
	UFUNCTION(BlueprintPure, Category="PlayKit|Player")
	bool HasValidToken() const;

	/** Get cached player info */
	UFUNCTION(BlueprintPure, Category="PlayKit|Player")
	FPlayKitPlayerInfo GetCachedPlayerInfo() const { return CachedPlayerInfo; }

	/** Get player's current credits */
	UFUNCTION(BlueprintPure, Category="PlayKit|Player")
	float GetCredits() const { return CachedPlayerInfo.Credits; }

	/** Get player's nickname */
	UFUNCTION(BlueprintPure, Category="PlayKit|Player")
	FString GetNickname() const { return CachedPlayerInfo.Nickname; }

	//========== Player Info ==========//

	/**
	 * Get player information from the server.
	 * Result will be broadcast via OnPlayerInfoUpdated.
	 */
	UFUNCTION(BlueprintCallable, Category="PlayKit|Player", meta=(DisplayName="Get Player Info"))
	void GetPlayerInfo();

	/**
	 * Set the player's nickname.
	 * @param Nickname The nickname to set (1-16 characters)
	 */
	UFUNCTION(BlueprintCallable, Category="PlayKit|Player", meta=(DisplayName="Set Nickname"))
	void SetNickname(const FString& Nickname);

	//========== Credits ==========//

	/**
	 * Refresh daily credits.
	 * Grants credits when balance is below threshold, once per day.
	 */
	UFUNCTION(BlueprintCallable, Category="PlayKit|Player", meta=(DisplayName="Refresh Daily Credits"))
	void RefreshDailyCredits();

	//========== Authentication ==========//

	/**
	 * Exchange a JWT token for a player token.
	 * @param JWT The JWT token from your game's auth system
	 */
	UFUNCTION(BlueprintCallable, Category="PlayKit|Player", meta=(DisplayName="Exchange JWT"))
	void ExchangeJWT(const FString& JWT);

	/**
	 * Set the player token directly.
	 * @param Token The player token
	 */
	UFUNCTION(BlueprintCallable, Category="PlayKit|Player", meta=(DisplayName="Set Player Token"))
	void SetPlayerToken(const FString& Token);

	/** Clear the stored player token */
	UFUNCTION(BlueprintCallable, Category="PlayKit|Player", meta=(DisplayName="Clear Player Token"))
	void ClearPlayerToken();

private:
	void HandlePlayerInfoResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void HandleSetNicknameResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void HandleDailyCreditsResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void HandleJWTExchangeResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> CreateAuthenticatedRequest(const FString& Url, const FString& Verb = TEXT("GET"));
	void BroadcastError(const FString& ErrorMessage);

private:
	FPlayKitPlayerInfo CachedPlayerInfo;
	FString CurrentJWT;

	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> CurrentRequest;
};
