// Copyright PlayKit. All Rights Reserved.

#include "PlayKitSettings.h"
#include "Misc/ConfigCacheIni.h"

#if WITH_EDITOR
#include "Misc/App.h"
#endif

const FString UPlayKitSettings::DeveloperTokenKey = TEXT("PlayKit_DeveloperToken");
const FString UPlayKitSettings::PlayerTokenKey = TEXT("PlayKit_PlayerToken");

UPlayKitSettings::UPlayKitSettings()
{
	this->CategoryName = TEXT("Plugins");
	this->SectionName = TEXT("PlayKit SDK Setting");
	this->DefaultChatModel = TEXT("gpt-4o");
	this->DefaultImageModel = TEXT("flux-1-schnell");
	this->DefaultTranscriptionModel = TEXT("whisper-large");
	this->Default3DModel = TEXT("tripo-v3");
	
	
	// 配置默认配置
	CustomBaseUrl = TEXT("https://api.playkit.ai");
}

UPlayKitSettings* UPlayKitSettings::Get()
{
	return GetMutableDefault<UPlayKitSettings>();
}

FString UPlayKitSettings::GetBaseUrl() const
{
	if (!CustomBaseUrl.IsEmpty())
	{
		return CustomBaseUrl;
	}
	return TEXT("https://api.playkit.ai");
}

FString UPlayKitSettings::GetAIBaseUrl() const
{
	return FString::Printf(TEXT("%s/ai/%s"), *GetBaseUrl(), *GameId);
}

bool UPlayKitSettings::HasDeveloperToken() const
{
	return !GetDeveloperToken().IsEmpty();
}

FString UPlayKitSettings::GetDeveloperToken() const
{
#if WITH_EDITOR
	FString Token;
	GConfig->GetString(TEXT("PlayKit"), *DeveloperTokenKey, Token, GEditorPerProjectIni);
	return Token;
#else
	return FString();
#endif
}

void UPlayKitSettings::SetDeveloperToken(const FString& Token)
{
#if WITH_EDITOR
	GConfig->SetString(TEXT("PlayKit"), *DeveloperTokenKey, *Token, GEditorPerProjectIni);
	GConfig->Flush(false, GEditorPerProjectIni);

	DeveloperTokenStatus = Token.IsEmpty() ? TEXT("Not logged in") : TEXT("Logged in");

	UE_LOG(LogTemp, Log, TEXT("[PlayKitSettings] Developer token updated"));
#endif
}

void UPlayKitSettings::ClearDeveloperToken()
{
	SetDeveloperToken(FString());
	DeveloperTokenStatus = TEXT("Not logged in");
}

FString UPlayKitSettings::GetPlayerToken() const
{
	FString Token;
	GConfig->GetString(TEXT("PlayKit"), *PlayerTokenKey, Token, GGameUserSettingsIni);
	return Token;
}

void UPlayKitSettings::SetPlayerToken(const FString& Token)
{
	GConfig->SetString(TEXT("PlayKit"), *PlayerTokenKey, *Token, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);

	UE_LOG(LogTemp, Log, TEXT("[PlayKitSettings] Player token updated"));
}

void UPlayKitSettings::ClearPlayerToken()
{
	SetPlayerToken(FString());
}

void UPlayKitSettings::SaveSettings()
{
	TryUpdateDefaultConfigFile();
}

#if WITH_EDITOR
void UPlayKitSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Save config to DefaultGame.ini when properties change
	SaveConfig();
	TryUpdateDefaultConfigFile();

	UE_LOG(LogTemp, Log, TEXT("[PlayKitSettings] Settings saved"));
}
#endif
