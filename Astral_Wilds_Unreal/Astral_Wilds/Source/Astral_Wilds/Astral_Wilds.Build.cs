// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Astral_Wilds : ModuleRules
{
	public Astral_Wilds(ReadOnlyTargetRules Target) : base(Target)
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
			"Astral_Wilds",
			"Astral_Wilds/Variant_Platforming",
			"Astral_Wilds/Variant_Platforming/Animation",
			"Astral_Wilds/Variant_Combat",
			"Astral_Wilds/Variant_Combat/AI",
			"Astral_Wilds/Variant_Combat/Animation",
			"Astral_Wilds/Variant_Combat/Gameplay",
			"Astral_Wilds/Variant_Combat/Interfaces",
			"Astral_Wilds/Variant_Combat/UI",
			"Astral_Wilds/Variant_SideScrolling",
			"Astral_Wilds/Variant_SideScrolling/AI",
			"Astral_Wilds/Variant_SideScrolling/Gameplay",
			"Astral_Wilds/Variant_SideScrolling/Interfaces",
			"Astral_Wilds/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
