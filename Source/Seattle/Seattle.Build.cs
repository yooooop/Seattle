// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Seattle : ModuleRules
{
	public Seattle(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"Seattle",
			"Seattle/Variant_Platforming",
			"Seattle/Variant_Platforming/Animation",
			"Seattle/Variant_Combat",
			"Seattle/Variant_Combat/AI",
			"Seattle/Variant_Combat/Animation",
			"Seattle/Variant_Combat/Gameplay",
			"Seattle/Variant_Combat/Interfaces",
			"Seattle/Variant_Combat/UI",
			"Seattle/Variant_SideScrolling",
			"Seattle/Variant_SideScrolling/AI",
			"Seattle/Variant_SideScrolling/Gameplay",
			"Seattle/Variant_SideScrolling/Interfaces",
			"Seattle/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
