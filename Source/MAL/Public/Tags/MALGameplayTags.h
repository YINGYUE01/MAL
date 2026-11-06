// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "GameplayTagContainer.h"
#include "MALGameplayTags.generated.h"

/**
 * UMALGameplayTags
 * 
 * Singleton class that manages all gameplay tags used in the MAL project.
 * Tags are registered during initialization and can be accessed globally via Get().
 */
UCLASS()
class MAL_API UMALGameplayTags : public UObject
{
	GENERATED_BODY()

public:
	/** Get the singleton instance, creating it if necessary */
	static UMALGameplayTags* Get();

	/** Initialize all native gameplay tags */
	void InitializeNativeTags();

	// -----------------------------------------------------------------------------
	// Status Tags - Represent the current state of an entity
	// -----------------------------------------------------------------------------
	
	/** Entity is dead */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Status")
	FGameplayTag Status_Dead;

	/** Entity is attacking */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Status")
	FGameplayTag Status_Attacking;

	/** Entity is casting an ability and cannot move */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Status")
	FGameplayTag Status_Casting;

	/** Entity is mid-dodge; movement is locked to the dodge impulse. */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Status")
	FGameplayTag Status_Dodging;

	/** Damage arriving while this is held is discarded. Owned by the dodge window. */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Status")
	FGameplayTag Status_Invulnerable;

	/** A hit reaction ability currently owns the body; further hits do not restart it. */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Status")
	FGameplayTag Status_HitReacting;

	// -----------------------------------------------------------------------------
	// Event Tags - Represent events that can be triggered
	// -----------------------------------------------------------------------------
	
	/** Entity was hit/damaged */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Event")
	FGameplayTag Event_Hit;

	/** Non-lethal damage resolved; activates the shared hit reaction ability. */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Event")
	FGameplayTag Event_HitReact;

	/** Entity died */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Event")
	FGameplayTag Event_Death;

	/** Weapon collision detected a hit */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Event")
	FGameplayTag Event_WeaponHit;

	// -----------------------------------------------------------------------------
	// Weapon Tags - What kind of weapon is equipped
	// -----------------------------------------------------------------------------

	/**
	 * Weapon families.
	 *
	 * A UMALWeaponDefinition names one of these, and a skill node names the one it needs, so the
	 * menu and the grant code can ask "is the right kind of weapon in hand" without holding a
	 * reference to an asset. Matching is hierarchical: MAL.Weapon.Sword covers a rapier tagged
	 * MAL.Weapon.Sword.Rapier.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Weapon")
	FGameplayTag Weapon_Sword;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Weapon")
	FGameplayTag Weapon_Axe;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Weapon")
	FGameplayTag Weapon_Dagger;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Weapon")
	FGameplayTag Weapon_Hammer;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Weapon")
	FGameplayTag Weapon_Scythe;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Weapon")
	FGameplayTag Weapon_Spear;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Weapon")
	FGameplayTag Weapon_Bow;

	// -----------------------------------------------------------------------------
	// Data Tags - Represent data/attribute types
	// -----------------------------------------------------------------------------
	
	/** Damage attribute */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Data")
	FGameplayTag Data_Damage;

	/** Damage bonus percentage carried by buff GEs through SetByCaller */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Data")
	FGameplayTag Data_DamageBonus;

	// -----------------------------------------------------------------------------
	// Damage Type Tags - Select the attack/defense channel an execution uses
	// -----------------------------------------------------------------------------

	/** Magic damage channel */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Damage")
	FGameplayTag Damage_Type_Magic;

	// -----------------------------------------------------------------------------
	// Ability Tags - Represent gameplay abilities
	// -----------------------------------------------------------------------------
	
	/** Attack ability */
	// UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Ability")
	// FGameplayTag Ability_Attack;

	/** Shared hit reaction ability, granted to players and enemies alike. */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Ability")
	FGameplayTag Ability_HitReact;

	// --- Ownership hierarchy (new) ---
	//
	// Abilities are grouped by who owns them so an AI can activate "Enemy.*" without ever
	// matching a player ability, and vice versa. Old flat tags above stay for assets that have
	// not been migrated yet; new abilities should use the children below.

	/** Parent of every player-owned ability tag. */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Ability|Player")
	FGameplayTag Ability_Player;

	/** Parent of every enemy-owned ability tag. */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Ability|Enemy")
	FGameplayTag Ability_Enemy;

	/** Parent of abilities shared by both sides (hit reaction, future universal actions). */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Ability|Common")
	FGameplayTag Ability_Common;

