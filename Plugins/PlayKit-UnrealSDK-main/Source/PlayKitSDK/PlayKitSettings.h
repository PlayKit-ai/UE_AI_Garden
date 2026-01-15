// Copyright PlayKit. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "PlayKitSettings.generated.h"

/**
 * PlayKit SDK Settings
 * Configure your PlayKit integration from Project Settings > Plugins > PlayKit SDK
 */
UCLASS(config=PlayKit, defaultconfig, meta=(DisplayName="PlayKit SDK"))
class PLAYKITSDK_API UPlayKitSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPlayKitSettings();

	/** Get the settings instance */
	static UPlayKitSettings* Get();

	//========== Authentication ==========//

	/** Your Game ID from the PlayKit dashboard */
	UPROPERTY(config, EditAnywhere, Category="Authentication", meta=(DisplayName="Game ID"))
	FString GameId;

	/** Developer token (stored locally, not committed to source control) */
	UPROPERTY(Transient, VisibleAnywhere, Category="Authentication", meta=(DisplayName="Developer Token Status"))
	FString DeveloperTokenStatus = TEXT("Not logged in");

	//========== AI Models ==========//

	/** Default AI model for chat/NPC conversations */
	UPROPERTY(config, EditAnywhere, Category="AI Models", meta=(DisplayName="Default Chat Model"))
	FString DefaultChatModel = TEXT("default-chat");

	/** Default AI model for image generation */
	UPROPERTY(config, EditAnywhere, Category="AI Models", meta=(DisplayName="Default Image Model"))
	FString DefaultImageModel = TEXT("default-image");

	/** Default AI model for speech-to-text transcription */
	UPROPERTY(config, EditAnywhere, Category="AI Models", meta=(DisplayName="Default Transcription Model"))
	FString DefaultTranscriptionModel = TEXT("default-transcription-model");

	/** Default AI model for 3D generation */
	UPROPERTY(config, EditAnywhere, Category="AI Models", meta=(DisplayName="Default 3D Model"))
	FString Default3DModel = TEXT("default-3d-model");

	/** Model to use for fast operations (compaction, predictions) */
	UPROPERTY(config, EditAnywhere, Category="AI Models", meta=(DisplayName="Fast Model"))
	FString FastModel = TEXT("default-chat-fast");

	//========== Context Management ==========//

	/** Enable automatic conversation compaction when NPC is idle */
	UPROPERTY(config, EditAnywhere, Category="Context Management", meta=(DisplayName="Enable Auto Compact"))
	bool bEnableAutoCompact = true;

	/** Seconds of idle time before triggering auto-compact */
	UPROPERTY(config, EditAnywhere, Category="Context Management", meta=(DisplayName="Auto Compact Timeout", ClampMin="60", ClampMax="3600"))
	float AutoCompactTimeoutSeconds = 300.0f;

	/** Minimum messages before considering compaction */
	UPROPERTY(config, EditAnywhere, Category="Context Management", meta=(DisplayName="Auto Compact Min Messages", ClampMin="5", ClampMax="100"))
	int32 AutoCompactMinMessages = 10;

	//========== Advanced ==========//

	/** Override the default API base URL (leave empty to use default: https://api.playkit.ai) */
	UPROPERTY(config, EditAnywhere, Category="Advanced", meta=(DisplayName="Custom Base URL"))
	FString CustomBaseUrl;

	/** Force player authentication flow even when developer token is available (for testing) */
	UPROPERTY(config, EditAnywhere, Category="Advanced", meta=(DisplayName="Ignore Developer Token"))
	bool bIgnoreDeveloperToken = false;

	/** Enable verbose logging for debugging */
	UPROPERTY(config, EditAnywhere, Category="Advanced", meta=(DisplayName="Enable Debug Logging"))
	bool bEnableDebugLogging = false;

	//========== Helpers ==========//

	/** Get the effective base URL */
	UFUNCTION(BlueprintPure, Category="PlayKit")
	FString GetBaseUrl() const;

	/** Get the AI API base URL */
	UFUNCTION(BlueprintPure, Category="PlayKit")
	FString GetAIBaseUrl() const;

	/** Check if a developer token is available */
	UFUNCTION(BlueprintPure, Category="PlayKit")
	bool HasDeveloperToken() const;

	/** Get the stored developer token */
	FString GetDeveloperToken() const;

	/** Set the developer token (stored locally) */
	void SetDeveloperToken(const FString& Token);

	/** Clear the developer token */
	void ClearDeveloperToken();

	/** Get the stored player token */
	FString GetPlayerToken() const;

	/** Set the player token (stored locally) */
	void SetPlayerToken(const FString& Token);

	/** Clear the player token */
	void ClearPlayerToken();

	/** Save Config */
	UFUNCTION(CallInEditor)
	void SaveSettings();
	
	//========== UDeveloperSettings Interface ==========//

	virtual FName GetCategoryName() const override { return FName(TEXT("Plugins")); }
	virtual FName GetSectionName() const override { return FName(TEXT("PlayKit SDK")); }

#if WITH_EDITOR
	virtual FText GetSectionText() const override { return NSLOCTEXT("PlayKitSettings", "SectionText", "PlayKit SDK"); }
	virtual FText GetSectionDescription() const override { return NSLOCTEXT("PlayKitSettings", "SectionDesc", "Configure PlayKit SDK for AI-powered game features"); }
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	static const FString DeveloperTokenKey;
	static const FString PlayerTokenKey;
};
