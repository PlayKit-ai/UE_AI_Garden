// Copyright PlayKit. All Rights Reserved.

#include "PlayKitBlueprintLibrary.h"
#include "PlayKitSettings.h"
#include "Client/PlayKitPlayerClient.h"
#include "NPC/PlayKitNPCClient.h"

#define PLAYKIT_VERSION TEXT("0.2.0")

bool UPlayKitBlueprintLibrary::IsReady()
{
	UPlayKitSettings* Settings = UPlayKitSettings::Get();
	if (!Settings)
	{
		return false;
	}

	// Check if we have a valid Game ID and authentication
	return !Settings->GameId.IsEmpty() && (Settings->HasDeveloperToken() || !Settings->GetPlayerToken().IsEmpty());
}

FString UPlayKitBlueprintLibrary::GetVersion()
{
	return PLAYKIT_VERSION;
}

UPlayKitPlayerClient* UPlayKitBlueprintLibrary::GetPlayerClient(const UObject* WorldContextObject)
{
	return UPlayKitPlayerClient::Get(WorldContextObject);
}

void UPlayKitBlueprintLibrary::SetupNPC(UPlayKitNPCClient* NPCClient, const FString& ModelName)
{
	if (!NPCClient)
	{
		UE_LOG(LogTemp, Error, TEXT("[PlayKit] NPCClient is null"));
		return;
	}

	UPlayKitSettings* Settings = UPlayKitSettings::Get();
	if (!Settings)
	{
		UE_LOG(LogTemp, Error, TEXT("[PlayKit] Settings not found. Please configure PlayKit in Project Settings."));
		return;
	}

	FString Model = ModelName.IsEmpty() ? Settings->DefaultChatModel : ModelName;
	NPCClient->Setup(Model);
}

FString UPlayKitBlueprintLibrary::GetAuthToken()
{
	UPlayKitSettings* Settings = UPlayKitSettings::Get();
	if (!Settings)
	{
		return TEXT("");
	}

	// Prefer developer token, fallback to player token
	if (Settings->HasDeveloperToken() && !Settings->bIgnoreDeveloperToken)
	{
		return Settings->GetDeveloperToken();
	}

	return Settings->GetPlayerToken();
}

bool UPlayKitBlueprintLibrary::IsAuthenticated()
{
	return !GetAuthToken().IsEmpty();
}

FString UPlayKitBlueprintLibrary::GetGameId()
{
	UPlayKitSettings* Settings = UPlayKitSettings::Get();
	return Settings ? Settings->GameId : TEXT("");
}

FString UPlayKitBlueprintLibrary::GetBaseUrl()
{
	UPlayKitSettings* Settings = UPlayKitSettings::Get();
	return Settings ? Settings->GetBaseUrl() : TEXT("https://api.playkit.ai");
}
