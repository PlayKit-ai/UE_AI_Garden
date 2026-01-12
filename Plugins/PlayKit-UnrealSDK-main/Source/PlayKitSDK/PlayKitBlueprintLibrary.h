// Copyright PlayKit. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PlayKitTypes.h"
#include "PlayKitBlueprintLibrary.generated.h"

class UPlayKitPlayerClient;
class UPlayKitNPCClient;

/**
 * PlayKit Blueprint Function Library
 * Provides static utility functions for PlayKit SDK.
 *
 * Usage:
 * 1. Ensure your game is configured in Project Settings > Plugins > PlayKit SDK
 * 2. Add PlayKit components (ChatClient, ImageClient, STTClient, NPCClient) to your Actors
 * 3. Configure component properties in the Details panel
 * 4. Bind to component events using the "+" button
 * 5. Call component methods to trigger requests
 *
 * For player info and credits, use GetPlayerClient() to access the PlayerClient subsystem.
 */
UCLASS()
class PLAYKITSDK_API UPlayKitBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	//========== SDK State ==========//

	/** Check if the SDK is properly configured and ready to use */
	UFUNCTION(BlueprintPure, Category="PlayKit|SDK", meta=(DisplayName="Is PlayKit Ready"))
	static bool IsReady();

	/** Get the current SDK version */
	UFUNCTION(BlueprintPure, Category="PlayKit|SDK", meta=(DisplayName="Get SDK Version"))
	static FString GetVersion();

	//========== Player Client ==========//

	/**
	 * Get the Player Client subsystem for user info and credits management.
	 * @param WorldContextObject Any world context object
	 * @return The Player Client subsystem instance
	 */
	UFUNCTION(BlueprintCallable, Category="PlayKit|Player", meta=(DisplayName="Get Player Client", WorldContext="WorldContextObject"))
	static UPlayKitPlayerClient* GetPlayerClient(const UObject* WorldContextObject);

	//========== NPC Setup ==========//

	/**
	 * Initialize an NPC Client component with the SDK.
	 * Call this after adding a PlayKitNPCClient component to your actor.
	 * @param NPCClient The NPC Client component to initialize
	 * @param ModelName Optional model override. If empty, uses default from settings.
	 */
	UFUNCTION(BlueprintCallable, Category="PlayKit|NPC", meta=(DisplayName="Setup NPC"))
	static void SetupNPC(UPlayKitNPCClient* NPCClient, const FString& ModelName = TEXT(""));

	//========== Utility ==========//

	/** Get the current authentication token (developer or player token) */
	UFUNCTION(BlueprintPure, Category="PlayKit|Auth", meta=(DisplayName="Get Auth Token"))
	static FString GetAuthToken();

	/** Check if user is authenticated */
	UFUNCTION(BlueprintPure, Category="PlayKit|Auth", meta=(DisplayName="Is Authenticated"))
	static bool IsAuthenticated();

	/** Get the configured Game ID */
	UFUNCTION(BlueprintPure, Category="PlayKit|Settings", meta=(DisplayName="Get Game ID"))
	static FString GetGameId();

	/** Get the API base URL */
	UFUNCTION(BlueprintPure, Category="PlayKit|Settings", meta=(DisplayName="Get Base URL"))
	static FString GetBaseUrl();
};
