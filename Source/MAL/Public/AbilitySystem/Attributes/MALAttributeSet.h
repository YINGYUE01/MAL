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
 *   - Secondary  : 由 Primary 派生的战斗 / 资源属性（当前仅声明，公式在后续任务中通过
 *                  GameplayEffect 或 MetaAttribute 实现）
 *   - Standalone : 运行时的资源当前值（Health / Mana / Stamina），独立于 Max* 属性
 *
 * 本文件仅负责属性声明与初值 / 数值 Clamp，不包含派生重算逻辑。
 */
UCLASS()
class MAL_API UMALAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UMALAttributeSet();

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	// ========== Primary Stats ==========

	UPROPERTY(BlueprintReadOnly, Category = "Primary")
	FGameplayAttributeData Vigor;
	ATTRIBUTE_ACCESSORS(UMALAttributeSet, Vigor)

	UPROPERTY(BlueprintReadOnly, Category = "Primary")
	FGameplayAttributeData Strength;
	ATTRIBUTE_ACCESSORS(UMALAttributeSet, Strength)

	UPROPERTY(BlueprintReadOnly, Category = "Primary")
	FGameplayAttributeData Dexterity;
	ATTRIBUTE_ACCESSORS(UMALAttributeSet, Dexterity)

	UPROPERTY(BlueprintReadOnly, Category = "Primary")
	FGameplayAttributeData Endurance;
	ATTRIBUTE_ACCESSORS(UMALAttributeSet, Endurance)

	UPROPERTY(BlueprintReadOnly, Category = "Primary")
	FGameplayAttributeData Spirit;
	ATTRIBUTE_ACCESSORS(UMALAttributeSet, Spirit)

	// ========== Secondary Stats ==========

	// Derived from Vigor: 100 + Vigor * 25
	UPROPERTY(BlueprintReadOnly, Category = "Secondary")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UMALAttributeSet, MaxHealth)

	// Derived from Vigor: 1 + Vigor * 0.5
	UPROPERTY(BlueprintReadOnly, Category = "Secondary")
	FGameplayAttributeData HealthRegen;
	ATTRIBUTE_ACCESSORS(UMALAttributeSet, HealthRegen)

	// Derived from Endurance: 80 + Endurance * 15
	UPROPERTY(BlueprintReadOnly, Category = "Secondary")
	FGameplayAttributeData MaxStamina;
	ATTRIBUTE_ACCESSORS(UMALAttributeSet, MaxStamina)

	// Derived from Endurance: 5 + Endurance * 1.0
	UPROPERTY(BlueprintReadOnly, Category = "Secondary")
	FGameplayAttributeData StaminaRegen;
	ATTRIBUTE_ACCESSORS(UMALAttributeSet, StaminaRegen)

	// Derived from Spirit: 50 + Spirit * 20
	UPROPERTY(BlueprintReadOnly, Category = "Secondary")
	FGameplayAttributeData MaxMana;
	ATTRIBUTE_ACCESSORS(UMALAttributeSet, MaxMana)

	// Derived from Spirit: 2 + Spirit * 0.8
	UPROPERTY(BlueprintReadOnly, Category = "Secondary")
	FGameplayAttributeData ManaRegen;
	ATTRIBUTE_ACCESSORS(UMALAttributeSet, ManaRegen)

	// Derived from Strength: 10 + Strength * 3
	UPROPERTY(BlueprintReadOnly, Category = "Secondary")
	FGameplayAttributeData PhysicalAttack;
	ATTRIBUTE_ACCESSORS(UMALAttributeSet, PhysicalAttack)

	// Derived from Spirit: 5 + Spirit * 2.5
	UPROPERTY(BlueprintReadOnly, Category = "Secondary")
	FGameplayAttributeData MagicAttack;
	ATTRIBUTE_ACCESSORS(UMALAttributeSet, MagicAttack)

	// Derived from Endurance: 5 + Endurance * 2
	UPROPERTY(BlueprintReadOnly, Category = "Secondary")
	FGameplayAttributeData PhysicalDefense;
	ATTRIBUTE_ACCESSORS(UMALAttributeSet, PhysicalDefense)

	// Derived from Spirit: 5 + Spirit * 2
	UPROPERTY(BlueprintReadOnly, Category = "Secondary")
	FGameplayAttributeData MagicDefense;
	ATTRIBUTE_ACCESSORS(UMALAttributeSet, MagicDefense)

	// Derived from Dexterity: 5 + Dexterity * 0.3  (percent, 0~100)
	UPROPERTY(BlueprintReadOnly, Category = "Secondary")
	FGameplayAttributeData CritChance;
	ATTRIBUTE_ACCESSORS(UMALAttributeSet, CritChance)

	// Derived from Dexterity: 150 + Dexterity * 1.0  (percent, 暴击伤害倍率)
	UPROPERTY(BlueprintReadOnly, Category = "Secondary")
	FGameplayAttributeData CritDamage;
	ATTRIBUTE_ACCESSORS(UMALAttributeSet, CritDamage)

	// Derived from Dexterity: 1.0 + Dexterity * 0.02  (攻击速度倍率)
	UPROPERTY(BlueprintReadOnly, Category = "Secondary")
	FGameplayAttributeData AttackSpeed;
	ATTRIBUTE_ACCESSORS(UMALAttributeSet, AttackSpeed)

	// ========== Standalone ==========

	// 当前生命值，运行时 Clamp 到 [0, MaxHealth]
	UPROPERTY(BlueprintReadOnly, Category = "Standalone")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UMALAttributeSet, Health)

	// 当前法力值，运行时 Clamp 到 [0, MaxMana]
	UPROPERTY(BlueprintReadOnly, Category = "Standalone")
	FGameplayAttributeData Mana;
	ATTRIBUTE_ACCESSORS(UMALAttributeSet, Mana)

	// 当前耐力值，运行时 Clamp 到 [0, MaxStamina]
	UPROPERTY(BlueprintReadOnly, Category = "Standalone")
	FGameplayAttributeData Stamina;
	ATTRIBUTE_ACCESSORS(UMALAttributeSet, Stamina)
};
