// Copyright PlayKit. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PlayKitNPCActionsModule.generated.h"

/**
 * NPC Action Parameter Type
 */
UENUM(BlueprintType)
enum class ENPCParamType : uint8
{
	String		UMETA(DisplayName = "String"),
	Number		UMETA(DisplayName = "Number"),
	Boolean		UMETA(DisplayName = "Boolean"),
	Enum		UMETA(DisplayName = "Enum")
};

/**
 * NPC Action Parameter Definition
 */
USTRUCT(BlueprintType)
struct FNPCActionParam
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ENPCParamType Type = ENPCParamType::String;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bRequired = true;

	/** Enum options (only used when Type is Enum) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FString> EnumOptions;

	// Fluent builders
	FNPCActionParam& SetName(const FString& InName) { Name = InName; return *this; }
	FNPCActionParam& SetType(ENPCParamType InType) { Type = InType; return *this; }
	FNPCActionParam& SetDescription(const FString& InDesc) { Description = InDesc; return *this; }
	FNPCActionParam& SetRequired(bool bInRequired) { bRequired = bInRequired; return *this; }
	FNPCActionParam& AddEnumOption(const FString& Option) { EnumOptions.Add(Option); return *this; }
};

/**
 * NPC Action Definition
 */
USTRUCT(BlueprintType)
struct FNPCAction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString ActionName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FNPCActionParam> Parameters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bEnabled = true;

	// Fluent builders
	FNPCAction& SetName(const FString& InName) { ActionName = InName; return *this; }
	FNPCAction& SetDescription(const FString& InDesc) { Description = InDesc; return *this; }
	FNPCAction& SetEnabled(bool bInEnabled) { bEnabled = bInEnabled; return *this; }

	FNPCAction& AddStringParam(const FString& Name, const FString& Desc, bool bRequired = true)
	{
		FNPCActionParam Param;
		Param.Name = Name;
		Param.Type = ENPCParamType::String;
		Param.Description = Desc;
		Param.bRequired = bRequired;
		Parameters.Add(Param);
		return *this;
	}

	FNPCAction& AddNumberParam(const FString& Name, const FString& Desc, bool bRequired = true)
	{
		FNPCActionParam Param;
		Param.Name = Name;
		Param.Type = ENPCParamType::Number;
		Param.Description = Desc;
		Param.bRequired = bRequired;
		Parameters.Add(Param);
		return *this;
	}

	FNPCAction& AddBoolParam(const FString& Name, const FString& Desc, bool bRequired = true)
	{
		FNPCActionParam Param;
		Param.Name = Name;
		Param.Type = ENPCParamType::Boolean;
		Param.Description = Desc;
		Param.bRequired = bRequired;
		Parameters.Add(Param);
		return *this;
	}

	FNPCAction& AddEnumParam(const FString& Name, const FString& Desc, const TArray<FString>& Options, bool bRequired = true)
	{
		FNPCActionParam Param;
		Param.Name = Name;
		Param.Type = ENPCParamType::Enum;
		Param.Description = Desc;
		Param.bRequired = bRequired;
		Param.EnumOptions = Options;
		Parameters.Add(Param);
		return *this;
	}
};

/**
 * NPC Action Call Arguments
 */
USTRUCT(BlueprintType)
struct FNPCActionCallArgs
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString ActionName;

	UPROPERTY(BlueprintReadOnly)
	FString CallId;

	UPROPERTY(BlueprintReadOnly)
	TMap<FString, FString> RawParameters;

	/** Get string parameter */
	FString GetString(const FString& ParamName) const
	{
		const FString* Value = RawParameters.Find(ParamName);
		return Value ? *Value : FString();
	}

	/** Get number parameter */
	float GetNumber(const FString& ParamName) const
	{
		const FString* Value = RawParameters.Find(ParamName);
		return Value ? FCString::Atof(**Value) : 0.0f;
	}

	/** Get integer parameter */
	int32 GetInt(const FString& ParamName) const
	{
		const FString* Value = RawParameters.Find(ParamName);
		return Value ? FCString::Atoi(**Value) : 0;
	}

	/** Get boolean parameter */
	bool GetBool(const FString& ParamName) const
	{
		const FString* Value = RawParameters.Find(ParamName);
		if (!Value) return false;
		return Value->Equals(TEXT("true"), ESearchCase::IgnoreCase) || *Value == TEXT("1");
	}

	/** Check if parameter exists */
	bool HasParam(const FString& ParamName) const
	{
		return RawParameters.Contains(ParamName);
	}
};

