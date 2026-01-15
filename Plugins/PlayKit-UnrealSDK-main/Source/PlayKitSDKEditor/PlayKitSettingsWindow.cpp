// Copyright PlayKit. All Rights Reserved.

#include "PlayKitSettingsWindow.h"
#include "PlayKitSettings.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Images/SThrobber.h"
#include "Framework/Application/SlateApplication.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Base64.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Editor.h"

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

#define LOCTEXT_NAMESPACE "PlayKitSettingsWindow"

TWeakPtr<SWindow> FPlayKitSettingsWindow::SettingsWindow;

void FPlayKitSettingsWindow::Open()
{
	if (SettingsWindow.IsValid())
	{
		SettingsWindow.Pin()->BringToFront();
		return;
	}

	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("WindowTitle", "PlayKit SDK Settings"))
		.ClientSize(FVector2D(600, 700))
		.SupportsMinimize(false)
		.SupportsMaximize(false)
		[
			SNew(SPlayKitSettingsWindow)
		];

	SettingsWindow = Window;

	FSlateApplication::Get().AddWindow(Window);
}

SPlayKitSettingsWindow::~SPlayKitSettingsWindow()
{
	// Mark as destroyed to prevent callbacks from accessing invalid state
	bIsDestroyed = true;

	// Cancel any pending HTTP requests to prevent callbacks to destroyed widget
	if (PendingAuthRequest.IsValid())
	{
		PendingAuthRequest->CancelRequest();
		PendingAuthRequest.Reset();
	}
	if (PendingPollRequest.IsValid())
	{
		PendingPollRequest->CancelRequest();
		PendingPollRequest.Reset();
	}
	if (PendingGamesRequest.IsValid())
	{
		PendingGamesRequest->CancelRequest();
		PendingGamesRequest.Reset();
	}
	if (PendingModelsRequest.IsValid())
	{
		PendingModelsRequest->CancelRequest();
		PendingModelsRequest.Reset();
	}

	// Cancel poll timer
	if (GEditor && PollTimerHandle.IsValid())
	{
		GEditor->GetTimerManager()->ClearTimer(PollTimerHandle);
	}
}

void SPlayKitSettingsWindow::Construct(const FArguments& InArgs)
{
	Settings = UPlayKitSettings::Get();

	ChildSlot
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot()
		.Padding(20)
		[
			SNew(SVerticalBox)

			// Header
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 20)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("HeaderTitle", "PlayKit SDK"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 24))
			]

			// Authentication Section
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 20)
			[
				BuildAuthenticationSection()
			]

			// Game Selection Section (only visible when logged in)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 20)
			[
				SAssignNew(GameSection, SVerticalBox)
				.Visibility_Lambda([this]() {
					return (Settings && Settings->HasDeveloperToken()) ? EVisibility::Visible : EVisibility::Collapsed;
				})
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					BuildGameSection()
				]
			]

			// Models Section
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 20)
			[
				SAssignNew(ModelsSection, SVerticalBox)
				.Visibility_Lambda([this]() {
					return (Settings && !Settings->GameId.IsEmpty()) ? EVisibility::Visible : EVisibility::Collapsed;
				})
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					BuildModelsSection()
				]
			]

			// Advanced Section
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 20)
			[
				BuildAdvancedSection()
			]

			// About Section
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				BuildAboutSection()
			]
		]
	];

	UpdateLoginStatus();

	// Load games if already logged in
	if (Settings->HasDeveloperToken())
	{
		LoadGames();
	}
}

TSharedRef<SWidget> SPlayKitSettingsWindow::BuildAuthenticationSection()
{
	return SNew(SVerticalBox)

	// Section Header
	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding(0, 0, 0, 10)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("AuthSection", "Developer Authentication"))
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
	]

	// Status
	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding(0, 0, 0, 10)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("StatusLabel", "Status: "))
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0, 0, 5, 0)
		[
			SNew(SCircularThrobber)
			.Radius(8.0f)
			.Visibility_Lambda([this]()
			{
				return bIsAuthenticating ? EVisibility::Visible : EVisibility::Collapsed;
			})
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SAssignNew(LoginStatusText, STextBlock)
			.Text(LOCTEXT("NotLoggedIn", "Not logged in"))
		]
	]

	// Auth Instructions (visible during auth flow)
	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding(0, 0, 0, 10)
	[
		SAssignNew(AuthInstructionsText, STextBlock)
		.Text(FText::GetEmpty())
		.Visibility(EVisibility::Collapsed)
		.AutoWrapText(true)
	]

	// Buttons
	+ SVerticalBox::Slot()
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0, 0, 10, 0)
		[
			SAssignNew(LoginButton, SButton)
			.Text(LOCTEXT("LoginButton", "Login"))
			.OnClicked(this, &SPlayKitSettingsWindow::OnLoginClicked)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SAssignNew(LogoutButton, SButton)
			.Text(LOCTEXT("LogoutButton", "Logout"))
			.OnClicked(this, &SPlayKitSettingsWindow::OnLogoutClicked)
			.Visibility(EVisibility::Collapsed)
		]
	]

	// Separator
	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding(0, 15, 0, 0)
	[
		SNew(SSeparator)
	];
}

