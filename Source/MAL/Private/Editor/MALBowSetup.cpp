// Copyright Epic Games, Inc. All Rights Reserved.

#include "MALBowSetup.h"

#include "AssetToolsModule.h"
#include "Character/MALCharacterBase.h"
#include "Camera/CameraComponent.h"
#include "Combat/MALProjectile.h"
#include "Combat/MALWeaponComponent.h"
#include "Combat/MALWeaponDefinition.h"
#include "Data/MALAbilitySet.h"
#include "Data/MALClassDefinition.h"
#include "Data/MALSkillTreeDefinition.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/StaticMesh.h"
#include "Factories/BlueprintFactory.h"
#include "Factories/DataAssetFactory.h"
#include "GameAbilities/MALAbility_BowShot.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Tags/MALGameplayTags.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogMALBowSetup, Log, All);

namespace MALBowSetup
{
	const TCHAR* WeaponDefinitionPath = TEXT("/Game/Blueprint/Data/Weapons/Weapon/DA_Weapon_Bow");
	const TCHAR* AbilityFolderPath = TEXT("/Game/Blueprint/GameAbilitySyste/Ability");
	const TCHAR* AbilitySetFolderPath = TEXT("/Game/Blueprint/Data/Weapons/WeaponAbilitySet");
	const TCHAR* PlayerBlueprintPath = TEXT("/Game/Blueprint/Characters/MALCharacter/BP_MALCharacter");
	const TCHAR* ArrowMeshPath = TEXT("/Game/FantasyWeapons/meshes/SM_arrow.SM_arrow");

	const TCHAR* AbilityAssetName = TEXT("GA_BowShotAbility");
	const TCHAR* AbilitySetAssetName = TEXT("DA_AbilitySet_Bow");

	FString AssetPath(const TCHAR* Folder, const TCHAR* Name)
	{
		return FString::Printf(TEXT("%s/%s.%s"), Folder, Name, Name);
	}

	bool SaveAssetPackage(UObject* Asset)
	{
		if (!Asset)
		{
			return false;
		}

		UPackage* Package = Asset->GetOutermost();
		Package->MarkPackageDirty();

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;

		const FString FileName = FPackageName::LongPackageNameToFilename(
			Package->GetName(), FPackageName::GetAssetPackageExtension());

		const bool bSaved = UPackage::SavePackage(Package, Asset, *FileName, SaveArgs);
		UE_CLOG(!bSaved, LogMALBowSetup, Error, TEXT("Failed to save %s."), *Package->GetName());
		return bSaved;
	}

	UBlueprint* CreateBlueprintAsset(const FString& AssetName, const TCHAR* FolderPath, UClass* ParentClass)
	{
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

		UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
		Factory->ParentClass = ParentClass;

		UObject* Created = AssetTools.CreateAsset(AssetName, FolderPath, UBlueprint::StaticClass(), Factory);
		UBlueprint* Blueprint = Cast<UBlueprint>(Created);

		UE_CLOG(!Blueprint, LogMALBowSetup, Error, TEXT("Could not create blueprint %s."), *AssetName);
		return Blueprint;
	}

	UMALAbilitySet* CreateAbilitySetAsset()
	{
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

		UDataAssetFactory* Factory = NewObject<UDataAssetFactory>();
		Factory->DataAssetClass = UMALAbilitySet::StaticClass();

		UObject* Created = AssetTools.CreateAsset(
			AbilitySetAssetName, AbilitySetFolderPath, UMALAbilitySet::StaticClass(), Factory);

		return Cast<UMALAbilitySet>(Created);
	}

