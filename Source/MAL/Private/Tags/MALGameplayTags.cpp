// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tags/MALGameplayTags.h"
#include "GameplayTagsManager.h"

// Initialize static singleton pointer
UMALGameplayTags* UMALGameplayTags::TagsSingleton = nullptr;

UMALGameplayTags* UMALGameplayTags::Get()
{
	if (!TagsSingleton)
	{
		TagsSingleton = NewObject<UMALGameplayTags>();
		TagsSingleton->AddToRoot();
		TagsSingleton->InitializeNativeTags();
	}

	return TagsSingleton;
}

void UMALGameplayTags::InitializeNativeTags()
{
	UGameplayTagsManager& TagManager = UGameplayTagsManager::Get();

	// Status Tags
	Status_Dead = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Status.Dead")), TEXT("Entity is dead"));
	Status_Attacking = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Status.Attacking")), TEXT("Entity is performing an attack"));
	Status_Casting = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Status.Casting")), TEXT("Entity is casting and cannot move"));
	Status_Aiming = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Status.Aiming")), TEXT("Entity is aiming; movement is allowed but slower"));

	// Event Tags
	Event_Hit = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Event.Hit")), TEXT("Entity was hit or damaged"));
	Event_Death = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Event.Death")), TEXT("Entity died"));
	Event_WeaponHit = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Event.WeaponHit")), TEXT("Weapon collision detected a hit"));

	// Weapon Tags
	Weapon_Sword = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Weapon.Sword")), TEXT("One-handed blade family"));
	Weapon_Axe = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Weapon.Axe")), TEXT("Axe family"));
	Weapon_Dagger = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Weapon.Dagger")), TEXT("Dagger family"));
	Weapon_Hammer = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Weapon.Hammer")), TEXT("Hammer family"));
	Weapon_Scythe = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Weapon.Scythe")), TEXT("Scythe family"));
	Weapon_Spear = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Weapon.Spear")), TEXT("Spear family"));
	Weapon_Bow = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Weapon.Bow")), TEXT("Bow family"));

	// Data Tags
	Data_Damage = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Data.Damage")), TEXT("Damage attribute"));
	Data_DamageBonus = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Data.DamageBonus")), TEXT("Damage bonus percentage carried by buff GEs"));

	// Damage Type Tags
	Damage_Type_Magic = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Damage.Type.Magic")), TEXT("Magic damage channel"));

	// Ability Tags
	Ability_Attack = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Ability.Attack")), TEXT("Attack ability"));
	Ability_Buff = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Ability.Buff")), TEXT("Self-buff ability"));
	Ability_BowShot = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Ability.BowShot")), TEXT("Charged bow shot"));

	// Cooldown Tags
	Cooldown_Attack = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Cooldown.Attack")), TEXT("Attack is on cooldown"));
	Cooldown_Buff = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Cooldown.Buff")), TEXT("Self-buff is on cooldown"));

	// Input Tags
	Input_Move = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Input.Move")), TEXT("Move input"));
	Input_Look = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Input.Look")), TEXT("Look input"));
	Input_Jump = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Input.Jump")), TEXT("Jump input"));
	Input_LMB = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Input.LMB")), TEXT("LMB input"));
	Input_Defend = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Input.Defend")), TEXT("Defend input"));
	Input_Dash = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Input.Dash")), TEXT("Dash input"));
	Input_Buff = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Input.Buff")), TEXT("Buff input"));
	Input_AttributeMenu = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Input.AttributeMenu")), TEXT("Opens or closes the attribute menu; not an ability input"));
	Input_SkillMenu = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Input.SkillMenu")), TEXT("Opens or closes the skill tree menu; not an ability input"));
	Input_WeaponNext = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Input.WeaponNext")), TEXT("Swaps to the next weapon in the loadout; not an ability input"));
	Input_WeaponPrev = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Input.WeaponPrev")), TEXT("Swaps to the previous weapon in the loadout; not an ability input"));

	// Gameplay Cue Tags
	GameplayCue_Buff_DamageUp = TagManager.AddNativeGameplayTag(FName(TEXT("GameplayCue.Buff.DamageUp")), TEXT("Looping aura shown while the damage buff is active"));
	GameplayCue_Hit_Impact = TagManager.AddNativeGameplayTag(FName(TEXT("GameplayCue.Hit.Impact")), TEXT("Impact feedback fired where a melee hit lands"));
	GameplayCue_Character_LevelUp = TagManager.AddNativeGameplayTag(FName(TEXT("GameplayCue.Character.LevelUp")), TEXT("Level-up feedback, played on every machine that sees the player level"));
}