TSharedRef<SWidget> SPlayKitSettingsWindow::BuildGameSection()
{
	return SNew(SVerticalBox)

	// Section Header
	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding(0, 0, 0, 10)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("GameSection", "Game Selection"))
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
	]

	// Game Dropdown
	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding(0, 0, 0, 10)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0, 0, 10, 0)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("GameLabel", "Select Game:"))
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			SAssignNew(GameComboBox, SComboBox<TSharedPtr<FString>>)
			.OptionsSource(&GameOptions)
			.OnSelectionChanged(this, &SPlayKitSettingsWindow::OnGameSelected)
			.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
			{
				return SNew(STextBlock).Text(Item.IsValid() ? FText::FromString(*Item) : FText::GetEmpty());
			})
			.Content()
			[
				SNew(STextBlock)
				.Text_Lambda([this]()
				{
					if (!Settings) return LOCTEXT("SelectGame", "Select a game...");
					return Settings->GameId.IsEmpty()
						? LOCTEXT("SelectGame", "Select a game...")
						: FText::FromString(Settings->GameId);
				})
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(10, 0, 0, 0)
		[
			SNew(SCircularThrobber)
			.Radius(8.0f)
			.Visibility_Lambda([this]()
			{
				return bIsLoadingGames ? EVisibility::Visible : EVisibility::Collapsed;
			})
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(5, 0, 0, 0)
		[
			SNew(SButton)
			.Text(LOCTEXT("RefreshGames", "Refresh"))
			.IsEnabled_Lambda([this]() { return !bIsLoadingGames; })
			.OnClicked_Lambda([this]()
			{
				LoadGames();
				return FReply::Handled();
			})
		]
	]

	// Separator
	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding(0, 15, 0, 0)
	[
		SNew(SSeparator)
	];
}

TSharedRef<SWidget> SPlayKitSettingsWindow::BuildModelsSection()
{
	return SNew(SVerticalBox)

	// Section Header
	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding(0, 0, 0, 10)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ModelsSection", "AI Model Defaults"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(10, 0, 0, 0)
		[
			SNew(SCircularThrobber)
			.Radius(8.0f)
			.Visibility_Lambda([this]()
			{
				return bIsLoadingModels ? EVisibility::Visible : EVisibility::Collapsed;
			})
		]
	]

	// Chat Model
	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding(0, 0, 0, 10)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0, 0, 10, 0)
		[
			SNew(SBox)
			.WidthOverride(120)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ChatModelLabel", "Chat Model:"))
			]
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			SAssignNew(ChatModelComboBox, SComboBox<TSharedPtr<FString>>)
			.OptionsSource(&ChatModelOptions)
			.OnSelectionChanged_Lambda([this](TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo)
			{
				if (NewValue.IsValid() && Settings)
				{
					SelectedChatModel = NewValue;
					Settings->DefaultChatModel = *NewValue;
					Settings->SaveSettings();
					UE_LOG(LogTemp, Log, TEXT("[PlayKit] Selected chat model: %s"), **NewValue);
				}
			})
			.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
			{
				return SNew(STextBlock).Text(FText::FromString(*Item));
			})
			.Content()
			[
				SNew(STextBlock)
				.Text_Lambda([this]()
				{
					if (SelectedChatModel.IsValid())
					{
						return FText::FromString(*SelectedChatModel);
					}
					return Settings && !Settings->DefaultChatModel.IsEmpty()
						? FText::FromString(Settings->DefaultChatModel)
						: LOCTEXT("SelectChatModel", "Select...");
				})
			]
		]
	]

	// Image Model
	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding(0, 0, 0, 10)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0, 0, 10, 0)
		[
			SNew(SBox)
			.WidthOverride(120)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ImageModelLabel", "Image Model:"))
			]
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			SAssignNew(ImageModelComboBox, SComboBox<TSharedPtr<FString>>)
			.OptionsSource(&ImageModelOptions)
			.OnSelectionChanged_Lambda([this](TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo)
			{
				if (NewValue.IsValid() && Settings)
				{
					SelectedImageModel = NewValue;
					Settings->DefaultImageModel = *NewValue;
					Settings->SaveSettings();
					UE_LOG(LogTemp, Log, TEXT("[PlayKit] Selected image model: %s"), **NewValue);
				}
			})
			.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
			{
				return SNew(STextBlock).Text(FText::FromString(*Item));
			})
			.Content()
			[
				SNew(STextBlock)
				.Text_Lambda([this]()
				{
					if (SelectedImageModel.IsValid())
					{
						return FText::FromString(*SelectedImageModel);
					}
					return Settings && !Settings->DefaultImageModel.IsEmpty()
						? FText::FromString(Settings->DefaultImageModel)
						: LOCTEXT("SelectImageModel", "Select...");
				})
			]
		]
	]

	// Transcription Model
	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding(0, 0, 0, 10)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0, 0, 10, 0)
		[
			SNew(SBox)
			.WidthOverride(120)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("TranscriptionModelLabel", "Transcription Model:"))
			]
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			SAssignNew(TranscriptionModelComboBox, SComboBox<TSharedPtr<FString>>)
			.OptionsSource(&TranscriptionModelOptions)
			.OnSelectionChanged_Lambda([this](TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo)
			{
				if (NewValue.IsValid() && Settings)
				{
					SelectedTranscriptionModel = NewValue;
					Settings->DefaultTranscriptionModel = *NewValue;
					Settings->SaveSettings();
					UE_LOG(LogTemp, Log, TEXT("[PlayKit] Selected transcription model: %s"), **NewValue);
				}
			})
			.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
			{
				return SNew(STextBlock).Text(FText::FromString(*Item));
			})
			.Content()
			[
				SNew(STextBlock)
				.Text_Lambda([this]()
				{
					if (SelectedTranscriptionModel.IsValid())
					{
						return FText::FromString(*SelectedTranscriptionModel);
					}
					return Settings && !Settings->DefaultTranscriptionModel.IsEmpty()
						? FText::FromString(Settings->DefaultTranscriptionModel)
						: LOCTEXT("SelectTranscriptionModel", "Select...");
				})
			]
		]
	]

	// 3D Model
	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding(0, 0, 0, 10)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0, 0, 10, 0)
		[
			SNew(SBox)
			.WidthOverride(120)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("3DModelLabel", "3D Model:"))
			]
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			SAssignNew(Model3DComboBox, SComboBox<TSharedPtr<FString>>)
			.OptionsSource(&Model3DOptions)
			.OnSelectionChanged_Lambda([this](TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo)
			{
				if (NewValue.IsValid() && Settings)
				{
					Selected3DModel = NewValue;
					Settings->Default3DModel = *NewValue;
					Settings->SaveSettings();
					UE_LOG(LogTemp, Log, TEXT("[PlayKit] Selected 3D model: %s"), **NewValue);
				}
			})
			.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
			{
				return SNew(STextBlock).Text(FText::FromString(*Item));
			})
			.Content()
			[
				SNew(STextBlock)
				.Text_Lambda([this]()
				{
					if (Selected3DModel.IsValid())
					{
						return FText::FromString(*Selected3DModel);
					}
					return Settings && !Settings->Default3DModel.IsEmpty()
						? FText::FromString(Settings->Default3DModel)
						: LOCTEXT("Select3DModel", "Select...");
				})
			]
		]
	]

	// Separator
	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding(0, 15, 0, 0)
	[
		SNew(SSeparator)
	];
}