	void DumpLoadout()
	{
		const UBlueprint* PlayerBlueprint = LoadObject<UBlueprint>(nullptr, PlayerBlueprintPath);
		const AActor* CharacterCDO = (PlayerBlueprint && PlayerBlueprint->GeneratedClass)
			? Cast<AActor>(PlayerBlueprint->GeneratedClass->GetDefaultObject())
			: nullptr;
		const UMALWeaponComponent* WeaponComponent = CharacterCDO ? CharacterCDO->FindComponentByClass<UMALWeaponComponent>() : nullptr;

		if (!WeaponComponent)
		{
			UE_LOG(LogMALBowSetup, Warning, TEXT("BP_MALCharacter has no weapon component."));
			return;
		}

		// The loadout is private by design; the commandlet reads it through reflection so the
		// component does not grow an accessor that only diagnostics would use.
		FArrayProperty* LoadoutProperty = FindFProperty<FArrayProperty>(UMALWeaponComponent::StaticClass(), TEXT("Loadout"));
		FObjectProperty* ElementProperty = LoadoutProperty ? CastField<FObjectProperty>(LoadoutProperty->Inner) : nullptr;
		if (!ElementProperty)
		{
			UE_LOG(LogMALBowSetup, Warning, TEXT("Could not resolve UMALWeaponComponent::Loadout."));
			return;
		}

		FScriptArrayHelper Helper(LoadoutProperty, LoadoutProperty->ContainerPtrToValuePtr<void>(WeaponComponent));
		UE_LOG(LogMALBowSetup, Display, TEXT("BP_MALCharacter loadout (%d):"), Helper.Num());
		for (int32 Index = 0; Index < Helper.Num(); ++Index)
		{
			const UObject* Entry = ElementProperty->GetObjectPropertyValue(Helper.GetRawPtr(Index));
			UE_LOG(LogMALBowSetup, Display, TEXT("  [%d] %s"), Index, *GetNameSafe(Entry));
		}

		// The class kit is shared by every weapon, so anything it grants on LMB would run alongside
		// the bow shot. Worth printing whenever a new weapon input tag is introduced.
		if (const AMALCharacterBase* Character = Cast<AMALCharacterBase>(CharacterCDO))
		{
			if (const UMALClassDefinition* ClassDefinition = Character->ClassDefinition)
			{
				UE_LOG(LogMALBowSetup, Display, TEXT("Class kit (%s):"), *GetNameSafe(ClassDefinition));
				for (const TObjectPtr<UMALAbilitySet>& Set : ClassDefinition->GrantedAbilitySets)
				{
					for (const FMALAbilitySetEntry& Entry : Set ? Set->Abilities : TArray<FMALAbilitySetEntry>())
					{
						UE_LOG(LogMALBowSetup, Display, TEXT("  %s (input %s)"),
							*GetNameSafe(Entry.AbilityClass), *Entry.OverrideInputTag.ToString());
					}
				}
			}
		}
	}

