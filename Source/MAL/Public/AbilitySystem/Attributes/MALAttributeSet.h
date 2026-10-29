// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
// ATTRIBUTE_ACCESSORS_BASIC expands to inline getters that call through the owning ASC, which
// needs the complete type; AttributeSet.h only forward-declares it.
#include "AbilitySystemComponent.h"
#include "MALAttributeSet.generated.h"

/**
 * UMALAttributeSet
 *
 * Layered attribute model:
 *   - Primary    : innate character stats, modified directly by level-ups and equipment
 *   - Secondary  : combat and resource stats derived from the Primary ones
 *   - Standalone : current resource values (Health / Mana / Stamina), independent of the Max*
 *   - Meta       : transient within one GameplayEffect execution (Damage), never replicated
 *
 * Every Primary/Secondary/Standalone attribute uses ReplicatedUsing, so server-side changes reach
 * clients, and the client refreshes its aggregator through GAMEPLAYATTRIBUTE_REPNOTIFY to keep
 * CurrentValue correct.
 */
UCLASS()
class MAL_API UMALAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UMALAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	// ========== Primary Stats ==========

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Vigor, Category = "Primary")
	FGameplayAttributeData Vigor;
	ATTRIBUTE_ACCESSORS_BASIC(UMALAttributeSet, Vigor)

	UFUNCTION()
	virtual void OnRep_Vigor(const FGameplayAttributeData& OldVigor);

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Strength, Category = "Primary")
	FGameplayAttributeData Strength;
	ATTRIBUTE_ACCESSORS_BASIC(UMALAttributeSet, Strength)

	UFUNCTION()
	virtual void OnRep_Strength(const FGameplayAttributeData& OldStrength);

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Dexterity, Category = "Primary")
	FGameplayAttributeData Dexterity;
	ATTRIBUTE_ACCESSORS_BASIC(UMALAttributeSet, Dexterity)

	UFUNCTION()
	virtual void OnRep_Dexterity(const FGameplayAttributeData& OldDexterity);

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Endurance, Category = "Primary")
	FGameplayAttributeData Endurance;
	ATTRIBUTE_ACCESSORS_BASIC(UMALAttributeSet, Endurance)

	UFUNCTION()
	virtual void OnRep_Endurance(const FGameplayAttributeData& OldEndurance);

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Spirit, Category = "Primary")
	FGameplayAttributeData Spirit;
	ATTRIBUTE_ACCESSORS_BASIC(UMALAttributeSet, Spirit)

	UFUNCTION()
	virtual void OnRep_Spirit(const FGameplayAttributeData& OldSpirit);

	// ========== Secondary Stats ==========

	// Derived from Vigor: 100 + Vigor * 25
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Secondary")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS_BASIC(UMALAttributeSet, MaxHealth)

	UFUNCTION()
	virtual void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);

	// Derived from Vigor: 1 + Vigor * 0.5
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_HealthRegen, Category = "Secondary")
	FGameplayAttributeData HealthRegen;
	ATTRIBUTE_ACCESSORS_BASIC(UMALAttributeSet, HealthRegen)

	UFUNCTION()
	virtual void OnRep_HealthRegen(const FGameplayAttributeData& OldHealthRegen);

	// Derived from Endurance: 80 + Endurance * 15
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxStamina, Category = "Secondary")
	FGameplayAttributeData MaxStamina;
	ATTRIBUTE_ACCESSORS_BASIC(UMALAttributeSet, MaxStamina)

	UFUNCTION()
	virtual void OnRep_MaxStamina(const FGameplayAttributeData& OldMaxStamina);

	// Derived from Endurance: 5 + Endurance * 1.0
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_StaminaRegen, Category = "Secondary")
	FGameplayAttributeData StaminaRegen;
	ATTRIBUTE_ACCESSORS_BASIC(UMALAttributeSet, StaminaRegen)

	UFUNCTION()
	virtual void OnRep_StaminaRegen(const FGameplayAttributeData& OldStaminaRegen);

	// Derived from Spirit: 50 + Spirit * 20
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxMana, Category = "Secondary")
	FGameplayAttributeData MaxMana;
	ATTRIBUTE_ACCESSORS_BASIC(UMALAttributeSet, MaxMana)

	UFUNCTION()
	virtual void OnRep_MaxMana(const FGameplayAttributeData& OldMaxMana);

	// Derived from Spirit: 2 + Spirit * 0.8
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ManaRegen, Category = "Secondary")
	FGameplayAttributeData ManaRegen;
	ATTRIBUTE_ACCESSORS_BASIC(UMALAttributeSet, ManaRegen)

	UFUNCTION()
	virtual void OnRep_ManaRegen(const FGameplayAttributeData& OldManaRegen);

	// Derived from Strength: 10 + Strength * 3
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_PhysicalAttack, Category = "Secondary")
	FGameplayAttributeData PhysicalAttack;
	ATTRIBUTE_ACCESSORS_BASIC(UMALAttributeSet, PhysicalAttack)

	UFUNCTION()
	virtual void OnRep_PhysicalAttack(const FGameplayAttributeData& OldPhysicalAttack);

	// Derived from Spirit: 5 + Spirit * 2.5
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MagicAttack, Category = "Secondary")
	FGameplayAttributeData MagicAttack;
	ATTRIBUTE_ACCESSORS_BASIC(UMALAttributeSet, MagicAttack)

	UFUNCTION()
	virtual void OnRep_MagicAttack(const FGameplayAttributeData& OldMagicAttack);

	// Derived from Endurance: 5 + Endurance * 2
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_PhysicalDefense, Category = "Secondary")
	FGameplayAttributeData PhysicalDefense;
	ATTRIBUTE_ACCESSORS_BASIC(UMALAttributeSet, PhysicalDefense)

	UFUNCTION()
	virtual void OnRep_PhysicalDefense(const FGameplayAttributeData& OldPhysicalDefense);

	// Derived from Spirit: 5 + Spirit * 2
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MagicDefense, Category = "Secondary")
	FGameplayAttributeData MagicDefense;
	ATTRIBUTE_ACCESSORS_BASIC(UMALAttributeSet, MagicDefense)

	UFUNCTION()
	virtual void OnRep_MagicDefense(const FGameplayAttributeData& OldMagicDefense);

	// Derived from Dexterity: 5 + Dexterity * 0.3  (percent, 0~100)
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CritChance, Category = "Secondary")
	FGameplayAttributeData CritChance;
	ATTRIBUTE_ACCESSORS_BASIC(UMALAttributeSet, CritChance)

	UFUNCTION()
	virtual void OnRep_CritChance(const FGameplayAttributeData& OldCritChance);

	// Derived from Dexterity: 150 + Dexterity * 1.0  (percent, crit damage multiplier)
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CritDamage, Category = "Secondary")
	FGameplayAttributeData CritDamage;
	ATTRIBUTE_ACCESSORS_BASIC(UMALAttributeSet, CritDamage)

	UFUNCTION()
	virtual void OnRep_CritDamage(const FGameplayAttributeData& OldCritDamage);

	// Derived from Dexterity: 1.0 + Dexterity * 0.02  (attack speed multiplier)
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_AttackSpeed, Category = "Secondary")
	FGameplayAttributeData AttackSpeed;
	ATTRIBUTE_ACCESSORS_BASIC(UMALAttributeSet, AttackSpeed)

	UFUNCTION()
	virtual void OnRep_AttackSpeed(const FGameplayAttributeData& OldAttackSpeed);

	// Damage bonus in percent (0 = no bonus).
	// Kept out of the Primary -> Secondary derivation: only buff/debuff GEs touch it, and the
	// damage execution reads it in the final multiplier, so it can build up and expire over time.
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DamageBonus, Category = "Secondary")
	FGameplayAttributeData DamageBonus;
	ATTRIBUTE_ACCESSORS_BASIC(UMALAttributeSet, DamageBonus)

	UFUNCTION()
	virtual void OnRep_DamageBonus(const FGameplayAttributeData& OldDamageBonus);

	// ========== Standalone ==========

	// Current health, clamped to [0, MaxHealth] at runtime
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Standalone")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS_BASIC(UMALAttributeSet, Health)

	UFUNCTION()
	virtual void OnRep_Health(const FGameplayAttributeData& OldHealth);

	// Current mana, clamped to [0, MaxMana] at runtime
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Mana, Category = "Standalone")
	FGameplayAttributeData Mana;
	ATTRIBUTE_ACCESSORS_BASIC(UMALAttributeSet, Mana)

	UFUNCTION()
	virtual void OnRep_Mana(const FGameplayAttributeData& OldMana);

	// Current stamina, clamped to [0, MaxStamina] at runtime
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Stamina, Category = "Standalone")
	FGameplayAttributeData Stamina;
	ATTRIBUTE_ACCESSORS_BASIC(UMALAttributeSet, Stamina)

	UFUNCTION()
	virtual void OnRep_Stamina(const FGameplayAttributeData& OldStamina);

	// ========== Meta ==========

	/**
	 * Intermediate damage value: written by the damage GameplayEffect and reset by
	 * PostGameplayEffectExecute. Not replicated (it only exists on the server), so clients never
	 * see an unprocessed damage value.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Meta")
	FGameplayAttributeData Damage;
	ATTRIBUTE_ACCESSORS_BASIC(UMALAttributeSet, Damage)
};
