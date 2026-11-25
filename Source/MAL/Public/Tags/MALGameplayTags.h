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
	// Enemy Type Tags - What kind of enemy an AI pawn is
	// -----------------------------------------------------------------------------

	/** Root of enemy kind tags. */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Enemy")
	FGameplayTag Enemy_Type;

	/** Boss-grade enemy; the only kind with a top-screen bar today. */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Enemy")
	FGameplayTag Enemy_Type_Boss;

	/** Elite enemy; reserved for a future elite bar. */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Enemy")
	FGameplayTag Enemy_Type_Elite;

	/** Ordinary enemy; no top-screen bar. */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Enemy")
	FGameplayTag Enemy_Type_Normal;

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
	

	// --- Per-weapon attack abilities (new) ---
	//
	// Four combo steps per weapon family, e.g. MAL.Ability.Player.Sword.Attack1. The family sits
	// in the middle of the tag, so MAL.Ability.Player.Sword answers for every step of a sword
	// swing the same way MAL.Weapon.Sword answers for a rapier.
	//
	// Every step named here has a matching cooldown tag in the Cooldown block below; the step's
	// cooldown effect has to grant that tag, or GetCooldownTimeRemaining reports zero forever.

	// Sword combo steps.
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Ability|Player|Weapon")
	FGameplayTag Ability_Player_Sword_Attack1;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Ability|Player|Weapon")
	FGameplayTag Ability_Player_Sword_Attack2;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Ability|Player|Weapon")
	FGameplayTag Ability_Player_Sword_Attack3;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Ability|Player|Weapon")
	FGameplayTag Ability_Player_Sword_Attack4;

	// Axe combo steps.
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Ability|Player|Weapon")
	FGameplayTag Ability_Player_Axe_Attack1;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Ability|Player|Weapon")
	FGameplayTag Ability_Player_Axe_Attack2;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Ability|Player|Weapon")
	FGameplayTag Ability_Player_Axe_Attack3;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Ability|Player|Weapon")
	FGameplayTag Ability_Player_Axe_Attack4;

	// Dagger combo steps.
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Ability|Player|Weapon")
	FGameplayTag Ability_Player_Dagger_Attack1;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Ability|Player|Weapon")
	FGameplayTag Ability_Player_Dagger_Attack2;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Ability|Player|Weapon")
	FGameplayTag Ability_Player_Dagger_Attack3;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Ability|Player|Weapon")
	FGameplayTag Ability_Player_Dagger_Attack4;

	// Hammer combo steps.
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Ability|Player|Weapon")
	FGameplayTag Ability_Player_Hammer_Attack1;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Ability|Player|Weapon")
	FGameplayTag Ability_Player_Hammer_Attack2;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Ability|Player|Weapon")
	FGameplayTag Ability_Player_Hammer_Attack3;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Ability|Player|Weapon")
	FGameplayTag Ability_Player_Hammer_Attack4;

	// Scythe combo steps.
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Ability|Player|Weapon")
	FGameplayTag Ability_Player_Scythe_Attack1;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Ability|Player|Weapon")
	FGameplayTag Ability_Player_Scythe_Attack2;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Ability|Player|Weapon")
	FGameplayTag Ability_Player_Scythe_Attack3;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Ability|Player|Weapon")
	FGameplayTag Ability_Player_Scythe_Attack4;

	// Spear combo steps.
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Ability|Player|Weapon")
	FGameplayTag Ability_Player_Spear_Attack1;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Ability|Player|Weapon")
	FGameplayTag Ability_Player_Spear_Attack2;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Ability|Player|Weapon")
	FGameplayTag Ability_Player_Spear_Attack3;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Ability|Player|Weapon")
	FGameplayTag Ability_Player_Spear_Attack4;

	// Bow combo steps.
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Ability|Player|Weapon")
	FGameplayTag Ability_Player_Bow_Attack1;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Ability|Player|Weapon")
	FGameplayTag Ability_Player_Bow_Attack2;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Ability|Player|Weapon")
	FGameplayTag Ability_Player_Bow_Attack3;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Ability|Player|Weapon")
	FGameplayTag Ability_Player_Bow_Attack4;

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


	/** Parent of player cooldown tags. */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Cooldown|Player")
	FGameplayTag Cooldown_Player;

	/** Parent of enemy cooldown tags. */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Cooldown|Enemy")
	FGameplayTag Cooldown_Enemy;


	// --- Per-weapon attack cooldowns (new) ---
	//
	// Mirror of the per-weapon ability tags above: MAL.Cooldown.Player.Sword.Attack1 is the tag
	// the sword's first step's cooldown effect grants. Keep the two lists in step -- a step whose
	// cooldown effect grants no tag reports zero remaining time, and its HUD sweep never shows.

	// Sword combo step cooldowns.
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Cooldown|Player|Weapon")
	FGameplayTag Cooldown_Player_Sword_Attack1;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Cooldown|Player|Weapon")
	FGameplayTag Cooldown_Player_Sword_Attack2;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Cooldown|Player|Weapon")
	FGameplayTag Cooldown_Player_Sword_Attack3;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Cooldown|Player|Weapon")
	FGameplayTag Cooldown_Player_Sword_Attack4;

	// Axe combo step cooldowns.
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Cooldown|Player|Weapon")
	FGameplayTag Cooldown_Player_Axe_Attack1;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Cooldown|Player|Weapon")
	FGameplayTag Cooldown_Player_Axe_Attack2;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Cooldown|Player|Weapon")
	FGameplayTag Cooldown_Player_Axe_Attack3;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Cooldown|Player|Weapon")
	FGameplayTag Cooldown_Player_Axe_Attack4;

	// Dagger combo step cooldowns.
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Cooldown|Player|Weapon")
	FGameplayTag Cooldown_Player_Dagger_Attack1;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Cooldown|Player|Weapon")
	FGameplayTag Cooldown_Player_Dagger_Attack2;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Cooldown|Player|Weapon")
	FGameplayTag Cooldown_Player_Dagger_Attack3;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Cooldown|Player|Weapon")
	FGameplayTag Cooldown_Player_Dagger_Attack4;

	// Hammer combo step cooldowns.
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Cooldown|Player|Weapon")
	FGameplayTag Cooldown_Player_Hammer_Attack1;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Cooldown|Player|Weapon")
	FGameplayTag Cooldown_Player_Hammer_Attack2;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Cooldown|Player|Weapon")
	FGameplayTag Cooldown_Player_Hammer_Attack3;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Cooldown|Player|Weapon")
	FGameplayTag Cooldown_Player_Hammer_Attack4;

	// Scythe combo step cooldowns.
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Cooldown|Player|Weapon")
	FGameplayTag Cooldown_Player_Scythe_Attack1;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Cooldown|Player|Weapon")
	FGameplayTag Cooldown_Player_Scythe_Attack2;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Cooldown|Player|Weapon")
	FGameplayTag Cooldown_Player_Scythe_Attack3;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Cooldown|Player|Weapon")
	FGameplayTag Cooldown_Player_Scythe_Attack4;

	// Spear combo step cooldowns.
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Cooldown|Player|Weapon")
	FGameplayTag Cooldown_Player_Spear_Attack1;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Cooldown|Player|Weapon")
	FGameplayTag Cooldown_Player_Spear_Attack2;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Cooldown|Player|Weapon")
	FGameplayTag Cooldown_Player_Spear_Attack3;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Cooldown|Player|Weapon")
	FGameplayTag Cooldown_Player_Spear_Attack4;

	// Bow combo step cooldowns.
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Cooldown|Player|Weapon")
	FGameplayTag Cooldown_Player_Bow_Attack1;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Cooldown|Player|Weapon")
	FGameplayTag Cooldown_Player_Bow_Attack2;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Cooldown|Player|Weapon")
	FGameplayTag Cooldown_Player_Bow_Attack3;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Cooldown|Player|Weapon")
	FGameplayTag Cooldown_Player_Bow_Attack4;

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
	FGameplayTag Input_Attack_Q;

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