	void DumpWeapon()
	{
		const UMALWeaponDefinition* Definition = LoadObject<UMALWeaponDefinition>(nullptr, WeaponDefinitionPath);
		if (!Definition)
		{
			UE_LOG(LogMALBowSetup, Error, TEXT("DA_Weapon_Bow not found at %s."), WeaponDefinitionPath);
			return;
		}

		UE_LOG(LogMALBowSetup, Display, TEXT("DA_Weapon_Bow"));
		UE_LOG(LogMALBowSetup, Display, TEXT("  DisplayName      : %s"), *Definition->DisplayName.ToString());
		UE_LOG(LogMALBowSetup, Display, TEXT("  WeaponTag        : %s"), *Definition->WeaponTag.ToString());
		UE_LOG(LogMALBowSetup, Display, TEXT("  WeaponMesh       : %s"), *GetNameSafe(Definition->WeaponMesh));
		UE_LOG(LogMALBowSetup, Display, TEXT("  WeaponSkeletal   : %s"), *GetNameSafe(Definition->WeaponSkeletalMesh));
		UE_LOG(LogMALBowSetup, Display, TEXT("  AttachSocket     : %s"), *Definition->AttachSocketName.ToString());
		UE_LOG(LogMALBowSetup, Display, TEXT("  LocomotionLayer  : %s"), *Definition->LocomotionLayer.ToString());
		UE_LOG(LogMALBowSetup, Display, TEXT("  DamageMultiplier : %.2f"), Definition->DamageMultiplier);
		UE_LOG(LogMALBowSetup, Display, TEXT("  MagazineSize     : %d (-1 = unlimited)"), Definition->MagazineSize);
		UE_LOG(LogMALBowSetup, Display, TEXT("  ProjectileClass  : %s"), *GetNameSafe(Definition->ProjectileClass));
		UE_LOG(LogMALBowSetup, Display, TEXT("  ProjectileMesh   : %s"), *GetNameSafe(Definition->ProjectileMesh));
		UE_LOG(LogMALBowSetup, Display, TEXT("  SpawnSocket      : %s"), *Definition->ProjectileSpawnSocketName.ToString());
		UE_LOG(LogMALBowSetup, Display, TEXT("  SkillTree        : %s"), *GetNameSafe(Definition->SkillTree));

		UE_LOG(LogMALBowSetup, Display, TEXT("  GrantedAbilitySets (%d):"), Definition->GrantedAbilitySets.Num());
		for (const TObjectPtr<UMALAbilitySet>& Set : Definition->GrantedAbilitySets)
		{
			UE_LOG(LogMALBowSetup, Display, TEXT("    %s"), *GetNameSafe(Set));
			if (!Set)
			{
				continue;
			}

			for (const FMALAbilitySetEntry& Entry : Set->Abilities)
			{
				UE_LOG(LogMALBowSetup, Display, TEXT("      %s (input %s)"),
					*GetNameSafe(Entry.AbilityClass), *Entry.OverrideInputTag.ToString());
			}
		}
	}

	void DumpAbility()
	{
		const UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath(AbilityFolderPath, AbilityAssetName));
		if (!Blueprint || !Blueprint->GeneratedClass)
		{
			UE_LOG(LogMALBowSetup, Display, TEXT("%s: not created yet"), AbilityAssetName);
			return;
		}

		const UMALAbility_BowShot* Ability = Cast<UMALAbility_BowShot>(Blueprint->GeneratedClass->GetDefaultObject());
		if (!Ability)
		{
			UE_LOG(LogMALBowSetup, Error, TEXT("%s does not derive from UMALAbility_BowShot."), AbilityAssetName);
			return;
		}