TSharedRef<SWidget> SPlayKitSettingsWindow::BuildAdvancedSection()
{
	return SNew(SExpandableArea)
	.HeaderContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("AdvancedSection", "Advanced Settings"))
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
	]
	.BodyContent()
	[
		SNew(SVerticalBox)

		// Custom Base URL
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 10, 0, 10)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0, 0, 10, 0)
			[
				SNew(SBox)
				.WidthOverride(120)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("BaseUrlLabel", "Custom URL:"))
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SEditableTextBox)
				.Text_Lambda([this]() {
					return Settings ? FText::FromString(Settings->CustomBaseUrl) : FText::GetEmpty();
				})
				.HintText(LOCTEXT("BaseUrlHint", "https://api.playkit.ai"))
				.OnTextCommitted_Lambda([this](const FText& NewText, ETextCommit::Type CommitType)
				{
					if (Settings)
					{
						Settings->CustomBaseUrl = NewText.ToString();
						Settings->SaveSettings();
					}
				})
			]
		]

		// Ignore Developer Token
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 10)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([this]() {
					return (Settings && Settings->bIgnoreDeveloperToken) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)
				{
					if (Settings)
					{
						Settings->bIgnoreDeveloperToken = (NewState == ECheckBoxState::Checked);
						Settings->SaveSettings();
					}
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(5, 0, 0, 0)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("IgnoreDevToken", "Play as Player (ignore developer token)"))
			]
		]

		// Debug Logging
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 10)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([this]() {
					return (Settings && Settings->bEnableDebugLogging) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)
				{
					if (Settings)
					{
						Settings->bEnableDebugLogging = (NewState == ECheckBoxState::Checked);
						Settings->SaveSettings();
					}
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(5, 0, 0, 0)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("DebugLogging", "Enable debug logging"))
			]
		]

		// Clear Player Token
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 10, 0, 0)
		[
			SNew(SButton)
			.Text(LOCTEXT("ClearPlayerToken", "Clear Local Player Token"))
			.OnClicked_Lambda([this]()
			{
				if (Settings)
				{
					Settings->ClearPlayerToken();
				}
				return FReply::Handled();
			})
		]
	];
}

TSharedRef<SWidget> SPlayKitSettingsWindow::BuildAboutSection()
{
	return SNew(SVerticalBox)

	// Section Header
	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding(0, 0, 0, 10)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("AboutSection", "About"))
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
	]

	// Version
	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding(0, 0, 0, 5)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("Version", "PlayKit SDK for Unreal Engine v1.0.0"))
	]

	// Links
	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding(0, 10, 0, 0)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0, 0, 10, 0)
		[
			SNew(SButton)
			.Text(LOCTEXT("DocsButton", "Documentation"))
			.OnClicked_Lambda([]()
			{
				FPlatformProcess::LaunchURL(TEXT("https://docs.playkit.ai"), nullptr, nullptr);
				return FReply::Handled();
			})
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0, 0, 10, 0)
		[
			SNew(SButton)
			.Text(LOCTEXT("WebsiteButton", "Website"))
			.OnClicked_Lambda([]()
			{
				FPlatformProcess::LaunchURL(TEXT("https://api.playkit.ai"), nullptr, nullptr);
				return FReply::Handled();
			})
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.Text(LOCTEXT("SupportButton", "Support"))
			.OnClicked_Lambda([]()
			{
				FPlatformProcess::LaunchURL(TEXT("mailto:support@playkit.ai"), nullptr, nullptr);
				return FReply::Handled();
			})
		]
	];
}

