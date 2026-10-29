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

	// -----------------------------------------------------------------------------
	// Event Tags - Represent events that can be triggered
	// -----------------------------------------------------------------------------
	
	/** Entity was hit/damaged */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Event")
	FGameplayTag Event_Hit;

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
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Ability")
	FGameplayTag Ability_Attack;

	/** Self-buff ability */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Ability")
	FGameplayTag Ability_Buff;

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

	// -----------------------------------------------------------------------------
	// Input Tags - Represent input actions
	// -----------------------------------------------------------------------------
	
	/** Move input */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Input")
	FGameplayTag Input_Move;

	/** Look input */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Input")
	FGameplayTag Input_Look;

	/** Jump input */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Input")
	FGameplayTag Input_Jump;

	/** Attack input */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Input")
	FGameplayTag Input_LMB;

	/** Defend input */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Input")
	FGameplayTag Input_Defend;

	/** Dash input */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Input")
	FGameplayTag Input_Dash;

	/** Buff input */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Input")
	FGameplayTag Input_Buff;

	/** Opens or closes the attribute menu; not an ability input. */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Input")
	FGameplayTag Input_AttributeMenu;

	/** Opens or closes the skill tree menu; not an ability input. */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Input")
	FGameplayTag Input_SkillMenu;

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
