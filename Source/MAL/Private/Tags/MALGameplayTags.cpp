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

	// Enemy Type Tags
	Enemy_Type = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Enemy.Type")), TEXT("Root of enemy kind tags"));
	Enemy_Type_Boss = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Enemy.Type.Boss")), TEXT("Boss-grade enemy"));
	Enemy_Type_Elite = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Enemy.Type.Elite")), TEXT("Elite enemy"));
	Enemy_Type_Normal = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Enemy.Type.Normal")), TEXT("Ordinary enemy"));

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
	// Per-weapon attack abilities: MAL.Ability.Player.{Weapon}.Attack1..4
	Ability_Player_Sword_Attack1 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Ability.Player.Sword.Attack1")), TEXT("Player sword attack 1"));
	Ability_Player_Sword_Attack2 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Ability.Player.Sword.Attack2")), TEXT("Player sword attack 2"));
	Ability_Player_Sword_Attack3 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Ability.Player.Sword.Attack3")), TEXT("Player sword attack 3"));
	Ability_Player_Sword_Attack4 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Ability.Player.Sword.Attack4")), TEXT("Player sword attack 4"));

	Ability_Player_Axe_Attack1 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Ability.Player.Axe.Attack1")), TEXT("Player axe attack 1"));
	Ability_Player_Axe_Attack2 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Ability.Player.Axe.Attack2")), TEXT("Player axe attack 2"));
	Ability_Player_Axe_Attack3 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Ability.Player.Axe.Attack3")), TEXT("Player axe attack 3"));
	Ability_Player_Axe_Attack4 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Ability.Player.Axe.Attack4")), TEXT("Player axe attack 4"));

	Ability_Player_Dagger_Attack1 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Ability.Player.Dagger.Attack1")), TEXT("Player dagger attack 1"));
	Ability_Player_Dagger_Attack2 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Ability.Player.Dagger.Attack2")), TEXT("Player dagger attack 2"));
	Ability_Player_Dagger_Attack3 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Ability.Player.Dagger.Attack3")), TEXT("Player dagger attack 3"));
	Ability_Player_Dagger_Attack4 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Ability.Player.Dagger.Attack4")), TEXT("Player dagger attack 4"));

	Ability_Player_Hammer_Attack1 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Ability.Player.Hammer.Attack1")), TEXT("Player hammer attack 1"));
	Ability_Player_Hammer_Attack2 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Ability.Player.Hammer.Attack2")), TEXT("Player hammer attack 2"));
	Ability_Player_Hammer_Attack3 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Ability.Player.Hammer.Attack3")), TEXT("Player hammer attack 3"));
	Ability_Player_Hammer_Attack4 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Ability.Player.Hammer.Attack4")), TEXT("Player hammer attack 4"));

	Ability_Player_Scythe_Attack1 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Ability.Player.Scythe.Attack1")), TEXT("Player scythe attack 1"));
	Ability_Player_Scythe_Attack2 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Ability.Player.Scythe.Attack2")), TEXT("Player scythe attack 2"));
	Ability_Player_Scythe_Attack3 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Ability.Player.Scythe.Attack3")), TEXT("Player scythe attack 3"));
	Ability_Player_Scythe_Attack4 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Ability.Player.Scythe.Attack4")), TEXT("Player scythe attack 4"));

	Ability_Player_Spear_Attack1 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Ability.Player.Spear.Attack1")), TEXT("Player spear attack 1"));
	Ability_Player_Spear_Attack2 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Ability.Player.Spear.Attack2")), TEXT("Player spear attack 2"));
	Ability_Player_Spear_Attack3 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Ability.Player.Spear.Attack3")), TEXT("Player spear attack 3"));
	Ability_Player_Spear_Attack4 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Ability.Player.Spear.Attack4")), TEXT("Player spear attack 4"));

	Ability_Player_Bow_Attack1 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Ability.Player.Bow.Attack1")), TEXT("Player bow attack 1"));
	Ability_Player_Bow_Attack2 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Ability.Player.Bow.Attack2")), TEXT("Player bow attack 2"));
	Ability_Player_Bow_Attack3 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Ability.Player.Bow.Attack3")), TEXT("Player bow attack 3"));
	Ability_Player_Bow_Attack4 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Ability.Player.Bow.Attack4")), TEXT("Player bow attack 4"));
	Ability_Enemy_Attack = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Ability.Enemy.Attack")), TEXT("Enemy melee attack"));
	Ability_Common_HitReact = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Ability.Common.HitReact")), TEXT("Hit reaction shared by all characters"));
	
	// Cooldown ownership hierarchy, mirroring the ability tags.
	Cooldown_Player = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Cooldown.Player")), TEXT("Root of player cooldowns"));
	Cooldown_Enemy = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Cooldown.Enemy")), TEXT("Root of enemy cooldowns"));

	// Per-weapon attack cooldowns: MAL.Cooldown.Player.{Weapon}.Attack1..4
	Cooldown_Player_Sword_Attack1 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Cooldown.Player.Sword.Attack1")), TEXT("Player sword attack 1 cooldown"));
	Cooldown_Player_Sword_Attack2 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Cooldown.Player.Sword.Attack2")), TEXT("Player sword attack 2 cooldown"));
	Cooldown_Player_Sword_Attack3 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Cooldown.Player.Sword.Attack3")), TEXT("Player sword attack 3 cooldown"));
	Cooldown_Player_Sword_Attack4 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Cooldown.Player.Sword.Attack4")), TEXT("Player sword attack 4 cooldown"));

	Cooldown_Player_Axe_Attack1 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Cooldown.Player.Axe.Attack1")), TEXT("Player axe attack 1 cooldown"));
	Cooldown_Player_Axe_Attack2 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Cooldown.Player.Axe.Attack2")), TEXT("Player axe attack 2 cooldown"));
	Cooldown_Player_Axe_Attack3 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Cooldown.Player.Axe.Attack3")), TEXT("Player axe attack 3 cooldown"));
	Cooldown_Player_Axe_Attack4 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Cooldown.Player.Axe.Attack4")), TEXT("Player axe attack 4 cooldown"));

	Cooldown_Player_Dagger_Attack1 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Cooldown.Player.Dagger.Attack1")), TEXT("Player dagger attack 1 cooldown"));
	Cooldown_Player_Dagger_Attack2 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Cooldown.Player.Dagger.Attack2")), TEXT("Player dagger attack 2 cooldown"));
	Cooldown_Player_Dagger_Attack3 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Cooldown.Player.Dagger.Attack3")), TEXT("Player dagger attack 3 cooldown"));
	Cooldown_Player_Dagger_Attack4 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Cooldown.Player.Dagger.Attack4")), TEXT("Player dagger attack 4 cooldown"));

	Cooldown_Player_Hammer_Attack1 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Cooldown.Player.Hammer.Attack1")), TEXT("Player hammer attack 1 cooldown"));
	Cooldown_Player_Hammer_Attack2 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Cooldown.Player.Hammer.Attack2")), TEXT("Player hammer attack 2 cooldown"));
	Cooldown_Player_Hammer_Attack3 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Cooldown.Player.Hammer.Attack3")), TEXT("Player hammer attack 3 cooldown"));
	Cooldown_Player_Hammer_Attack4 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Cooldown.Player.Hammer.Attack4")), TEXT("Player hammer attack 4 cooldown"));

	Cooldown_Player_Scythe_Attack1 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Cooldown.Player.Scythe.Attack1")), TEXT("Player scythe attack 1 cooldown"));
	Cooldown_Player_Scythe_Attack2 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Cooldown.Player.Scythe.Attack2")), TEXT("Player scythe attack 2 cooldown"));
	Cooldown_Player_Scythe_Attack3 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Cooldown.Player.Scythe.Attack3")), TEXT("Player scythe attack 3 cooldown"));
	Cooldown_Player_Scythe_Attack4 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Cooldown.Player.Scythe.Attack4")), TEXT("Player scythe attack 4 cooldown"));

	Cooldown_Player_Spear_Attack1 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Cooldown.Player.Spear.Attack1")), TEXT("Player spear attack 1 cooldown"));
	Cooldown_Player_Spear_Attack2 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Cooldown.Player.Spear.Attack2")), TEXT("Player spear attack 2 cooldown"));
	Cooldown_Player_Spear_Attack3 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Cooldown.Player.Spear.Attack3")), TEXT("Player spear attack 3 cooldown"));
	Cooldown_Player_Spear_Attack4 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Cooldown.Player.Spear.Attack4")), TEXT("Player spear attack 4 cooldown"));

	Cooldown_Player_Bow_Attack1 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Cooldown.Player.Bow.Attack1")), TEXT("Player bow attack 1 cooldown"));
	Cooldown_Player_Bow_Attack2 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Cooldown.Player.Bow.Attack2")), TEXT("Player bow attack 2 cooldown"));
	Cooldown_Player_Bow_Attack3 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Cooldown.Player.Bow.Attack3")), TEXT("Player bow attack 3 cooldown"));
	Cooldown_Player_Bow_Attack4 = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Cooldown.Player.Bow.Attack4")), TEXT("Player bow attack 4 cooldown"));
	Cooldown_Enemy_Attack = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Cooldown.Enemy.Attack")), TEXT("Enemy attack cooldown"));


	
	// Input Tags
	Input_Movement_Move = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Input.Movement.Move")), TEXT("Move input"));
	Input_Movement_Look = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Input.Movement.Look")), TEXT("Look input"));
	Input_Movement_Jump = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Input.Movement.Jump")), TEXT("Jump input"));
	Input_Attack_LMB = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Input.Attack.LMB")), TEXT("LMB input (legacy flat tag)"));
	Input_Attack_Q = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Input.Attack.Q")), TEXT("Q input"));
	//Input_PrimaryAttack = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Input.PrimaryAttack")), TEXT("Primary attack action"));
	//Input_SecondaryAttack = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Input.SecondaryAttack")), TEXT("Secondary/heavy attack action"));
	//Input_Skill = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Input.Skill")), TEXT("Extra skill action"));
	Input_Dodge = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Input.Dodge")), TEXT("Dodge action"));
	//Input_Defend = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Input.Defend")), TEXT("Defend input"));
	//Input_Dash = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Input.Dash")), TEXT("Dash input"));
	Input_LockOn = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Input.LockOn")), TEXT("Lock-on toggle"));
	Input_HUD_AttributeMenu = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Input.HUD.AttributeMenu")), TEXT("Opens or closes the attribute menu; not an ability input"));
	Input_HUD_SkillMenu = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Input.HUD.SkillMenu")), TEXT("Opens or closes the skill tree menu; not an ability input"));
	Input_WeaponNext = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Input.WeaponNext")), TEXT("Swaps to the next weapon in the loadout; not an ability input"));
	Input_WeaponPrev = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Input.WeaponPrev")), TEXT("Swaps to the previous weapon in the loadout; not an ability input"));

	// Gameplay Cue Tags
	GameplayCue_Buff_DamageUp = TagManager.AddNativeGameplayTag(FName(TEXT("GameplayCue.Buff.DamageUp")), TEXT("Looping aura shown while the damage buff is active"));
	GameplayCue_Hit_Impact = TagManager.AddNativeGameplayTag(FName(TEXT("GameplayCue.Hit.Impact")), TEXT("Impact feedback fired where a melee hit lands"));
	GameplayCue_Character_LevelUp = TagManager.AddNativeGameplayTag(FName(TEXT("GameplayCue.Character.LevelUp")), TEXT("Level-up feedback, played on every machine that sees the player level"));
}
