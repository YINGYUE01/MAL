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
			"SlateCore",
			// AMALCharacterBase implements IGenericTeamAgentInterface, so its header pulls in
			// GenericTeamAgentInterface.h. That makes the module part of our public surface.
			"AIModule",
			// AIController path following and the MoveTo BT task use the navigation system.
			"NavigationSystem"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"EngineCameras"
		});

		// The locomotion layer setup commandlet drives the animation graph editors, so it needs the
		// graph tooling. Editor targets only: the game target never compiles that code.
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] {
				"UnrealEd",
				"Kismet",
				"BlueprintGraph",
				"AnimGraph",
				"AssetTools"
			});
		}

		// Adds the GameplayDebugger dependency and the WITH_GAMEPLAY_DEBUGGER define for the
		// targets that can actually run the in-game debugger, and compiles our page out everywhere
		// else. The page itself lives behind that define.
		SetupGameplayDebuggerSupport(Target);

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
