// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/Attributes/MALSecondaryStat.h"

#include "AbilitySystem/Attributes/MALAttributeSet.h"

FGameplayAttribute MALGetSecondaryAttribute(EMALSecondaryStat Stat)
{
	switch (Stat)
	{
	case EMALSecondaryStat::MaxHealth:       return UMALAttributeSet::GetMaxHealthAttribute();
	case EMALSecondaryStat::HealthRegen:     return UMALAttributeSet::GetHealthRegenAttribute();
	case EMALSecondaryStat::MaxStamina:      return UMALAttributeSet::GetMaxStaminaAttribute();
	case EMALSecondaryStat::StaminaRegen:    return UMALAttributeSet::GetStaminaRegenAttribute();
	case EMALSecondaryStat::MaxMana:         return UMALAttributeSet::GetMaxManaAttribute();
	case EMALSecondaryStat::ManaRegen:       return UMALAttributeSet::GetManaRegenAttribute();
	case EMALSecondaryStat::PhysicalAttack:  return UMALAttributeSet::GetPhysicalAttackAttribute();
	case EMALSecondaryStat::MagicAttack:     return UMALAttributeSet::GetMagicAttackAttribute();
	case EMALSecondaryStat::PhysicalDefense: return UMALAttributeSet::GetPhysicalDefenseAttribute();
	case EMALSecondaryStat::MagicDefense:    return UMALAttributeSet::GetMagicDefenseAttribute();
	case EMALSecondaryStat::CritChance:      return UMALAttributeSet::GetCritChanceAttribute();
	case EMALSecondaryStat::CritDamage:      return UMALAttributeSet::GetCritDamageAttribute();
	case EMALSecondaryStat::AttackSpeed:     return UMALAttributeSet::GetAttackSpeedAttribute();
	case EMALSecondaryStat::DamageBonus:     return UMALAttributeSet::GetDamageBonusAttribute();
	default:                                  return FGameplayAttribute();
	}
}

EMALStatFormat MALGetSecondaryStatFormat(EMALSecondaryStat Stat)
{
	switch (Stat)
	{
	case EMALSecondaryStat::CritChance:
		// Stored on a 0~100 scale with a fractional part, so the tenth is worth showing.
		return EMALStatFormat::Percent;

	case EMALSecondaryStat::CritDamage:
	case EMALSecondaryStat::DamageBonus:
		return EMALStatFormat::PercentWhole;

	case EMALSecondaryStat::AttackSpeed:
		return EMALStatFormat::Multiplier;

	default:
		return EMALStatFormat::Integer;
	}
}

FText MALFormatStatValue(float Value, EMALStatFormat Format)
{
	FNumberFormattingOptions Options;
	Options.MinimumFractionalDigits = 0;
	Options.MaximumFractionalDigits = 0;

	switch (Format)
	{
	case EMALStatFormat::Percent:
		Options.MinimumFractionalDigits = 1;
		Options.MaximumFractionalDigits = 1;
		break;

	case EMALStatFormat::Multiplier:
		Options.MinimumFractionalDigits = 2;
		Options.MaximumFractionalDigits = 2;
		break;

	default:
		break;
	}

	// AsNumber rather than a fixed-point string: a culture that groups thousands should group a
	// stat here too.
	const FText Number = FText::AsNumber(Value, &Options);

	if (Format == EMALStatFormat::Integer || Format == EMALStatFormat::Multiplier)
	{
		return Number;
	}

	return FText::Format(NSLOCTEXT("MALAttribute", "StatPercentValue", "{0}%"), Number);
}
