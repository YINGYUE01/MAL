// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "MALAttributeSet.generated.h"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName,PropertyName)\
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName)\
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName)\
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * UMALAttributeSet
 *
 * 分级属性体系：
 *   - Primary    : 玩家基础属性（可通过升级 / 装备直接修改）
 *   - Secondary  : 由 Primary 派生的战斗 / 资源属性
 *   - Standalone : 运行时的资源当前值（Health / Mana / Stamina），独立于 Max* 属性
 *   - Meta       : 只在一次 GameplayEffect 执行内存在的中间值（Damage），不复制
 *
 * 所有 Primary/Secondary/Standalone 属性都带 ReplicatedUsing，服务器上的修改会同步到客户端；
 * 客户端收到后通过 GAMEPLAYATTRIBUTE_REPNOTIFY 更新 Aggregator，保证 CurrentValue 正确。
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
	ATTRIBUTE_ACCESSORS(UMALAttributeSet, Vigor)

	UFUNCTION()
	virtual void OnRep_Vigor(const FGameplayAttributeData& OldVigor);

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Strength, Category = "Primary")
	FGameplayAttributeData Strength;
	ATTRIBUTE_ACCESSORS(UMALAttributeSet, Strength)

	UFUNCTION()
	virtual void OnRep_Strength(const FGameplayAttributeData& OldStrength);

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Dexterity, Category = "Primary")
	FGameplayAttributeData Dexterity;
	ATTRIBUTE_ACCESSORS(UMALAttributeSet, Dexterity)

	UFUNCTION()
	virtual void OnRep_Dexterity(const FGameplayAttributeData& OldDexterity);

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Endurance, Category = "Primary")
	FGameplayAttributeData Endurance;
	ATTRIBUTE_ACCESSORS(UMALAttributeSet, Endurance)

	UFUNCTION()
	virtual void OnRep_Endurance(const FGameplayAttributeData& OldEndurance);

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Spirit, Category = "Primary")
	FGameplayAttributeData Spirit;
	ATTRIBUTE_ACCESSORS(UMALAttributeSet, Spirit)

	UFUNCTION()
	virtual void OnRep_Spirit(const FGameplayAttributeData& OldSpirit);

	// ========== Secondary Stats ==========

	// Derived from Vigor: 100 + Vigor * 25
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Secondary")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UMALAttributeSet, MaxHealth)

	UFUNCTION()
	virtual void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);

	// Derived from Vigor: 1 + Vigor * 0.5
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_HealthRegen, Category = "Secondary")
	FGameplayAttributeData HealthRegen;
	ATTRIBUTE_ACCESSORS(UMALAttributeSet, HealthRegen)

	UFUNCTION()
	virtual void OnRep_HealthRegen(const FGameplayAttributeData& OldHealthRegen);

	// Derived from Endurance: 80 + Endurance * 15
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxStamina, Category = "Secondary")
	FGameplayAttributeData MaxStamina;
	ATTRIBUTE_ACCESSORS(UMALAttributeSet, MaxStamina)

	UFUNCTION()
	virtual void OnRep_MaxStamina(const FGameplayAttributeData& OldMaxStamina);

	// Derived from Endurance: 5 + Endurance * 1.0
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_StaminaRegen, Category = "Secondary")
	FGameplayAttributeData StaminaRegen;
	ATTRIBUTE_ACCESSORS(UMALAttributeSet, StaminaRegen)

	UFUNCTION()
	virtual void OnRep_StaminaRegen(const FGameplayAttributeData& OldStaminaRegen);

	// Derived from Spirit: 50 + Spirit * 20
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxMana, Category = "Secondary")
	FGameplayAttributeData MaxMana;
	ATTRIBUTE_ACCESSORS(UMALAttributeSet, MaxMana)

	UFUNCTION()
	virtual void OnRep_MaxMana(const FGameplayAttributeData& OldMaxMana);

	// Derived from Spirit: 2 + Spirit * 0.8
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ManaRegen, Category = "Secondary")
	FGameplayAttributeData ManaRegen;
	ATTRIBUTE_ACCESSORS(UMALAttributeSet, ManaRegen)

	UFUNCTION()
	virtual void OnRep_ManaRegen(const FGameplayAttributeData& OldManaRegen);

	// Derived from Strength: 10 + Strength * 3
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_PhysicalAttack, Category = "Secondary")
	FGameplayAttributeData PhysicalAttack;
	ATTRIBUTE_ACCESSORS(UMALAttributeSet, PhysicalAttack)

	UFUNCTION()
	virtual void OnRep_PhysicalAttack(const FGameplayAttributeData& OldPhysicalAttack);

	// Derived from Spirit: 5 + Spirit * 2.5
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MagicAttack, Category = "Secondary")
	FGameplayAttributeData MagicAttack;
	ATTRIBUTE_ACCESSORS(UMALAttributeSet, MagicAttack)

	UFUNCTION()
	virtual void OnRep_MagicAttack(const FGameplayAttributeData& OldMagicAttack);

	// Derived from Endurance: 5 + Endurance * 2
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_PhysicalDefense, Category = "Secondary")
	FGameplayAttributeData PhysicalDefense;
	ATTRIBUTE_ACCESSORS(UMALAttributeSet, PhysicalDefense)

	UFUNCTION()
	virtual void OnRep_PhysicalDefense(const FGameplayAttributeData& OldPhysicalDefense);

	// Derived from Spirit: 5 + Spirit * 2
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MagicDefense, Category = "Secondary")
	FGameplayAttributeData MagicDefense;
	ATTRIBUTE_ACCESSORS(UMALAttributeSet, MagicDefense)

	UFUNCTION()
	virtual void OnRep_MagicDefense(const FGameplayAttributeData& OldMagicDefense);

	// Derived from Dexterity: 5 + Dexterity * 0.3  (percent, 0~100)
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CritChance, Category = "Secondary")
	FGameplayAttributeData CritChance;
	ATTRIBUTE_ACCESSORS(UMALAttributeSet, CritChance)

	UFUNCTION()
	virtual void OnRep_CritChance(const FGameplayAttributeData& OldCritChance);

	// Derived from Dexterity: 150 + Dexterity * 1.0  (percent, 暴击伤害倍率)
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CritDamage, Category = "Secondary")
	FGameplayAttributeData CritDamage;
	ATTRIBUTE_ACCESSORS(UMALAttributeSet, CritDamage)

	UFUNCTION()
	virtual void OnRep_CritDamage(const FGameplayAttributeData& OldCritDamage);

	// Derived from Dexterity: 1.0 + Dexterity * 0.02  (攻击速度倍率)
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_AttackSpeed, Category = "Secondary")
	FGameplayAttributeData AttackSpeed;
	ATTRIBUTE_ACCESSORS(UMALAttributeSet, AttackSpeed)

	UFUNCTION()
	virtual void OnRep_AttackSpeed(const FGameplayAttributeData& OldAttackSpeed);

	// ========== Standalone ==========

	// 当前生命值，运行时 Clamp 到 [0, MaxHealth]
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Standalone")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UMALAttributeSet, Health)

	UFUNCTION()
	virtual void OnRep_Health(const FGameplayAttributeData& OldHealth);

	// 当前法力值，运行时 Clamp 到 [0, MaxMana]
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Mana, Category = "Standalone")
	FGameplayAttributeData Mana;
	ATTRIBUTE_ACCESSORS(UMALAttributeSet, Mana)

	UFUNCTION()
	virtual void OnRep_Mana(const FGameplayAttributeData& OldMana);

	// 当前耐力值，运行时 Clamp 到 [0, MaxStamina]
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Stamina, Category = "Standalone")
	FGameplayAttributeData Stamina;
	ATTRIBUTE_ACCESSORS(UMALAttributeSet, Stamina)

	UFUNCTION()
	virtual void OnRep_Stamina(const FGameplayAttributeData& OldStamina);

	// ========== Meta ==========

	/**
	 * 伤害中间值：由 Damage GameplayEffect 写入，PostGameplayEffectExecute 消费后清零。
	 * 不复制（只在服务器执行），避免客户端看到未处理的伤害值。
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Meta")
	FGameplayAttributeData Damage;
	ATTRIBUTE_ACCESSORS(UMALAttributeSet, Damage)
};
