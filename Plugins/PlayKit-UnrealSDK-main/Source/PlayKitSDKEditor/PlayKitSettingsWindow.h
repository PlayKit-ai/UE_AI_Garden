// Copyright PlayKit. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Interfaces/IHttpRequest.h"

class UPlayKitSettings;

/**
 * PlayKit SDK Settings Window
 */
class SPlayKitSettingsWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPlayKitSettingsWindow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SPlayKitSettingsWindow();

	// Flag to check if widget is still valid during callbacks
	bool bIsDestroyed = false;

private:
	// UI Building
	TSharedRef<SWidget> BuildAuthenticationSection();
	TSharedRef<SWidget> BuildGameSection();
	TSharedRef<SWidget> BuildModelsSection();
	TSharedRef<SWidget> BuildAdvancedSection();
	TSharedRef<SWidget> BuildAboutSection();

	// Authentication
	FReply OnLoginClicked();
	FReply OnLogoutClicked();
	void StartDeviceAuthFlow();
	void PollForAuthorization();
	void HandleDeviceCodeResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void HandleTokenPollResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void HandlePlayerTokenResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	// Game Selection
	void LoadGames();
	void HandleGamesResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void OnGameSelected(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo);

	// Models
	void LoadModels();
	void HandleModelsResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	// UI Helpers
	void UpdateLoginStatus();
	void RefreshUI();

	// Settings reference
	UPlayKitSettings* Settings;

	// Auth state
	bool bIsAuthenticating = false;
	FString SessionId;           // session_id from server
	FString StoredCodeVerifier;  // PKCE code verifier for poll request
	FString AuthUrl;
	int32 PollInterval = 5;
	FTimerHandle PollTimerHandle;

	// Loading states
	bool bIsLoadingGames = false;
	bool bIsLoadingModels = false;

	// Pending HTTP requests (for cancellation on destroy)
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> PendingAuthRequest;
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> PendingPollRequest;
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> PendingGamesRequest;
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> PendingModelsRequest;

	// Cached data
	TArray<TSharedPtr<FString>> GameOptions;
	TArray<TSharedPtr<FString>> ChatModelOptions;
	TArray<TSharedPtr<FString>> ImageModelOptions;

	// Selected items (for combo box display)
	TSharedPtr<FString> SelectedChatModel;
	TSharedPtr<FString> SelectedImageModel;

	// UI Widgets
	TSharedPtr<STextBlock> LoginStatusText;
	TSharedPtr<STextBlock> AuthInstructionsText;
	TSharedPtr<SButton> LoginButton;
	TSharedPtr<SButton> LogoutButton;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> GameComboBox;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> ChatModelComboBox;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> ImageModelComboBox;
	TSharedPtr<SVerticalBox> GameSection;
	TSharedPtr<SVerticalBox> ModelsSection;
};

/**
 * Static helper to open the settings window
 */
class PLAYKITSDKEDITOR_API FPlayKitSettingsWindow
{
public:
	static void Open();

private:
	static TWeakPtr<SWindow> SettingsWindow;
};
