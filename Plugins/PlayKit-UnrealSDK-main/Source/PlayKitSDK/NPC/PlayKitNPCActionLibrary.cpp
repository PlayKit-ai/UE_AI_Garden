// Copyright PlayKit. All Rights Reserved.

#include "PlayKitNPCActionLibrary.h"

//========== FNPCActionCallArgs Helpers ==========//

FString UPlayKitNPCActionLibrary::GetActionString(const FNPCActionCallArgs& Args, const FString& ParamName)
{
	return Args.GetString(ParamName);
}

float UPlayKitNPCActionLibrary::GetActionNumber(const FNPCActionCallArgs& Args, const FString& ParamName)
{
	return Args.GetNumber(ParamName);
}

int32 UPlayKitNPCActionLibrary::GetActionInt(const FNPCActionCallArgs& Args, const FString& ParamName)
{
	return Args.GetInt(ParamName);
}

bool UPlayKitNPCActionLibrary::GetActionBool(const FNPCActionCallArgs& Args, const FString& ParamName)
{
	return Args.GetBool(ParamName);
}

bool UPlayKitNPCActionLibrary::ActionHasParam(const FNPCActionCallArgs& Args, const FString& ParamName)
{
	return Args.HasParam(ParamName);
}

//========== FNPCAction Builders ==========//

FNPCAction UPlayKitNPCActionLibrary::CreateAction(const FString& ActionName, const FString& Description)
{
	FNPCAction Action;
	Action.ActionName = ActionName;
	Action.Description = Description;
	Action.bEnabled = true;
	return Action;
}

FNPCAction UPlayKitNPCActionLibrary::AddStringParameter(const FNPCAction& Action, const FString& Name, const FString& Description, bool bRequired)
{
	FNPCAction Result = Action;
	FNPCActionParam Param;
	Param.Name = Name;
	Param.Type = ENPCParamType::String;
	Param.Description = Description;
	Param.bRequired = bRequired;
	Result.Parameters.Add(Param);
	return Result;
}

FNPCAction UPlayKitNPCActionLibrary::AddNumberParameter(const FNPCAction& Action, const FString& Name, const FString& Description, bool bRequired)
{
	FNPCAction Result = Action;
	FNPCActionParam Param;
	Param.Name = Name;
	Param.Type = ENPCParamType::Number;
	Param.Description = Description;
	Param.bRequired = bRequired;
	Result.Parameters.Add(Param);
	return Result;
}

FNPCAction UPlayKitNPCActionLibrary::AddBoolParameter(const FNPCAction& Action, const FString& Name, const FString& Description, bool bRequired)
{
	FNPCAction Result = Action;
	FNPCActionParam Param;
	Param.Name = Name;
	Param.Type = ENPCParamType::Boolean;
	Param.Description = Description;
	Param.bRequired = bRequired;
	Result.Parameters.Add(Param);
	return Result;
}

FNPCAction UPlayKitNPCActionLibrary::AddEnumParameter(const FNPCAction& Action, const FString& Name, const FString& Description, const TArray<FString>& Options, bool bRequired)
{
	FNPCAction Result = Action;
	FNPCActionParam Param;
	Param.Name = Name;
	Param.Type = ENPCParamType::Enum;
	Param.Description = Description;
	Param.bRequired = bRequired;
	Param.EnumOptions = Options;
	Result.Parameters.Add(Param);
	return Result;
}
