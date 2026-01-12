// Copyright PlayKit. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "PlayKitSchemaLibrary.generated.h"

/**
 * Schema Entry Structure
 */
USTRUCT(BlueprintType)
struct FSchemaEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString SchemaJson;
};

/**
 * PlayKit Schema Library
 * Manages JSON schemas for structured output generation
 */
UCLASS(BlueprintType, Blueprintable)
class PLAYKITSDK_API UPlayKitSchemaLibrary : public UObject
{
	GENERATED_BODY()

public:
	UPlayKitSchemaLibrary();

	//========== Schema Management ==========//

	/** Add a schema to the library */
	UFUNCTION(BlueprintCallable, Category="PlayKit|Schema")
	void AddSchema(const FSchemaEntry& Entry);

	/** Add a schema from JSON string */
	UFUNCTION(BlueprintCallable, Category="PlayKit|Schema")
	void AddSchemaFromJson(const FString& Name, const FString& Description, const FString& SchemaJson);

	/** Get a schema by name */
	UFUNCTION(BlueprintPure, Category="PlayKit|Schema")
	FSchemaEntry GetSchema(const FString& Name) const;

	/** Get schema JSON by name */
	UFUNCTION(BlueprintPure, Category="PlayKit|Schema")
	FString GetSchemaJson(const FString& Name) const;

	/** Check if schema exists */
	UFUNCTION(BlueprintPure, Category="PlayKit|Schema")
	bool HasSchema(const FString& Name) const;

	/** Get all schema names */
	UFUNCTION(BlueprintPure, Category="PlayKit|Schema")
	TArray<FString> GetSchemaNames() const;

	/** Get all schemas */
	UFUNCTION(BlueprintPure, Category="PlayKit|Schema")
	TArray<FSchemaEntry> GetAllSchemas() const;

	/** Remove a schema */
	UFUNCTION(BlueprintCallable, Category="PlayKit|Schema")
	bool RemoveSchema(const FString& Name);

	/** Clear all schemas */
	UFUNCTION(BlueprintCallable, Category="PlayKit|Schema")
	void Clear();

	/** Get number of schemas */
	UFUNCTION(BlueprintPure, Category="PlayKit|Schema")
	int32 GetCount() const { return Schemas.Num(); }

	//========== Serialization ==========//

	/** Export library to JSON */
	UFUNCTION(BlueprintCallable, Category="PlayKit|Schema")
	FString ToJson() const;

	/** Import library from JSON */
	UFUNCTION(BlueprintCallable, Category="PlayKit|Schema")
	bool FromJson(const FString& JsonString);

	//========== Factory Methods ==========//

	/** Create a simple object schema */
	UFUNCTION(BlueprintCallable, Category="PlayKit|Schema", meta=(AutoCreateRefTerm="Properties"))
	static FSchemaEntry CreateObjectSchema(
		const FString& Name,
		const FString& Description,
		const TMap<FString, FString>& Properties);

	/** Create an array schema */
	UFUNCTION(BlueprintCallable, Category="PlayKit|Schema")
	static FSchemaEntry CreateArraySchema(
		const FString& Name,
		const FString& Description,
		const FString& ItemType,
		const FString& ItemDescription);

	/** Create an enum schema */
	UFUNCTION(BlueprintCallable, Category="PlayKit|Schema")
	static FSchemaEntry CreateEnumSchema(
		const FString& Name,
		const FString& Description,
		const TArray<FString>& Options);

private:
	UPROPERTY()
	TMap<FString, FSchemaEntry> Schemas;
};