		UE_LOG(LogMALBowSetup, Display, TEXT("%s"), AbilityAssetName);
		UE_LOG(LogMALBowSetup, Display, TEXT("  Parent           : %s"), *GetNameSafe(Blueprint->ParentClass));
		// The console re-encodes log text, so the character count is the reliable read: the
		// intended label is four characters, while a wrongly compiled literal is longer.
		UE_LOG(LogMALBowSetup, Display, TEXT("  DisplayName      : %s (%d chars)"),
			*Ability->DisplayName.ToString(), Ability->DisplayName.ToString().Len());
		UE_LOG(LogMALBowSetup, Display, TEXT("  InputTag         : %s"), *Ability->InputActivatedTag.ToString());
		UE_LOG(LogMALBowSetup, Display, TEXT("  Policy           : %d (0 = OnInputTriggered)"), static_cast<int32>(Ability->ActivationPolicy));
		UE_LOG(LogMALBowSetup, Display, TEXT("  AssetTags        : %s"), *Ability->GetAssetTags().ToStringSimple());
		UE_LOG(LogMALBowSetup, Display, TEXT("  Montage          : %s"), *GetNameSafe(Ability->AbilityMontage));
	}

	/** The aim pose is measured against the authored camera, so print what the blueprint uses. */
	void DumpCamera()
	{
		const UBlueprint* PlayerBlueprint = LoadObject<UBlueprint>(nullptr, PlayerBlueprintPath);
		const AMALCharacterBase* Character = (PlayerBlueprint && PlayerBlueprint->GeneratedClass)
			? Cast<AMALCharacterBase>(PlayerBlueprint->GeneratedClass->GetDefaultObject())
			: nullptr;
		if (!Character)
		{
			UE_LOG(LogMALBowSetup, Warning, TEXT("BP_MALCharacter has no MAL character default object."));
			return;
		}

		const USpringArmComponent* SpringArm = Character->FindComponentByClass<USpringArmComponent>();
		const UCameraComponent* Camera = Character->FindComponentByClass<UCameraComponent>();

		// The camera may live on a parent blueprint or under a renamed component, so list the tree
		// as well: a missing spring arm here is why an aim zoom would do nothing.
		TArray<UActorComponent*> Components;
		Character->GetComponents(Components);
		UE_LOG(LogMALBowSetup, Display, TEXT("BP_MALCharacter components (%d):"), Components.Num());
		for (const UActorComponent* Component : Components)
		{
			UE_LOG(LogMALBowSetup, Display, TEXT("  %s : %s"),
				*GetNameSafe(Component), *GetNameSafe(Component ? Component->GetClass() : nullptr));
		}

		// A blueprint variable that shares its name with an inherited native property makes the
		// blueprint compiler report an internal error instead of creating the property, so both
		// lists are printed before the component tree that the collision would break.
		UE_LOG(LogMALBowSetup, Display, TEXT("BP_MALCharacter blueprint variables (%d):"), PlayerBlueprint->NewVariables.Num());
		for (const FBPVariableDescription& Variable : PlayerBlueprint->NewVariables)
		{
			UE_LOG(LogMALBowSetup, Display, TEXT("  %s : %s"),
				*Variable.VarName.ToString(), *Variable.VarType.PinCategory.ToString());
		}

		if (const USimpleConstructionScript* SCS = PlayerBlueprint->SimpleConstructionScript)
		{
			UE_LOG(LogMALBowSetup, Display, TEXT("BP_MALCharacter SCS nodes (%d):"), SCS->GetAllNodes().Num());
			for (const USCS_Node* Node : SCS->GetAllNodes())
			{
				UE_LOG(LogMALBowSetup, Display, TEXT("  %s : %s (template %s, parent %s)"),
					*Node->GetVariableName().ToString(), *GetNameSafe(Node->ComponentClass),
					*GetNameSafe(Node->ComponentTemplate), *Node->ParentComponentOrVariableName.ToString());
			}
		}

		UE_LOG(LogMALBowSetup, Display, TEXT("BP_MALCharacter camera (authored):"));
		UE_LOG(LogMALBowSetup, Display, TEXT("  SpringArm        : %s"), *GetNameSafe(SpringArm));
		if (SpringArm)
		{
			UE_LOG(LogMALBowSetup, Display, TEXT("    TargetArmLength: %.1f"), SpringArm->TargetArmLength);
			UE_LOG(LogMALBowSetup, Display, TEXT("    SocketOffset   : %s"), *SpringArm->SocketOffset.ToCompactString());
			UE_LOG(LogMALBowSetup, Display, TEXT("    TargetOffset   : %s"), *SpringArm->TargetOffset.ToCompactString());
			UE_LOG(LogMALBowSetup, Display, TEXT("    Lag/Look       : camera lag %d (speed %.1f), pawn control rotation %d"),
				SpringArm->bEnableCameraLag ? 1 : 0, SpringArm->CameraLagSpeed, SpringArm->bUsePawnControlRotation ? 1 : 0);
		}

		UE_LOG(LogMALBowSetup, Display, TEXT("  Camera           : %s"), *GetNameSafe(Camera));
		if (Camera)
		{
			UE_LOG(LogMALBowSetup, Display, TEXT("    FieldOfView    : %.1f"), Camera->FieldOfView);
			UE_LOG(LogMALBowSetup, Display, TEXT("    RelativeLoc    : %s"), *Camera->GetRelativeLocation().ToCompactString());
		}

		// A viewport with no camera component on the pawn falls back to the pawn's eyes, so also
		// report what the mode and the controller contribute.
		const UBlueprint* GameModeBlueprint = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Blueprint/GameMode/BP_GameMode"));
		const AGameModeBase* GameMode = (GameModeBlueprint && GameModeBlueprint->GeneratedClass)
			? Cast<AGameModeBase>(GameModeBlueprint->GeneratedClass->GetDefaultObject())
			: nullptr;
		if (GameMode)
		{
			UE_LOG(LogMALBowSetup, Display, TEXT("BP_GameMode: pawn %s, controller %s"),
				*GetNameSafe(GameMode->DefaultPawnClass), *GetNameSafe(GameMode->PlayerControllerClass));
		}

		const UBlueprint* ControllerBlueprint = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Blueprint/PlayerController/BP_MALPlayerController"));
		const APlayerController* ControllerCDO = (ControllerBlueprint && ControllerBlueprint->GeneratedClass)
			? Cast<APlayerController>(ControllerBlueprint->GeneratedClass->GetDefaultObject())
			: nullptr;
		if (ControllerCDO)
		{
			TArray<UActorComponent*> ControllerComponents;
			ControllerCDO->GetComponents(ControllerComponents);
			UE_LOG(LogMALBowSetup, Display, TEXT("BP_MALPlayerController components (%d):"), ControllerComponents.Num());
			for (const UActorComponent* Component : ControllerComponents)
			{
				UE_LOG(LogMALBowSetup, Display, TEXT("  %s : %s"),
					*GetNameSafe(Component), *GetNameSafe(Component ? Component->GetClass() : nullptr));
			}
		}
	}

	/**
	 * Delete the blueprint's stale camera nodes and recompile the blueprint.
	 *
	 * BP_MALCharacter still carries a CameraBoom and a FollowCamera in its construction
	 * script, but the camera was never attached to the boom (the node has no parent) and the
	 * boom never used the pawn's control rotation, so the pawn ended up viewed from a point
	 * sitting on the character. The rig now lives in AMALCharacter; the blueprint nodes only
	 * shadow those native properties, which makes the blueprint compiler fail to create them
	 * ("Tried to create a property FollowCamera ... already exists there").
	 */
	void RemoveBlueprintCameraNodes()
	{
		UBlueprint* PlayerBlueprint = LoadObject<UBlueprint>(nullptr, PlayerBlueprintPath);
		if (!PlayerBlueprint || !PlayerBlueprint->SimpleConstructionScript)
		{
			UE_LOG(LogMALBowSetup, Warning, TEXT("BP_MALCharacter has no construction script."));
			return;
		}

		USimpleConstructionScript* SCS = PlayerBlueprint->SimpleConstructionScript;

		TArray<USCS_Node*> CameraNodes;
		for (USCS_Node* Node : SCS->GetAllNodes())
		{
			if (Node && (Node->GetVariableName() == TEXT("CameraBoom") || Node->GetVariableName() == TEXT("FollowCamera")))
			{
				CameraNodes.Add(Node);
			}
		}

		if (CameraNodes.Num() == 0)
		{
			UE_LOG(LogMALBowSetup, Display, TEXT("BP_MALCharacter has no camera nodes left in its construction script."));
			return;
		}

		PlayerBlueprint->Modify();
		for (USCS_Node* Node : CameraNodes)
		{
			UE_LOG(LogMALBowSetup, Display, TEXT("Removing blueprint camera node %s (%s)."),
				*Node->GetVariableName().ToString(), *GetNameSafe(Node->ComponentClass));

			// Promote children rather than delete them: the nodes carry no camera children today,
			// but the call keeps a future attachment from being removed along with its parent.
			SCS->RemoveNodeAndPromoteChildren(Node);
		}

		FKismetEditorUtilities::CompileBlueprint(PlayerBlueprint, EBlueprintCompileOptions::SkipGarbageCollection);
		SaveAssetPackage(PlayerBlueprint);

		UE_LOG(LogMALBowSetup, Display, TEXT("BP_MALCharacter recompiled; status %d (3 = up to date, 2 = error)."),
			static_cast<int32>(PlayerBlueprint->Status));
	}

	void DumpAbilitySet()
	{
		const UMALAbilitySet* AbilitySet = LoadObject<UMALAbilitySet>(nullptr, *AssetPath(AbilitySetFolderPath, AbilitySetAssetName));
		if (!AbilitySet)
		{
			UE_LOG(LogMALBowSetup, Display, TEXT("%s: not created yet"), AbilitySetAssetName);
			return;
		}

		UE_LOG(LogMALBowSetup, Display, TEXT("%s (%d entries)"), AbilitySetAssetName, AbilitySet->Abilities.Num());
		for (const FMALAbilitySetEntry& Entry : AbilitySet->Abilities)
		{
			UE_LOG(LogMALBowSetup, Display, TEXT("  %s level %d input %s"),
				*GetNameSafe(Entry.AbilityClass), Entry.StartingLevel, *Entry.OverrideInputTag.ToString());
		}
	}

	/** Create GA_BowShotAbility if needed and write the values this commandlet owns. */
	UBlueprint* EnsureAbilityBlueprint()
	{
		UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath(AbilityFolderPath, AbilityAssetName));
		if (!Blueprint)
		{
			Blueprint = CreateBlueprintAsset(AbilityAssetName, AbilityFolderPath, UMALAbility_BowShot::StaticClass());
		}

		if (!Blueprint || !Blueprint->GeneratedClass)
		{
			return nullptr;
		}

		const UMALGameplayTags* Tags = UMALGameplayTags::Get();
		UMALAbility_BowShot* Ability = Cast<UMALAbility_BowShot>(Blueprint->GeneratedClass->GetDefaultObject());
		if (!Ability)
		{
			UE_LOG(LogMALBowSetup, Error, TEXT("%s is not a UMALAbility_BowShot blueprint."), AbilityAssetName);
			return nullptr;
		}

		// Written every run so the asset can be repaired by rerunning the commandlet rather than
		// by hand. Values a designer is expected to tune (damage, tiers, montage) are left alone.
		Ability->Modify();
		Ability->InputActivatedTag = Tags->Input_LMB;
		// Escaped code points: this file has no BOM, so a raw literal would be compiled in the
		// system code page and saved into the asset as mojibake.
		Ability->DisplayName = FText::FromString(FString(TEXT("\u84c4\u529b\u5c04\u51fb")));
		Blueprint->MarkPackageDirty();

		return Blueprint;
	}

	UMALAbilitySet* EnsureAbilitySet(UClass* AbilityClass)
	{
		UMALAbilitySet* AbilitySet = LoadObject<UMALAbilitySet>(nullptr, *AssetPath(AbilitySetFolderPath, AbilitySetAssetName));
		if (!AbilitySet)
		{
			AbilitySet = CreateAbilitySetAsset();
		}

		if (!AbilitySet)
		{
			return nullptr;
		}

		AbilitySet->Modify();

		const UMALGameplayTags* Tags = UMALGameplayTags::Get();

		bool bHasEntry = false;
		for (const FMALAbilitySetEntry& Entry : AbilitySet->Abilities)
		{
			if (Entry.AbilityClass == AbilityClass)
			{
				bHasEntry = true;
				break;
			}
		}

		if (!bHasEntry)
		{
			FMALAbilitySetEntry& Entry = AbilitySet->Abilities.AddDefaulted_GetRef();
			Entry.AbilityClass = AbilityClass;
			Entry.StartingLevel = 1;
			// Spelled out rather than inherited from the CDO: the bar and the grant code read this
			// tag, and an override that disagrees with the ability is invisible until someone shoots.
			Entry.OverrideInputTag = Tags->Input_LMB;
		}

		return AbilitySet;
	}

	void ConfigureWeapon(UMALAbilitySet* AbilitySet)
	{
		UMALWeaponDefinition* Definition = LoadObject<UMALWeaponDefinition>(nullptr, WeaponDefinitionPath);
		if (!Definition)
		{
			UE_LOG(LogMALBowSetup, Error, TEXT("DA_Weapon_Bow not found at %s."), WeaponDefinitionPath);
			return;
		}

		const UMALGameplayTags* Tags = UMALGameplayTags::Get();
		Definition->Modify();

		// Identity is written unconditionally: the asset was duplicated from the sword and still
		// carries MAL.Weapon.Sword, which would file its skill purchases under the blade family.
		Definition->WeaponTag = Tags->Weapon_Bow;
		if (!Definition->DisplayName.ToString().Contains(TEXT("Bow"))
			&& !Definition->DisplayName.ToString().Contains(TEXT("\u5f13")))
		{
			UE_LOG(LogMALBowSetup, Display, TEXT("DA_Weapon_Bow DisplayName was '%s'."), *Definition->DisplayName.ToString());
			Definition->DisplayName = FText::FromString(TEXT("Bow"));
		}

		// Mesh, socket and locomotion layer stay as authored.

		if (AbilitySet && !Definition->GrantedAbilitySets.Contains(AbilitySet))
		{
			Definition->GrantedAbilitySets.Add(AbilitySet);
		}

		if (!Definition->ProjectileClass)
		{
			Definition->ProjectileClass = AMALProjectile::StaticClass();
		}

		if (!Definition->ProjectileMesh)
		{
			Definition->ProjectileMesh = LoadObject<UStaticMesh>(nullptr, ArrowMeshPath);
			UE_CLOG(!Definition->ProjectileMesh, LogMALBowSetup, Warning, TEXT("Arrow mesh %s not found."), ArrowMeshPath);
		}

		Definition->MarkPackageDirty();
		SaveAssetPackage(Definition);

		UE_LOG(LogMALBowSetup, Display, TEXT("DA_Weapon_Bow: tag %s, %d ability set(s), projectile %s"),
			*Definition->WeaponTag.ToString(),
			Definition->GrantedAbilitySets.Num(),
			*GetNameSafe(Definition->ProjectileClass));
	}
}

