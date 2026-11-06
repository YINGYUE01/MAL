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
	Status_Dodging = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Status.Dodging")), TEXT("Entity is mid-dodge"));
	Status_Invulnerable = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Status.Invulnerable")), TEXT("Damage is discarded while this is held"));
	Status_HitReacting = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Status.HitReacting")), TEXT("A hit reaction currently owns the body"));

	// Event Tags
	Event_Hit = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Event.Hit")), TEXT("Entity was hit or damaged"));
	Event_HitReact = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Event.HitReact")), TEXT("Non-lethal damage resolved; trigger the hit reaction ability"));
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
	//Ability_Attack = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Ability.Attack")), TEXT("Attack ability (legacy flat tag)"));
	Ability_HitReact = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Ability.HitReact")), TEXT("Shared hit reaction ability (legacy flat tag)"));
	// Ability_Buff = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Ability.Buff")), TEXT("Self-buff ability (legacy flat tag)"));
	// Ability_Dodge = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Ability.Dodge")), TEXT("Dodge ability (legacy flat tag)"));

	// Ownership hierarchy: MAL.Ability.Player.* / Enemy.* / Common.*
	Ability_Player = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Ability.Player")), TEXT("Root of player-owned abilities"));
	Ability_Enemy = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Ability.Enemy")), TEXT("Root of enemy-owned abilities"));
	Ability_Common = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Ability.Common")), TEXT("Root of abilities shared by both sides"));
	Ability_Player_Attack = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Ability.Player.Attack")), TEXT("Player melee attack"));
	Ability_Player_Dodge = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Ability.Player.Dodge")), TEXT("Player dodge/dash"));
	Ability_Player_Buff = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Ability.Player.Buff")), TEXT("Player self-buff"));
	Ability_Enemy_Attack = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Ability.Enemy.Attack")), TEXT("Enemy melee attack"));
	Ability_Common_HitReact = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Ability.Common.HitReact")), TEXT("Hit reaction shared by all characters"));

	// Cooldown Tags
	Cooldown_Attack = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Cooldown.Attack")), TEXT("Attack is on cooldown (legacy flat tag)"));
	Cooldown_Buff = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Cooldown.Buff")), TEXT("Self-buff is on cooldown (legacy flat tag)"));

	// Cooldown ownership hierarchy, mirroring the ability tags.
	Cooldown_Player = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Cooldown.Player")), TEXT("Root of player cooldowns"));
	Cooldown_Enemy = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Cooldown.Enemy")), TEXT("Root of enemy cooldowns"));
	Cooldown_Player_Attack = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Cooldown.Player.Attack")), TEXT("Player attack cooldown"));
	Cooldown_Player_Buff = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Cooldown.Player.Buff")), TEXT("Player buff cooldown"));
	Cooldown_Player_Combo2 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Cooldown.Player.Combo2")), TEXT("Player combo II cooldown"));
	Cooldown_Player_Whirlwind = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Cooldown.Player.Whirlwind")), TEXT("Player whirlwind cooldown"));
	Cooldown_Enemy_Attack = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Cooldown.Enemy.Attack")), TEXT("Enemy attack cooldown"));


	
	// Input Tags
	Input_Movement_Move = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Input.Movement.Move")), TEXT("Move input"));
	Input_Movement_Look = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Input.Movement.Look")), TEXT("Look input"));
	Input_Movement_Jump = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Input.Movement.Jump")), TEXT("Jump input"));
	Input_Attack_LMB = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Input.Attack.LMB")), TEXT("LMB input (legacy flat tag)"));
	//Input_PrimaryAttack = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Input.PrimaryAttack")), TEXT("Primary attack action"));
	//Input_SecondaryAttack = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Input.SecondaryAttack")), TEXT("Secondary/heavy attack action"));
	//Input_Skill = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Input.Skill")), TEXT("Extra skill action"));
	Input_Dodge = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Input.Dodge")), TEXT("Dodge action"));
	//Input_Defend = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Input.Defend")), TEXT("Defend input"));
	//Input_Dash = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Input.Dash")), TEXT("Dash input"));
	Input_LockOn = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Input.LockOn")), TEXT("Lock-on toggle"));
	Input_Attack_Buff = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Input.Attack.Buff")), TEXT("Buff input"));
	Input_HUD_AttributeMenu = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Input.HUD.AttributeMenu")), TEXT("Opens or closes the attribute menu; not an ability input"));
	Input_HUD_SkillMenu = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Input.HUD.SkillMenu")), TEXT("Opens or closes the skill tree menu; not an ability input"));
	Input_WeaponNext = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Input.WeaponNext")), TEXT("Swaps to the next weapon in the loadout; not an ability input"));
	Input_WeaponPrev = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Input.WeaponPrev")), TEXT("Swaps to the previous weapon in the loadout; not an ability input"));

	// Gameplay Cue Tags
	GameplayCue_Buff_DamageUp = TagManager.AddNativeGameplayTag(FName(TEXT("GameplayCue.Buff.DamageUp")), TEXT("Looping aura shown while the damage buff is active"));
	GameplayCue_Hit_Impact = TagManager.AddNativeGameplayTag(FName(TEXT("GameplayCue.Hit.Impact")), TEXT("Impact feedback fired where a melee hit lands"));
	GameplayCue_Character_LevelUp = TagManager.AddNativeGameplayTag(FName(TEXT("GameplayCue.Character.LevelUp")), TEXT("Level-up feedback, played on every machine that sees the player level"));
}