/**
 * Interface for NPC Action Handlers
 */
UINTERFACE(MinimalAPI, Blueprintable)
class UNPCActionHandler : public UInterface
{
	GENERATED_BODY()
};

class PLAYKITSDK_API INPCActionHandler
{
	GENERATED_BODY()

public:
	/** Get the action definitions this handler provides */
	virtual TArray<FNPCAction> GetActionDefinitions() = 0;

	/** Execute an action and return the result */
	virtual FString Execute(const FNPCActionCallArgs& Args) = 0;
};

/**
 * Base class for synchronous action handlers
 */
UCLASS(Abstract, Blueprintable)
class PLAYKITSDK_API UNPCActionHandlerBase : public UObject, public INPCActionHandler
{
	GENERATED_BODY()

public:
	/** Override to define actions */
	UFUNCTION(BlueprintNativeEvent, Category="PlayKit|NPC|Actions")
	TArray<FNPCAction> GetActionDefinitions();
	virtual TArray<FNPCAction> GetActionDefinitions_Implementation() { return TArray<FNPCAction>(); }

	/** Override to execute actions */
	UFUNCTION(BlueprintNativeEvent, Category="PlayKit|NPC|Actions")
	FString Execute(const FNPCActionCallArgs& Args);
	virtual FString Execute_Implementation(const FNPCActionCallArgs& Args) { return TEXT(""); }
};

/**
 * Action Binding for connecting actions to handlers
 */
USTRUCT(BlueprintType)
struct FNPCActionBinding
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FNPCAction Action;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UNPCActionHandlerBase> HandlerClass;
};

// Delegate for action execution result
DECLARE_DYNAMIC_DELEGATE_RetVal_OneParam(FString, FOnActionExecute, const FNPCActionCallArgs&, Args);

/**
 * NPC Actions Module Component
 * Manages action definitions and execution for an NPC
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PLAYKITSDK_API UPlayKitNPCActionsModule : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayKitNPCActionsModule();

protected:
	virtual void BeginPlay() override;

public:
	/** Register an action with a delegate handler */
	UFUNCTION(BlueprintCallable, Category="PlayKit|NPC|Actions")
	void RegisterAction(const FNPCAction& Action, FOnActionExecute Handler);

	/** Register an action binding */
	UFUNCTION(BlueprintCallable, Category="PlayKit|NPC|Actions")
	void RegisterActionBinding(const FNPCActionBinding& Binding);

	/** Unregister an action */
	UFUNCTION(BlueprintCallable, Category="PlayKit|NPC|Actions")
	void UnregisterAction(const FString& ActionName);

	/** Get all enabled actions */
	UFUNCTION(BlueprintPure, Category="PlayKit|NPC|Actions")
	TArray<FNPCAction> GetEnabledActions() const;

	/** Check if any actions are enabled */
	UFUNCTION(BlueprintPure, Category="PlayKit|NPC|Actions")
	bool HasEnabledActions() const;

	/** Execute an action by name */
	UFUNCTION(BlueprintCallable, Category="PlayKit|NPC|Actions")
	FString ExecuteAction(const FNPCActionCallArgs& Args);

	/** Convert actions to JSON schema for AI */
	UFUNCTION(BlueprintPure, Category="PlayKit|NPC|Actions")
	FString GetActionsAsJsonSchema() const;

public:
	/** Pre-configured action bindings (set in editor) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayKit|NPC|Actions")
	TArray<FNPCActionBinding> ActionBindings;

private:
	struct FRegisteredAction
	{
		FNPCAction Action;
		FOnActionExecute DelegateHandler;
		TSubclassOf<UNPCActionHandlerBase> HandlerClass;
	};

	TMap<FString, FRegisteredAction> RegisteredActions;

	UPROPERTY()
	TMap<FString, UNPCActionHandlerBase*> HandlerInstances;
};
