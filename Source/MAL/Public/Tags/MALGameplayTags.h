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

	/** Entity is stunned and cannot act */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Status")
	FGameplayTag Status_Stunned;

	/** Entity has an active shield */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Status")
	FGameplayTag Status_Shielded;

	/** Entity is attacking */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Status")
	FGameplayTag Status_Attacking;

	/** Entity is defending */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Status")
	FGameplayTag Status_Defending;

	/** Entity is dashing */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Status")
	FGameplayTag Status_Dashing;

	// -----------------------------------------------------------------------------
	// Event Tags - Represent events that can be triggered
	// -----------------------------------------------------------------------------
	
	/** Entity was hit/damaged */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Event")
	FGameplayTag Event_Hit;

	/** Entity died */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Event")
	FGameplayTag Event_Death;

	/** Entity's shield was broken */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Event")
	FGameplayTag Event_ShieldBreak;

	/** Weapon collision detected a hit */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Event")
	FGameplayTag Event_WeaponHit;

	// -----------------------------------------------------------------------------
	// Data Tags - Represent data/attribute types
	// -----------------------------------------------------------------------------
	
	/** Health attribute */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Data")
	FGameplayTag Data_Health;

	/** Shield attribute */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Data")
	FGameplayTag Data_Shield;

	/** Damage attribute */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Data")
	FGameplayTag Data_Damage;

	// -----------------------------------------------------------------------------
	// Damage Type Tags - Select the attack/defense channel an execution uses
	// -----------------------------------------------------------------------------

	/** Physical damage channel (default when no type tag is present) */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Damage")
	FGameplayTag Damage_Type_Physical;

	/** Magic damage channel */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Damage")
	FGameplayTag Damage_Type_Magic;

	// -----------------------------------------------------------------------------
	// Ability Tags - Represent gameplay abilities
	// -----------------------------------------------------------------------------
	
	/** Attack ability */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Ability")
	FGameplayTag Ability_Attack;

	/** Defend ability */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Ability")
	FGameplayTag Ability_Defend;

	/** Dash ability */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Ability")
	FGameplayTag Ability_Dash;

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
	FGameplayTag Input_Attack;

	/** Defend input */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Input")
	FGameplayTag Input_Defend;

	/** Dash input */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Input")
	FGameplayTag Input_Dash;

	/** Ability activated - used by ASC for input forwarding routing */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Input")
	FGameplayTag Input_AbilityActivated;

	// -----------------------------------------------------------------------------
	// Attribute Tags - Primary (base stats that drive secondary calculations)
	// -----------------------------------------------------------------------------

	/** Vigor - affects health and regen */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Attribute|Primary")
	FGameplayTag Attribute_Primary_Vigor;

	/** Strength - affects physical attack */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Attribute|Primary")
	FGameplayTag Attribute_Primary_Strength;

	/** Dexterity - affects crit and attack speed */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Attribute|Primary")
	FGameplayTag Attribute_Primary_Dexterity;

	/** Endurance - affects defense and stamina */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Attribute|Primary")
	FGameplayTag Attribute_Primary_Endurance;

	/** Spirit - affects mana and magic attack */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Attribute|Primary")
	FGameplayTag Attribute_Primary_Spirit;

	// -----------------------------------------------------------------------------
	// Attribute Tags - Secondary (derived from primary via formulas)
	// -----------------------------------------------------------------------------

	/** Max health, derived from Vigor */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Attribute|Secondary")
	FGameplayTag Attribute_Secondary_MaxHealth;

	/** Health regen per second, derived from Vigor */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Attribute|Secondary")
	FGameplayTag Attribute_Secondary_HealthRegen;

	/** Max stamina, derived from Endurance */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Attribute|Secondary")
	FGameplayTag Attribute_Secondary_MaxStamina;

	/** Stamina regen per second, derived from Endurance */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Attribute|Secondary")
	FGameplayTag Attribute_Secondary_StaminaRegen;

	/** Max mana, derived from Spirit */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Attribute|Secondary")
	FGameplayTag Attribute_Secondary_MaxMana;

	/** Mana regen per second, derived from Spirit */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Attribute|Secondary")
	FGameplayTag Attribute_Secondary_ManaRegen;

	/** Physical attack power, derived from Strength */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Attribute|Secondary")
	FGameplayTag Attribute_Secondary_PhysicalAttack;

	/** Magic attack power, derived from Spirit */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Attribute|Secondary")
	FGameplayTag Attribute_Secondary_MagicAttack;

	/** Physical defense/armor, derived from Endurance */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Attribute|Secondary")
	FGameplayTag Attribute_Secondary_PhysicalDefense;

	/** Magic defense/resist, derived from Spirit */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Attribute|Secondary")
	FGameplayTag Attribute_Secondary_MagicDefense;

	/** Critical hit chance %, derived from Dexterity */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Attribute|Secondary")
	FGameplayTag Attribute_Secondary_CritChance;

	/** Critical damage multiplier %, derived from Dexterity */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Attribute|Secondary")
	FGameplayTag Attribute_Secondary_CritDamage;

	/** Attack speed multiplier, derived from Dexterity */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Attribute|Secondary")
	FGameplayTag Attribute_Secondary_AttackSpeed;

	// -----------------------------------------------------------------------------
	// Attribute Tags - Standalone (independent resource pools)
	// -----------------------------------------------------------------------------

	/** Current health pool */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Attribute|Standalone")
	FGameplayTag Attribute_Standalone_Health;

	/** Current mana pool */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Attribute|Standalone")
	FGameplayTag Attribute_Standalone_Mana;

	/** Current stamina pool */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Tags|Attribute|Standalone")
	FGameplayTag Attribute_Standalone_Stamina;

private:
	/** Singleton instance */
	static UMALGameplayTags* TagsSingleton;
};
