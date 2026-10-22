// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MAL : ModuleRules
{
	public MAL(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"UMG",
			"ModelViewViewModel",
			"FieldNotification",
			// UNiagaraSystem is exposed on a public cue property, so the module has to be a
			// public dependency for the reflected type to resolve.
			"Niagara",
			// FSlateColor is exposed on a public view model property (damage number colour),
			// so its module has to be a public dependency, not a private one.
			"SlateCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"AIModule",
			"EngineCameras"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
