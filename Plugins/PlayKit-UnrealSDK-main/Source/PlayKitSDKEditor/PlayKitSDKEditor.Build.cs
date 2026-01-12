// Copyright PlayKit. All Rights Reserved.

using UnrealBuildTool;

public class PlayKitSDKEditor : ModuleRules
{
	public PlayKitSDKEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"PlayKitSDK",
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore",
				"UnrealEd",
				"EditorStyle",
				"InputCore",
				"ToolMenus",
				"HTTP",
				"Json",
				"JsonUtilities",
				"Projects",
				"DeveloperSettings",
				"EditorSubsystem",
			}
			);
	}
}