	/** Player melee attack (weapon-driven). */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Ability|Player")
	FGameplayTag Ability_Player_Attack;

	/** Player dodge/dash. */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Ability|Player")
	FGameplayTag Ability_Player_Dodge;

	/** Player self-buff. */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Ability|Player")
	FGameplayTag Ability_Player_Buff;

	/** Enemy melee attack (activated by the AI). */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Ability|Enemy")
	FGameplayTag Ability_Enemy_Attack;

	/** Hit reaction owned by every character. */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Ability|Common")
	FGameplayTag Ability_Common_HitReact;

	// /** Self-buff ability */
	// UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Ability")
	// FGameplayTag Ability_Buff;
	//
	// /** Dodge/dash ability. */
	// UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Ability")
	// FGameplayTag Ability_Dodge;

	// -----------------------------------------------------------------------------
	// Cooldown Tags - Granted by a cooldown GameplayEffect for its whole duration
	// -----------------------------------------------------------------------------

	/**
	 * Attack is on cooldown.
	 *
	 * The tag is what makes the cooldown observable: UGameplayAbility::GetCooldownTimeRemaining
	 * reads the ability's cooldown tags back off the owner, so a cooldown effect that grants no
	 * tags reports zero remaining time and the HUD would never see the sweep.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Cooldown")
	FGameplayTag Cooldown_Attack;

	/** Self-buff is on cooldown; same contract as Cooldown_Attack. */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Cooldown")
	FGameplayTag Cooldown_Buff;

	// --- Ownership hierarchy (new) ---

	/** Parent of player cooldown tags. */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Cooldown|Player")
	FGameplayTag Cooldown_Player;

	/** Parent of enemy cooldown tags. */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Cooldown|Enemy")
	FGameplayTag Cooldown_Enemy;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Cooldown|Player")
	FGameplayTag Cooldown_Player_Attack;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Cooldown|Player")
	FGameplayTag Cooldown_Player_Buff;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Cooldown|Player")
	FGameplayTag Cooldown_Player_Combo2;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Cooldown|Player")
	FGameplayTag Cooldown_Player_Whirlwind;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Cooldown|Enemy")
	FGameplayTag Cooldown_Enemy_Attack;

	// -----------------------------------------------------------------------------
	// Input Tags - Represent input actions
	// -----------------------------------------------------------------------------
	
	/** Move input */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Input")
	FGameplayTag Input_Movement_Move;

	/** Look input */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Input")
	FGameplayTag Input_Movement_Look;

	/** Jump input */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Input")
	FGameplayTag Input_Movement_Jump;

	/** Attack input */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Input")
	FGameplayTag Input_Attack_LMB;

	// --- Action-named input tags (new) ---

	/** Primary attack (LMB). */
	// UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Input")
	// FGameplayTag Input_PrimaryAttack;
	//
	// /** Secondary/heavy attack (RMB). */
	// UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Input")
	// FGameplayTag Input_SecondaryAttack;
	

	/** Dodge/dash. */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Input")
	FGameplayTag Input_Dodge;
	
	
	/** Lock-on toggle; not an ability input. */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Input")
	FGameplayTag Input_LockOn;

	/** Buff input */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Input")
	FGameplayTag Input_Attack_Buff;

	/** Opens or closes the attribute menu; not an ability input. */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Input")
	FGameplayTag Input_HUD_AttributeMenu;

	/** Opens or closes the skill tree menu; not an ability input. */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Input")
	FGameplayTag Input_HUD_SkillMenu;

	/** Swaps to the next weapon in the loadout; not an ability input. */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Input")
	FGameplayTag Input_WeaponNext;

	/** Swaps to the previous weapon in the loadout; not an ability input. */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Input")
	FGameplayTag Input_WeaponPrev;

	// -----------------------------------------------------------------------------
	// Gameplay Cue Tags - The GameplayCue root is required for the cue manager to route them
	// -----------------------------------------------------------------------------

	/** Looping aura shown while the damage buff is active */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|GameplayCue")
	FGameplayTag GameplayCue_Buff_DamageUp;

	/** Impact feedback fired where a melee hit lands (sound, particles, decals) */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|GameplayCue")
	FGameplayTag GameplayCue_Hit_Impact;

	/** Level-up feedback, played on every machine that sees the player level */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|GameplayCue")
	FGameplayTag GameplayCue_Character_LevelUp;

private:
	/** Singleton instance */
	static UMALGameplayTags* TagsSingleton;
};
