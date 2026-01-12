// Copyright PlayKit. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PlayKitNPCActionsModule.h"
#include "PlayKitNPCActionLibrary.generated.h"

/**
 * Blueprint Function Library for NPC Action helpers
 */
UCLASS()
class PLAYKITSDK_API UPlayKitNPCActionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	//========== FNPCActionCallArgs Helpers ==========//

	/** Get string parameter from action args */
	UFUNCTION(BlueprintPure, Category="PlayKit|NPC|Actions", meta=(DisplayName="Get String Parameter"))
	static FString GetActionString(const FNPCActionCallArgs& Args, const FString& ParamName);

	/** Get number parameter from action args */
	UFUNCTION(BlueprintPure, Category="PlayKit|NPC|Actions", meta=(DisplayName="Get Number Parameter"))
	static float GetActionNumber(const FNPCActionCallArgs& Args, const FString& ParamName);

	/** Get integer parameter from action args */
	UFUNCTION(BlueprintPure, Category="PlayKit|NPC|Actions", meta=(DisplayName="Get Int Parameter"))
	static int32 GetActionInt(const FNPCActionCallArgs& Args, const FString& ParamName);

	/** Get boolean parameter from action args */
	UFUNCTION(BlueprintPure, Category="PlayKit|NPC|Actions", meta=(DisplayName="Get Bool Parameter"))
	static bool GetActionBool(const FNPCActionCallArgs& Args, const FString& ParamName);

	/** Check if parameter exists in action args */
	UFUNCTION(BlueprintPure, Category="PlayKit|NPC|Actions", meta=(DisplayName="Has Parameter"))
	static bool ActionHasParam(const FNPCActionCallArgs& Args, const FString& ParamName);

	//========== FNPCAction Builders ==========//

	/** Create a new action definition */
	UFUNCTION(BlueprintCallable, Category="PlayKit|NPC|Actions", meta=(DisplayName="Create Action"))
	static FNPCAction CreateAction(const FString& ActionName, const FString& Description);

	/** Add a string parameter to an action */
	UFUNCTION(BlueprintCallable, Category="PlayKit|NPC|Actions", meta=(DisplayName="Add String Param"))
	static FNPCAction AddStringParameter(const FNPCAction& Action, const FString& Name, const FString& Description, bool bRequired = true);

	/** Add a number parameter to an action */
	UFUNCTION(BlueprintCallable, Category="PlayKit|NPC|Actions", meta=(DisplayName="Add Number Param"))
	static FNPCAction AddNumberParameter(const FNPCAction& Action, const FString& Name, const FString& Description, bool bRequired = true);

	/** Add a boolean parameter to an action */
	UFUNCTION(BlueprintCallable, Category="PlayKit|NPC|Actions", meta=(DisplayName="Add Bool Param"))
	static FNPCAction AddBoolParameter(const FNPCAction& Action, const FString& Name, const FString& Description, bool bRequired = true);

	/** Add an enum parameter to an action */
	UFUNCTION(BlueprintCallable, Category="PlayKit|NPC|Actions", meta=(DisplayName="Add Enum Param"))
	static FNPCAction AddEnumParameter(const FNPCAction& Action, const FString& Name, const FString& Description, const TArray<FString>& Options, bool bRequired = true);
};