void SPlayKitSettingsWindow::UpdateLoginStatus()
{
	bool bLoggedIn = Settings->HasDeveloperToken();

	if (LoginStatusText.IsValid())
	{
		LoginStatusText->SetText(bLoggedIn
			? LOCTEXT("LoggedIn", "Logged in")
			: LOCTEXT("NotLoggedIn", "Not logged in"));
	}

	if (LoginButton.IsValid())
	{
		LoginButton->SetVisibility(bLoggedIn ? EVisibility::Collapsed : EVisibility::Visible);
	}

	if (LogoutButton.IsValid())
	{
		LogoutButton->SetVisibility(bLoggedIn ? EVisibility::Visible : EVisibility::Collapsed);
	}
}

FReply SPlayKitSettingsWindow::OnLoginClicked()
{
	if (bIsAuthenticating)
	{
		return FReply::Handled();
	}

	StartDeviceAuthFlow();
	return FReply::Handled();
}

FReply SPlayKitSettingsWindow::OnLogoutClicked()
{
	Settings->ClearDeveloperToken();
	GameOptions.Empty();
	UpdateLoginStatus();
	return FReply::Handled();
}

void SPlayKitSettingsWindow::StartDeviceAuthFlow()
{
	bIsAuthenticating = true;

	if (LoginButton.IsValid())
	{
		LoginButton->SetEnabled(false);
	}

	if (LoginStatusText.IsValid())
	{
		LoginStatusText->SetText(LOCTEXT("Authenticating", "Authenticating..."));
	}

	// Request device code
	FString Url = Settings->GetBaseUrl() / TEXT("api/device-auth/initiate");
	UE_LOG(LogTemp, Log, TEXT("[PlayKit] Starting device auth flow, URL: %s"), *Url);

	PendingAuthRequest = FHttpModule::Get().CreateRequest();
	PendingAuthRequest->SetURL(Url);
	PendingAuthRequest->SetVerb(TEXT("POST"));
	PendingAuthRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	// Build request body with PKCE S256
	// Step 1: Generate random bytes for code verifier
	TArray<uint8> RandomBytes;
	RandomBytes.SetNum(32);
	for (int32 i = 0; i < 32; i++)
	{
		RandomBytes[i] = static_cast<uint8>(FMath::RandRange(0, 255));
	}

	// Step 2: Base64URL encode to create code verifier
	StoredCodeVerifier = FBase64::Encode(RandomBytes);
	StoredCodeVerifier.ReplaceInline(TEXT("+"), TEXT("-"));
	StoredCodeVerifier.ReplaceInline(TEXT("/"), TEXT("_"));
	StoredCodeVerifier.ReplaceInline(TEXT("="), TEXT(""));

	// Step 3: SHA256 hash of code verifier for code challenge
	auto Utf8Verifier = StringCast<UTF8CHAR>(*StoredCodeVerifier);
	const uint8* VerifierData = reinterpret_cast<const uint8*>(Utf8Verifier.Get());
	int32 VerifierLen = Utf8Verifier.Length();

	uint8 HashResult[32];
	ComputeSHA256(VerifierData, VerifierLen, HashResult);

	TArray<uint8> HashArray;
	HashArray.Append(HashResult, 32);

	// Step 4: Base64URL encode the hash to create code challenge
	FString CodeChallenge = FBase64::Encode(HashArray);
	CodeChallenge.ReplaceInline(TEXT("+"), TEXT("-"));
	CodeChallenge.ReplaceInline(TEXT("/"), TEXT("_"));
	CodeChallenge.ReplaceInline(TEXT("="), TEXT(""));

	UE_LOG(LogTemp, Log, TEXT("[PlayKit] PKCE Code Verifier: %s"), *StoredCodeVerifier);
	UE_LOG(LogTemp, Log, TEXT("[PlayKit] PKCE Code Challenge (S256): %s"), *CodeChallenge);

	TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();
	JsonObject->SetStringField(TEXT("code_challenge"), CodeChallenge);
	JsonObject->SetStringField(TEXT("code_challenge_method"), TEXT("S256"));
	JsonObject->SetStringField(TEXT("scope"), TEXT("developer:full"));

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
	PendingAuthRequest->SetContentAsString(JsonString);

	UE_LOG(LogTemp, Log, TEXT("[PlayKit] Request body: %s"), *JsonString);

	PendingAuthRequest->OnProcessRequestComplete().BindRaw(this, &SPlayKitSettingsWindow::HandleDeviceCodeResponse);
	PendingAuthRequest->ProcessRequest();
}

