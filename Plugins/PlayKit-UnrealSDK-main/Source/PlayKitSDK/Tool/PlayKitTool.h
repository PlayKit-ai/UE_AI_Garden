// Copyright PlayKit. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture2D.h"
#include "Misc/Base64.h"
#include "ImageUtils.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "PlayKitTool.generated.h"

/**
 * PlayKit Utility Functions
 */
UCLASS()
class PLAYKITSDK_API UPlayKitTool : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	// Convert JSON object to string
	static FString JsonObjectToString(const TSharedPtr<FJsonObject>& JsonObject, bool bPrettyPrint = false)
	{
		if (!JsonObject.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("JsonObjectToString: Invalid JSON object"));
			return FString();
		}

		FString OutputString;

		if (bPrettyPrint)
		{
			// Pretty print with indentation
			TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
				TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutputString);
			FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
		}
		else
		{
			// Compact output
			TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
			FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
		}

		return OutputString;
	}

	// Convert string to JSON object
	static bool StringToJsonObject(const FString& JsonString, TSharedPtr<FJsonObject>& OutJsonObject, bool bLogErrors = true)
	{
		if (JsonString.IsEmpty())
		{
			if (bLogErrors)
			{
				UE_LOG(LogTemp, Warning, TEXT("StringToJsonObject: Input string is empty"));
			}
			return false;
		}

		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

		if (FJsonSerializer::Deserialize(Reader, OutJsonObject) && OutJsonObject.IsValid())
		{
			return true;
		}
		else
		{
			if (bLogErrors)
			{
				UE_LOG(LogTemp, Error, TEXT("StringToJsonObject: Failed to parse JSON string: %s"), *JsonString);
			}
			return false;
		}
	}

	UFUNCTION(BlueprintCallable, Category="PlayKit|Tools")
	static UTexture2D* Texture2DFromBase64(const FString& Base64String)
	{
		UTexture2D* Texture2D = nullptr;
		if (Base64String.IsEmpty()) return Texture2D;

		TArray<uint8> RawData;

		// Decode Base64 string to binary data
		if (!FBase64::Decode(Base64String, RawData))
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to decode Base64 string"));
			return nullptr;
		}

		// Create texture
		Texture2D = FImageUtils::ImportBufferAsTexture2D(RawData);

		if (!Texture2D)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to create texture from binary data"));
		}

		return Texture2D;
	}
};
