// Copyright PlayKit. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Supported languages for PlayKit Editor
 */
enum class EPlayKitLanguage : uint8
{
	English,
	SimplifiedChinese,
	TraditionalChinese,
	Japanese,
	Korean
};

/**
 * PlayKit Editor Localization System
 * Provides localized strings for the editor UI
 */
class PLAYKITSDKEDITOR_API FPlayKitEditorLocalization
{
public:
	/** Get the singleton instance */
	static FPlayKitEditorLocalization& Get();

	/** Get a localized string by key */
	FText GetText(const FString& Key) const;

	/** Get a localized string with format arguments */
	FText GetTextFormat(const FString& Key, const FFormatNamedArguments& Args) const;

	/** Set the current language */
	void SetLanguage(EPlayKitLanguage Language);

	/** Get the current language */
	EPlayKitLanguage GetLanguage() const { return CurrentLanguage; }

	/** Get language display name */
	static FString GetLanguageDisplayName(EPlayKitLanguage Language);

	/** Get all available languages */
	static TArray<EPlayKitLanguage> GetAvailableLanguages();

	/** Reload localization data */
	void Reload();

	// Constructor - public for MakeUnique but use Get() for access
	FPlayKitEditorLocalization();

private:
	void LoadLocalizationData();
	FString GetLanguageCode() const;

	EPlayKitLanguage CurrentLanguage = EPlayKitLanguage::English;
	TMap<FString, FString> LocalizedStrings;

	static TUniquePtr<FPlayKitEditorLocalization> Instance;
};

/** Shorthand for getting localized text */
#define PLAYKIT_LOC(Key) FPlayKitEditorLocalization::Get().GetText(TEXT(Key))