void SPlayKitSettingsWindow::HandleDeviceCodeResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (bIsDestroyed) return;

	PendingAuthRequest.Reset();
	FString ErrorMessage;

	if (!bWasSuccessful)
	{
		ErrorMessage = TEXT("Network error - check your internet connection");
		UE_LOG(LogTemp, Error, TEXT("[PlayKit] Device auth failed: Network error"));
	}
	else if (!Response.IsValid())
	{
		ErrorMessage = TEXT("Invalid response from server");
		UE_LOG(LogTemp, Error, TEXT("[PlayKit] Device auth failed: Invalid response"));
	}
	else if (Response->GetResponseCode() != 200)
	{
		int32 StatusCode = Response->GetResponseCode();
		FString ResponseBody = Response->GetContentAsString();
		ErrorMessage = FString::Printf(TEXT("HTTP %d - %s"), StatusCode, *ResponseBody.Left(100));
		UE_LOG(LogTemp, Error, TEXT("[PlayKit] Device auth failed: HTTP %d - %s"), StatusCode, *ResponseBody);
	}

	if (!ErrorMessage.IsEmpty())
	{
		bIsAuthenticating = false;
		if (LoginButton.IsValid())
		{
			LoginButton->SetEnabled(true);
		}
		if (LoginStatusText.IsValid())
		{
			LoginStatusText->SetText(FText::FromString(FString::Printf(TEXT("Failed: %s"), *ErrorMessage)));
		}
		if (AuthInstructionsText.IsValid())
		{
			AuthInstructionsText->SetVisibility(EVisibility::Visible);
			AuthInstructionsText->SetText(FText::FromString(FString::Printf(TEXT("Error: %s\n\nCheck Output Log for details."), *ErrorMessage)));
		}
		return;
	}

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
	if (!FJsonSerializer::Deserialize(Reader, JsonObject))
	{
		bIsAuthenticating = false;
		UE_LOG(LogTemp, Error, TEXT("[PlayKit] Device auth failed: Could not parse JSON response"));
		if (LoginStatusText.IsValid())
		{
			LoginStatusText->SetText(LOCTEXT("AuthFailedParse", "Failed: Invalid JSON response"));
		}
		return;
	}

	// Parse response (matching Unity SDK field names)
	SessionId = JsonObject->GetStringField(TEXT("session_id"));
	AuthUrl = JsonObject->GetStringField(TEXT("auth_url"));
	PollInterval = JsonObject->GetIntegerField(TEXT("poll_interval"));

	UE_LOG(LogTemp, Log, TEXT("[PlayKit] Got session_id: %s, auth_url: %s, poll_interval: %d"),
		*SessionId, *AuthUrl, PollInterval);

	if (SessionId.IsEmpty() || AuthUrl.IsEmpty())
	{
		bIsAuthenticating = false;
		UE_LOG(LogTemp, Error, TEXT("[PlayKit] Device auth failed: Missing session_id or auth_url"));
		if (LoginStatusText.IsValid())
		{
			LoginStatusText->SetText(LOCTEXT("AuthFailedMissing", "Failed: Invalid server response"));
		}
		return;
	}

	// Show instructions
	if (AuthInstructionsText.IsValid())
	{
		AuthInstructionsText->SetVisibility(EVisibility::Visible);
		AuthInstructionsText->SetText(LOCTEXT("AuthInstructionsBrowser", "Opening browser for authorization..."));
	}

	// Open browser
	FPlatformProcess::LaunchURL(*AuthUrl, nullptr, nullptr);

	// Start polling
	PollForAuthorization();
}

void SPlayKitSettingsWindow::PollForAuthorization()
{
	// Include code_verifier for PKCE verification (matching Unity SDK)
	FString Url = FString::Printf(TEXT("%s/api/device-auth/poll?session_id=%s&code_verifier=%s"),
		*Settings->GetBaseUrl(), *SessionId, *StoredCodeVerifier);

	UE_LOG(LogTemp, Log, TEXT("[PlayKit] Polling: %s"), *Url);

	PendingPollRequest = FHttpModule::Get().CreateRequest();
	PendingPollRequest->SetURL(Url);
	PendingPollRequest->SetVerb(TEXT("GET"));

	PendingPollRequest->OnProcessRequestComplete().BindRaw(this, &SPlayKitSettingsWindow::HandleTokenPollResponse);
	PendingPollRequest->ProcessRequest();
}

