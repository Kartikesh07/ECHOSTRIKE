// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ECHOSTRIKE : ModuleRules
{
	public ECHOSTRIKE(ReadOnlyTargetRules Target) : base(Target)
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
			"ECHOSTRIKE",
			"ECHOSTRIKE/Variant_Platforming",
			"ECHOSTRIKE/Variant_Platforming/Animation",
			"ECHOSTRIKE/Variant_Combat",
			"ECHOSTRIKE/Variant_Combat/AI",
			"ECHOSTRIKE/Variant_Combat/Animation",
			"ECHOSTRIKE/Variant_Combat/Gameplay",
			"ECHOSTRIKE/Variant_Combat/Interfaces",
			"ECHOSTRIKE/Variant_Combat/UI",
			"ECHOSTRIKE/Variant_SideScrolling",
			"ECHOSTRIKE/Variant_SideScrolling/AI",
			"ECHOSTRIKE/Variant_SideScrolling/Gameplay",
			"ECHOSTRIKE/Variant_SideScrolling/Interfaces",
			"ECHOSTRIKE/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
