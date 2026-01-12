// Copyright PlayKit. All Rights Reserved.

using UnrealBuildTool;

public class PlayKitSDK : ModuleRules
{
	public PlayKitSDK(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[] {
				ModuleDirectory,
			}
			);


		PrivateIncludePaths.AddRange(
			new string[] {
				ModuleDirectory,
			}
			);


		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"HTTP",
				"Json",
				"JsonUtilities",
				"AudioMixer",
				"AudioCapture",
				"ImageWrapper",
				"DeveloperSettings",
			}
			);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore",
			}
			);


		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