UMALBowSetupCommandlet::UMALBowSetupCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UMALBowSetupCommandlet::Main(const FString& Params)
{
	FString Mode = TEXT("dump");
	FParse::Value(*Params, TEXT("Mode="), Mode);

	UE_LOG(LogMALBowSetup, Display, TEXT("=== MAL bow setup (%s) ==="), *Mode);

	MALBowSetup::DumpWeapon();
	MALBowSetup::DumpAbilitySet();
	MALBowSetup::DumpAbility();
	MALBowSetup::DumpLoadout();
	MALBowSetup::DumpCamera();

	if (Mode.Equals(TEXT("fixcamera"), ESearchCase::IgnoreCase))
	{
		MALBowSetup::RemoveBlueprintCameraNodes();
		MALBowSetup::DumpCamera();
		return 0;
	}

	if (!Mode.Equals(TEXT("apply"), ESearchCase::IgnoreCase))
	{
		return 0;
	}

	UBlueprint* AbilityBlueprint = MALBowSetup::EnsureAbilityBlueprint();
	if (!AbilityBlueprint)
	{
		UE_LOG(LogMALBowSetup, Error, TEXT("Aborting: %s could not be created."), MALBowSetup::AbilityAssetName);
		return 1;
	}

	FKismetEditorUtilities::CompileBlueprint(AbilityBlueprint);
	MALBowSetup::SaveAssetPackage(AbilityBlueprint);

	UMALAbilitySet* AbilitySet = MALBowSetup::EnsureAbilitySet(AbilityBlueprint->GeneratedClass);
	if (!AbilitySet)
	{
		UE_LOG(LogMALBowSetup, Error, TEXT("Aborting: %s could not be created."), MALBowSetup::AbilitySetAssetName);
		return 1;
	}
	MALBowSetup::SaveAssetPackage(AbilitySet);

	MALBowSetup::ConfigureWeapon(AbilitySet);

	UE_LOG(LogMALBowSetup, Display, TEXT("=== done; rerun with -Mode=dump to verify ==="));
	return 0;
}