void SPlayKitSettingsWindow::HandleTokenPollResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (bIsDestroyed) return;

	PendingPollRequest.Reset();

	if (!bWasSuccessful || !Response.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayKit] Poll request failed, retrying..."));
		// Retry after interval
		if (GEditor)
		{
			GEditor->GetTimerManager()->SetTimer(
				PollTimerHandle,
				FTimerDelegate::CreateRaw(this, &SPlayKitSettingsWindow::PollForAuthorization),
				static_cast<float>(PollInterval),
				false
			);
		}
		return;
	}

	int32 ResponseCode = Response->GetResponseCode();
	FString ResponseBody = Response->GetContentAsString();
	UE_LOG(LogTemp, Log, TEXT("[PlayKit] Poll response: HTTP %d - %s"), ResponseCode, *ResponseBody);

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject))
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayKit] Failed to parse poll response, retrying..."));
		// Retry
		if (GEditor)
		{
			GEditor->GetTimerManager()->SetTimer(
				PollTimerHandle,
				FTimerDelegate::CreateRaw(this, &SPlayKitSettingsWindow::PollForAuthorization),
				static_cast<float>(PollInterval),
				false
			);
		}
		return;
	}

	// Check status field (Unity SDK style)
	FString Status = JsonObject->GetStringField(TEXT("status"));
	FString Error = JsonObject->GetStringField(TEXT("error"));

	if (ResponseCode == 200)
	{
		if (Status == TEXT("authorized"))
		{
			// Success! Got the token
			FString AccessToken = JsonObject->GetStringField(TEXT("access_token"));
			UE_LOG(LogTemp, Log, TEXT("[PlayKit] Authorization successful!"));

			Settings->SetDeveloperToken(AccessToken);

			bIsAuthenticating = false;
			if (AuthInstructionsText.IsValid())
			{
				AuthInstructionsText->SetVisibility(EVisibility::Collapsed);
			}
			if (LoginButton.IsValid())
			{
				LoginButton->SetEnabled(true);
			}

			UpdateLoginStatus();
			LoadGames();
			return;
		}
		else if (Status == TEXT("pending"))
		{
			// Still waiting for user, update poll interval if provided
			int32 NewInterval = JsonObject->GetIntegerField(TEXT("poll_interval"));
			if (NewInterval > 0)
			{
				PollInterval = NewInterval;
			}

			// Continue polling
			if (GEditor)
			{
				GEditor->GetTimerManager()->SetTimer(
					PollTimerHandle,
					FTimerDelegate::CreateRaw(this, &SPlayKitSettingsWindow::PollForAuthorization),
					static_cast<float>(PollInterval),
					false
				);
			}
			return;
		}
	}

	// Handle error responses
	if (!Error.IsEmpty())
	{
		if (Error == TEXT("slow_down"))
		{
			// Increase poll interval
			PollInterval = FMath::Min(PollInterval * 2, 30);
			UE_LOG(LogTemp, Log, TEXT("[PlayKit] Slowing down polling to %d seconds"), PollInterval);

			if (GEditor)
			{
				GEditor->GetTimerManager()->SetTimer(
					PollTimerHandle,
					FTimerDelegate::CreateRaw(this, &SPlayKitSettingsWindow::PollForAuthorization),
					static_cast<float>(PollInterval),
					false
				);
			}
			return;
		}
		else if (Error == TEXT("access_denied"))
		{
			bIsAuthenticating = false;
			if (LoginButton.IsValid())
			{
				LoginButton->SetEnabled(true);
			}
			if (AuthInstructionsText.IsValid())
			{
				AuthInstructionsText->SetVisibility(EVisibility::Collapsed);
			}
			if (LoginStatusText.IsValid())
			{
				LoginStatusText->SetText(LOCTEXT("AuthDenied", "Authorization denied by user"));
			}
			return;
		}
		else if (Error == TEXT("expired_token"))
		{
			bIsAuthenticating = false;
			if (LoginButton.IsValid())
			{
				LoginButton->SetEnabled(true);
			}
			if (AuthInstructionsText.IsValid())
			{
				AuthInstructionsText->SetVisibility(EVisibility::Collapsed);
			}
			if (LoginStatusText.IsValid())
			{
				LoginStatusText->SetText(LOCTEXT("AuthExpired", "Session expired, please try again"));
			}
			return;
		}
	}

	// For pending status on 400, continue polling
	if (ResponseCode == 400)
	{
		if (GEditor)
		{
			GEditor->GetTimerManager()->SetTimer(
				PollTimerHandle,
				FTimerDelegate::CreateRaw(this, &SPlayKitSettingsWindow::PollForAuthorization),
				static_cast<float>(PollInterval),
				false
			);
		}
		return;
	}

	// Unknown error
	bIsAuthenticating = false;
	if (LoginButton.IsValid())
	{
		LoginButton->SetEnabled(true);
	}
	if (AuthInstructionsText.IsValid())
	{
		AuthInstructionsText->SetVisibility(EVisibility::Collapsed);
	}
	if (LoginStatusText.IsValid())
	{
		FString ErrorMsg = Error.IsEmpty() ? FString::Printf(TEXT("HTTP %d"), ResponseCode) : Error;
		LoginStatusText->SetText(FText::FromString(FString::Printf(TEXT("Error: %s"), *ErrorMsg)));
	}
}

void SPlayKitSettingsWindow::HandlePlayerTokenResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	// Not implemented yet
}

void SPlayKitSettingsWindow::LoadGames()
{
	UE_LOG(LogTemp, Log, TEXT("[PlayKit] LoadGames() called"));

	// Refresh Settings pointer in case of hot reload
	if (!Settings)
	{
		Settings = UPlayKitSettings::Get();
	}

	if (!Settings)
	{
		UE_LOG(LogTemp, Error, TEXT("[PlayKit] Cannot load games: Settings is null"));
		return;
	}

	if (!Settings->HasDeveloperToken())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayKit] Cannot load games: not logged in (no developer token)"));
		return;
	}

	// Cancel any pending request
	if (PendingGamesRequest.IsValid())
	{
		UE_LOG(LogTemp, Log, TEXT("[PlayKit] Cancelling previous games request"));
		PendingGamesRequest->CancelRequest();
	}

	bIsLoadingGames = true;

	FString Url = Settings->GetBaseUrl() / TEXT("api/external/developer-games");
	UE_LOG(LogTemp, Log, TEXT("[PlayKit] Loading games from: %s"), *Url);
	UE_LOG(LogTemp, Log, TEXT("[PlayKit] Using token: %s..."), *Settings->GetDeveloperToken().Left(20));

	PendingGamesRequest = FHttpModule::Get().CreateRequest();
	PendingGamesRequest->SetURL(Url);
	PendingGamesRequest->SetVerb(TEXT("GET"));
	PendingGamesRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *Settings->GetDeveloperToken()));

	PendingGamesRequest->OnProcessRequestComplete().BindRaw(this, &SPlayKitSettingsWindow::HandleGamesResponse);
	PendingGamesRequest->ProcessRequest();
}

