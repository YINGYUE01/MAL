// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/Attributes/MALAttributeSet.h"
#include "GameplayEffectExtension.h"

UMALAttributeSet::UMALAttributeSet()
{
	// ========== Primary Stats ==========
	InitVigor(10.0f);
	InitStrength(10.0f);
	InitDexterity(10.0f);
	InitEndurance(10.0f);
	InitSpirit(10.0f);

	// ========== Secondary Stats ==========
	InitMaxHealth(350.0f);
	InitHealthRegen(6.0f);
	InitMaxStamina(230.0f);
	InitStaminaRegen(15.0f);
	InitMaxMana(250.0f);
	InitManaRegen(10.0f);
	InitPhysicalAttack(40.0f);
	InitMagicAttack(30.0f);
	InitPhysicalDefense(25.0f);
	InitMagicDefense(25.0f);
	InitCritChance(8.0f);
	InitCritDamage(150.0f);
	InitAttackSpeed(1.2f);

	// ========== Standalone ==========
	InitHealth(350.0f);
	InitMana(250.0f);
	InitStamina(230.0f);
}

void UMALAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	// ---------- Standalone: 当前资源值 Clamp ----------
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
	else if (Attribute == GetManaAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxMana());
	}
	else if (Attribute == GetStaminaAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxStamina());
	}
	// ---------- Secondary: 上限属性，最小 1 ----------
	else if (Attribute == GetMaxHealthAttribute()
		|| Attribute == GetMaxManaAttribute()
		|| Attribute == GetMaxStaminaAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.0f);
	}
	// ---------- Secondary: 回复速率，最小 0 ----------
	else if (Attribute == GetHealthRegenAttribute()
		|| Attribute == GetManaRegenAttribute()
		|| Attribute == GetStaminaRegenAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
	// ---------- Secondary: 暴击率 0~100 ----------
	else if (Attribute == GetCritChanceAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, 100.0f);
	}
	// ---------- Secondary: 攻击速度 0.1~5.0 ----------
	else if (Attribute == GetAttackSpeedAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.1f, 5.0f);
	}
}

void UMALAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
}
