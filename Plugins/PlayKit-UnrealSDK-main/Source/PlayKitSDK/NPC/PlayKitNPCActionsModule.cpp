// Copyright PlayKit. All Rights Reserved.

#include "PlayKitNPCActionsModule.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "PlayKitSDK/Tool/PlayKitTool.h"

UPlayKitNPCActionsModule::UPlayKitNPCActionsModule()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPlayKitNPCActionsModule::BeginPlay()
{
	Super::BeginPlay();

	// Register pre-configured bindings
	for (const FNPCActionBinding& Binding : ActionBindings)
	{
		RegisterActionBinding(Binding);
	}
}

void UPlayKitNPCActionsModule::RegisterAction(const FNPCAction& Action, FOnActionExecute Handler)
{
	FRegisteredAction Registered;
	Registered.Action = Action;
	Registered.DelegateHandler = Handler;
	RegisteredActions.Add(Action.ActionName, Registered);

	UE_LOG(LogTemp, Log, TEXT("[ActionsModule] Registered action: %s"), *Action.ActionName);
}

void UPlayKitNPCActionsModule::RegisterActionBinding(const FNPCActionBinding& Binding)
{
	FRegisteredAction Registered;
	Registered.Action = Binding.Action;
	Registered.HandlerClass = Binding.HandlerClass;
	RegisteredActions.Add(Binding.Action.ActionName, Registered);

	UE_LOG(LogTemp, Log, TEXT("[ActionsModule] Registered action binding: %s"), *Binding.Action.ActionName);
}

void UPlayKitNPCActionsModule::UnregisterAction(const FString& ActionName)
{
	RegisteredActions.Remove(ActionName);
	HandlerInstances.Remove(ActionName);

	UE_LOG(LogTemp, Log, TEXT("[ActionsModule] Unregistered action: %s"), *ActionName);
}

TArray<FNPCAction> UPlayKitNPCActionsModule::GetEnabledActions() const
{
	TArray<FNPCAction> EnabledActions;

	for (const auto& Pair : RegisteredActions)
	{
		if (Pair.Value.Action.bEnabled)
		{
			EnabledActions.Add(Pair.Value.Action);
		}
	}

	return EnabledActions;
}

bool UPlayKitNPCActionsModule::HasEnabledActions() const
{
	for (const auto& Pair : RegisteredActions)
	{
		if (Pair.Value.Action.bEnabled)
		{
			return true;
		}
	}
	return false;
}

FString UPlayKitNPCActionsModule::ExecuteAction(const FNPCActionCallArgs& Args)
{
	const FRegisteredAction* Registered = RegisteredActions.Find(Args.ActionName);
	if (!Registered)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActionsModule] Action not found: %s"), *Args.ActionName);
		return FString::Printf(TEXT("Error: Action '%s' not found"), *Args.ActionName);
	}

	// Try delegate handler first
	if (Registered->DelegateHandler.IsBound())
	{
		return Registered->DelegateHandler.Execute(Args);
	}

	// Try class-based handler
	if (Registered->HandlerClass)
	{
		// Get or create handler instance
		UNPCActionHandlerBase** ExistingInstance = HandlerInstances.Find(Args.ActionName);
		UNPCActionHandlerBase* Handler = nullptr;

		if (ExistingInstance)
		{
			Handler = *ExistingInstance;
		}
		else
		{
			Handler = NewObject<UNPCActionHandlerBase>(this, Registered->HandlerClass);
			HandlerInstances.Add(Args.ActionName, Handler);
		}

		if (Handler)
		{
			return Handler->Execute(Args);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[ActionsModule] No handler for action: %s"), *Args.ActionName);
	return FString::Printf(TEXT("Error: No handler for action '%s'"), *Args.ActionName);
}

FString UPlayKitNPCActionsModule::GetActionsAsJsonSchema() const
{
	TArray<TSharedPtr<FJsonValue>> ToolsArray;

	for (const auto& Pair : RegisteredActions)
	{
		const FNPCAction& Action = Pair.Value.Action;
		if (!Action.bEnabled)
		{
			continue;
		}

		// Build function definition
		TSharedPtr<FJsonObject> FunctionObj = MakeShared<FJsonObject>();
		FunctionObj->SetStringField(TEXT("name"), Action.ActionName);
		FunctionObj->SetStringField(TEXT("description"), Action.Description);

		// Build parameters schema
		TSharedPtr<FJsonObject> ParametersObj = MakeShared<FJsonObject>();
		ParametersObj->SetStringField(TEXT("type"), TEXT("object"));

		TSharedPtr<FJsonObject> PropertiesObj = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> RequiredArray;

		for (const FNPCActionParam& Param : Action.Parameters)
		{
			TSharedPtr<FJsonObject> ParamObj = MakeShared<FJsonObject>();

			// Set type
			switch (Param.Type)
			{
			case ENPCParamType::String:
				ParamObj->SetStringField(TEXT("type"), TEXT("string"));
				break;
			case ENPCParamType::Number:
				ParamObj->SetStringField(TEXT("type"), TEXT("number"));
				break;
			case ENPCParamType::Boolean:
				ParamObj->SetStringField(TEXT("type"), TEXT("boolean"));
				break;
			case ENPCParamType::Enum:
				ParamObj->SetStringField(TEXT("type"), TEXT("string"));
				{
					TArray<TSharedPtr<FJsonValue>> EnumValues;
					for (const FString& Option : Param.EnumOptions)
					{
						EnumValues.Add(MakeShared<FJsonValueString>(Option));
					}
					ParamObj->SetArrayField(TEXT("enum"), EnumValues);
				}
				break;
			}

			ParamObj->SetStringField(TEXT("description"), Param.Description);
			PropertiesObj->SetObjectField(Param.Name, ParamObj);

			if (Param.bRequired)
			{
				RequiredArray.Add(MakeShared<FJsonValueString>(Param.Name));
			}
		}

		ParametersObj->SetObjectField(TEXT("properties"), PropertiesObj);
		ParametersObj->SetArrayField(TEXT("required"), RequiredArray);

		FunctionObj->SetObjectField(TEXT("parameters"), ParametersObj);

		// Build tool object
		TSharedPtr<FJsonObject> ToolObj = MakeShared<FJsonObject>();
		ToolObj->SetStringField(TEXT("type"), TEXT("function"));
		ToolObj->SetObjectField(TEXT("function"), FunctionObj);

		ToolsArray.Add(MakeShared<FJsonValueObject>(ToolObj));
	}

	// Serialize to string
	TSharedPtr<FJsonObject> RootObj = MakeShared<FJsonObject>();
	RootObj->SetArrayField(TEXT("tools"), ToolsArray);

	return UPlayKitTool::JsonObjectToString(RootObj, true);
}
