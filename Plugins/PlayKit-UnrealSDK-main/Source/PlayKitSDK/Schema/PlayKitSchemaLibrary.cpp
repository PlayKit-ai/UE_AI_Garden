// Copyright PlayKit. All Rights Reserved.

#include "PlayKitSchemaLibrary.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "PlayKitSDK/Tool/PlayKitTool.h"

UPlayKitSchemaLibrary::UPlayKitSchemaLibrary()
{
}

//========== Schema Management ==========//

void UPlayKitSchemaLibrary::AddSchema(const FSchemaEntry& Entry)
{
	Schemas.Add(Entry.Name, Entry);
	UE_LOG(LogTemp, Log, TEXT("[SchemaLibrary] Added schema: %s"), *Entry.Name);
}

void UPlayKitSchemaLibrary::AddSchemaFromJson(const FString& Name, const FString& Description, const FString& SchemaJson)
{
	FSchemaEntry Entry;
	Entry.Name = Name;
	Entry.Description = Description;
	Entry.SchemaJson = SchemaJson;
	AddSchema(Entry);
}

FSchemaEntry UPlayKitSchemaLibrary::GetSchema(const FString& Name) const
{
	const FSchemaEntry* Found = Schemas.Find(Name);
	return Found ? *Found : FSchemaEntry();
}

FString UPlayKitSchemaLibrary::GetSchemaJson(const FString& Name) const
{
	const FSchemaEntry* Found = Schemas.Find(Name);
	return Found ? Found->SchemaJson : FString();
}

bool UPlayKitSchemaLibrary::HasSchema(const FString& Name) const
{
	return Schemas.Contains(Name);
}

TArray<FString> UPlayKitSchemaLibrary::GetSchemaNames() const
{
	TArray<FString> Names;
	Schemas.GetKeys(Names);
	return Names;
}

TArray<FSchemaEntry> UPlayKitSchemaLibrary::GetAllSchemas() const
{
	TArray<FSchemaEntry> AllSchemas;
	for (const auto& Pair : Schemas)
	{
		AllSchemas.Add(Pair.Value);
	}
	return AllSchemas;
}

bool UPlayKitSchemaLibrary::RemoveSchema(const FString& Name)
{
	int32 Removed = Schemas.Remove(Name);
	if (Removed > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[SchemaLibrary] Removed schema: %s"), *Name);
	}
	return Removed > 0;
}

void UPlayKitSchemaLibrary::Clear()
{
	Schemas.Empty();
	UE_LOG(LogTemp, Log, TEXT("[SchemaLibrary] Cleared all schemas"));
}

//========== Serialization ==========//

FString UPlayKitSchemaLibrary::ToJson() const
{
	TSharedPtr<FJsonObject> RootObj = MakeShared<FJsonObject>();

	for (const auto& Pair : Schemas)
	{
		TSharedPtr<FJsonObject> EntryObj = MakeShared<FJsonObject>();
		EntryObj->SetStringField(TEXT("name"), Pair.Value.Name);
		EntryObj->SetStringField(TEXT("description"), Pair.Value.Description);
		EntryObj->SetStringField(TEXT("schema"), Pair.Value.SchemaJson);
		RootObj->SetObjectField(Pair.Key, EntryObj);
	}

	return UPlayKitTool::JsonObjectToString(RootObj, true);
}

bool UPlayKitSchemaLibrary::FromJson(const FString& JsonString)
{
	TSharedPtr<FJsonObject> RootObj;
	if (!UPlayKitTool::StringToJsonObject(JsonString, RootObj, true))
	{
		return false;
	}

	Schemas.Empty();

	for (const auto& Pair : RootObj->Values)
	{
		TSharedPtr<FJsonObject> EntryObj = Pair.Value->AsObject();
		if (EntryObj.IsValid())
		{
			FSchemaEntry Entry;
			Entry.Name = EntryObj->GetStringField(TEXT("name"));
			Entry.Description = EntryObj->GetStringField(TEXT("description"));
			Entry.SchemaJson = EntryObj->GetStringField(TEXT("schema"));
			Schemas.Add(Entry.Name, Entry);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[SchemaLibrary] Loaded %d schemas from JSON"), Schemas.Num());
	return true;
}

//========== Factory Methods ==========//

FSchemaEntry UPlayKitSchemaLibrary::CreateObjectSchema(
	const FString& Name,
	const FString& Description,
	const TMap<FString, FString>& Properties)
{
	TSharedPtr<FJsonObject> SchemaObj = MakeShared<FJsonObject>();
	SchemaObj->SetStringField(TEXT("type"), TEXT("object"));
	SchemaObj->SetStringField(TEXT("description"), Description);

	TSharedPtr<FJsonObject> PropertiesObj = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> RequiredArray;

	for (const auto& Pair : Properties)
	{
		TSharedPtr<FJsonObject> PropObj = MakeShared<FJsonObject>();
		PropObj->SetStringField(TEXT("type"), TEXT("string"));
		PropObj->SetStringField(TEXT("description"), Pair.Value);
		PropertiesObj->SetObjectField(Pair.Key, PropObj);
		RequiredArray.Add(MakeShared<FJsonValueString>(Pair.Key));
	}

	SchemaObj->SetObjectField(TEXT("properties"), PropertiesObj);
	SchemaObj->SetArrayField(TEXT("required"), RequiredArray);

	FSchemaEntry Entry;
	Entry.Name = Name;
	Entry.Description = Description;
	Entry.SchemaJson = UPlayKitTool::JsonObjectToString(SchemaObj);

	return Entry;
}

FSchemaEntry UPlayKitSchemaLibrary::CreateArraySchema(
	const FString& Name,
	const FString& Description,
	const FString& ItemType,
	const FString& ItemDescription)
{
	TSharedPtr<FJsonObject> SchemaObj = MakeShared<FJsonObject>();
	SchemaObj->SetStringField(TEXT("type"), TEXT("array"));
	SchemaObj->SetStringField(TEXT("description"), Description);

	TSharedPtr<FJsonObject> ItemsObj = MakeShared<FJsonObject>();
	ItemsObj->SetStringField(TEXT("type"), ItemType);
	ItemsObj->SetStringField(TEXT("description"), ItemDescription);
	SchemaObj->SetObjectField(TEXT("items"), ItemsObj);

	FSchemaEntry Entry;
	Entry.Name = Name;
	Entry.Description = Description;
	Entry.SchemaJson = UPlayKitTool::JsonObjectToString(SchemaObj);

	return Entry;
}

FSchemaEntry UPlayKitSchemaLibrary::CreateEnumSchema(
	const FString& Name,
	const FString& Description,
	const TArray<FString>& Options)
{
	TSharedPtr<FJsonObject> SchemaObj = MakeShared<FJsonObject>();
	SchemaObj->SetStringField(TEXT("type"), TEXT("string"));
	SchemaObj->SetStringField(TEXT("description"), Description);

	TArray<TSharedPtr<FJsonValue>> EnumArray;
	for (const FString& Option : Options)
	{
		EnumArray.Add(MakeShared<FJsonValueString>(Option));
	}
	SchemaObj->SetArrayField(TEXT("enum"), EnumArray);

	FSchemaEntry Entry;
	Entry.Name = Name;
	Entry.Description = Description;
	Entry.SchemaJson = UPlayKitTool::JsonObjectToString(SchemaObj);

	return Entry;
}