void SPlayKitSettingsWindow::HandleGamesResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (bIsDestroyed) return;

	UE_LOG(LogTemp, Log, TEXT("[PlayKit] HandleGamesResponse called - bWasSuccessful: %s"), bWasSuccessful ? TEXT("true") : TEXT("false"));

	PendingGamesRequest.Reset();
	bIsLoadingGames = false;
	GameOptions.Empty();

	if (!bWasSuccessful)
	{
		UE_LOG(LogTemp, Error, TEXT("[PlayKit] Games request failed: bWasSuccessful is false"));
		return;
	}

	if (!Response.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[PlayKit] Games request failed: Response is not valid"));
		return;
	}

	int32 ResponseCode = Response->GetResponseCode();
	FString ResponseBody = Response->GetContentAsString();
	UE_LOG(LogTemp, Log, TEXT("[PlayKit] Games response: HTTP %d - %s"), ResponseCode, *ResponseBody.Left(500));

	if (ResponseCode != 200)
	{
		UE_LOG(LogTemp, Error, TEXT("[PlayKit] Games request failed: HTTP %d"), ResponseCode);
		return;
	}

	// Parse response object: {"success": true, "games": [...]}
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[PlayKit] Failed to parse games response as JSON object"));
		return;
	}

	// Check success field
	bool bSuccess = JsonObject->GetBoolField(TEXT("success"));
	if (!bSuccess)
	{
		FString Error = JsonObject->GetStringField(TEXT("error"));
		UE_LOG(LogTemp, Error, TEXT("[PlayKit] Games API returned error: %s"), *Error);
		return;
	}

	// Get games array
	const TArray<TSharedPtr<FJsonValue>>* GamesArray;
	if (!JsonObject->TryGetArrayField(TEXT("games"), GamesArray))
	{
		UE_LOG(LogTemp, Error, TEXT("[PlayKit] No 'games' array in response"));
		return;
	}

	for (const TSharedPtr<FJsonValue>& Value : *GamesArray)
	{
		TSharedPtr<FJsonObject> GameObj = Value->AsObject();
		if (GameObj.IsValid())
		{
			FString GameId = GameObj->GetStringField(TEXT("id"));
			FString GameName = GameObj->GetStringField(TEXT("name"));
			FString ChannelType = GameObj->HasField(TEXT("channel_type")) ? GameObj->GetStringField(TEXT("channel_type")) : TEXT("standalone");

			// Format channel name for display
			FString ChannelDisplay = ChannelType;
			if (ChannelType == TEXT("standalone"))
			{
				ChannelDisplay = TEXT("Standalone");
			}
			else if (ChannelType.StartsWith(TEXT("steam")))
			{
				FString SteamSuffix = ChannelType.Replace(TEXT("steam_"), TEXT(""));
				ChannelDisplay = FString::Printf(TEXT("Steam (%s)"), *SteamSuffix);
			}
			else
			{
				// Capitalize first letter
				ChannelDisplay = ChannelType.Left(1).ToUpper() + ChannelType.Mid(1);
			}

			GameOptions.Add(MakeShared<FString>(FString::Printf(TEXT("%s [%s] (%s)"), *GameName, *ChannelDisplay, *GameId)));
			UE_LOG(LogTemp, Log, TEXT("[PlayKit] Found game: %s [%s] (%s)"), *GameName, *ChannelDisplay, *GameId);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[PlayKit] Loaded %d games"), GameOptions.Num());

	// Debug: list all games
	for (int32 i = 0; i < GameOptions.Num(); i++)
	{
		UE_LOG(LogTemp, Log, TEXT("[PlayKit] Game[%d]: %s"), i, GameOptions[i].IsValid() ? **GameOptions[i] : TEXT("null"));
	}

	if (GameComboBox.IsValid())
	{
		UE_LOG(LogTemp, Log, TEXT("[PlayKit] Refreshing GameComboBox..."));
		GameComboBox->RefreshOptions();
		GameComboBox->Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[PlayKit] GameComboBox is not valid!"));
	}
}

void SPlayKitSettingsWindow::OnGameSelected(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo)
{
	if (!NewValue.IsValid())
	{
		return;
	}

	// Extract GameId from "GameName (GameId)" format
	FString Selected = *NewValue;
	int32 OpenParen = Selected.Find(TEXT("("), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	int32 CloseParen = Selected.Find(TEXT(")"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);

	if (OpenParen != INDEX_NONE && CloseParen != INDEX_NONE)
	{
		Settings->GameId = Selected.Mid(OpenParen + 1, CloseParen - OpenParen - 1);
		Settings->SaveSettings();
		LoadModels();
	}
}

void SPlayKitSettingsWindow::LoadModels()
{
	if (!Settings || Settings->GameId.IsEmpty() || !Settings->HasDeveloperToken())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayKit] Cannot load models: no game selected or not logged in"));
		return;
	}

	bIsLoadingModels = true;

	FString Url = FString::Printf(TEXT("%s/ai/%s/models"), *Settings->GetBaseUrl(), *Settings->GameId);
	UE_LOG(LogTemp, Log, TEXT("[PlayKit] Loading models from: %s"), *Url);

	PendingModelsRequest = FHttpModule::Get().CreateRequest();
	PendingModelsRequest->SetURL(Url);
	PendingModelsRequest->SetVerb(TEXT("GET"));
	PendingModelsRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *Settings->GetDeveloperToken()));

	PendingModelsRequest->OnProcessRequestComplete().BindRaw(this, &SPlayKitSettingsWindow::HandleModelsResponse);
	PendingModelsRequest->ProcessRequest();
}

