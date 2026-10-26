// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/Attributes/MALPrimaryStat.h"

#include "AbilitySystem/Attributes/MALAttributeSet.h"

FGameplayAttribute MALGetPrimaryAttribute(EMALPrimaryStat Stat)
{
	switch (Stat)
	{
	case EMALPrimaryStat::Vigor:     return UMALAttributeSet::GetVigorAttribute();
	case EMALPrimaryStat::Strength:  return UMALAttributeSet::GetStrengthAttribute();
	case EMALPrimaryStat::Dexterity: return UMALAttributeSet::GetDexterityAttribute();
	case EMALPrimaryStat::Endurance: return UMALAttributeSet::GetEnduranceAttribute();
	case EMALPrimaryStat::Spirit:    return UMALAttributeSet::GetSpiritAttribute();
	default:                         return FGameplayAttribute();
	}
}