void SPlayKitSettingsWindow::HandleModelsResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (bIsDestroyed) return;

	PendingModelsRequest.Reset();
	bIsLoadingModels = false;
	ChatModelOptions.Empty();
	ImageModelOptions.Empty();
	TranscriptionModelOptions.Empty();
	Model3DOptions.Empty();

	if (!bWasSuccessful || !Response.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[PlayKit] Models request failed: network error"));
		return;
	}

	int32 ResponseCode = Response->GetResponseCode();
	FString ResponseBody = Response->GetContentAsString();
	UE_LOG(LogTemp, Log, TEXT("[PlayKit] Models response: HTTP %d - %s"), ResponseCode, *ResponseBody.Left(500));

	if (ResponseCode != 200)
	{
		UE_LOG(LogTemp, Error, TEXT("[PlayKit] Models request failed: HTTP %d"), ResponseCode);
		return;
	}

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject))
	{
		UE_LOG(LogTemp, Error, TEXT("[PlayKit] Failed to parse models response"));
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* ModelsArray;
	if (JsonObject->TryGetArrayField(TEXT("models"), ModelsArray))
	{
		for (const TSharedPtr<FJsonValue>& Value : *ModelsArray)
		{
			TSharedPtr<FJsonObject> ModelObj = Value->AsObject();
			if (ModelObj.IsValid())
			{
				FString ModelId = ModelObj->GetStringField(TEXT("id"));
				FString ModelName = ModelObj->GetStringField(TEXT("name"));
				FString ModelType = ModelObj->GetStringField(TEXT("type"));

				UE_LOG(LogTemp, Log, TEXT("[PlayKit] Found model: %s (%s) type=%s"), *ModelName, *ModelId, *ModelType);

				// Use "text" type for chat models (matching Unity SDK)
				if (ModelType == TEXT("text"))
				{
					ChatModelOptions.Add(MakeShared<FString>(ModelId));
				}
				else if (ModelType == TEXT("image"))
				{
					ImageModelOptions.Add(MakeShared<FString>(ModelId));
				}
				else if (ModelType == TEXT("transcription"))
				{
					TranscriptionModelOptions.Add(MakeShared<FString>(ModelId));
				}
				else if (ModelType == TEXT("3d"))
				{
					Model3DOptions.Add(MakeShared<FString>(ModelId));
				}
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[PlayKit] Loaded %d chat, %d image, %d transcription, %d 3D models"), 
		ChatModelOptions.Num(), ImageModelOptions.Num(), TranscriptionModelOptions.Num(), Model3DOptions.Num());

	// Find and set initial selected items based on Settings
	SelectedChatModel = nullptr;
	SelectedImageModel = nullptr;
	SelectedTranscriptionModel = nullptr;
	Selected3DModel = nullptr;

	if (Settings)
	{
		// Find matching chat model
		for (const TSharedPtr<FString>& Option : ChatModelOptions)
		{
			if (Option.IsValid() && *Option == Settings->DefaultChatModel)
			{
				SelectedChatModel = Option;
				break;
			}
		}

		// Find matching image model
		for (const TSharedPtr<FString>& Option : ImageModelOptions)
		{
			if (Option.IsValid() && *Option == Settings->DefaultImageModel)
			{
				SelectedImageModel = Option;
				break;
			}
		}

		// Find matching transcription model
		for (const TSharedPtr<FString>& Option : TranscriptionModelOptions)
		{
			if (Option.IsValid() && *Option == Settings->DefaultTranscriptionModel)
			{
				SelectedTranscriptionModel = Option;
				break;
			}
		}

		// Find matching 3D model
		for (const TSharedPtr<FString>& Option : Model3DOptions)
		{
			if (Option.IsValid() && *Option == Settings->Default3DModel)
			{
				Selected3DModel = Option;
				break;
			}
		}
	}

	// Refresh and sync combo boxes
	if (ChatModelComboBox.IsValid())
	{
		ChatModelComboBox->RefreshOptions();
		if (SelectedChatModel.IsValid())
		{
			ChatModelComboBox->SetSelectedItem(SelectedChatModel);
		}
	}
	if (ImageModelComboBox.IsValid())
	{
		ImageModelComboBox->RefreshOptions();
		if (SelectedImageModel.IsValid())
		{
			ImageModelComboBox->SetSelectedItem(SelectedImageModel);
		}
	}
	if (TranscriptionModelComboBox.IsValid())
	{
		TranscriptionModelComboBox->RefreshOptions();
		if (SelectedTranscriptionModel.IsValid())
		{
			TranscriptionModelComboBox->SetSelectedItem(SelectedTranscriptionModel);
		}
	}
	if (Model3DComboBox.IsValid())
	{
		Model3DComboBox->RefreshOptions();
		if (Selected3DModel.IsValid())
		{
			Model3DComboBox->SetSelectedItem(Selected3DModel);
		}
	}
}

void SPlayKitSettingsWindow::RefreshUI()
{
	UpdateLoginStatus();
}

#undef LOCTEXT_NAMESPACE
