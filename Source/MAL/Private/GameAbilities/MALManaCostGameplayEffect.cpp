// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameAbilities/MALManaCostGameplayEffect.h"

#include "AbilitySystem/Attributes/MALAttributeSet.h"
#include "GameplayEffect.h"

UMALManaCostGameplayEffect::UMALManaCostGameplayEffect()
{
	// Costs never tick: the mana is spent at the moment the ability commits.
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo ManaCost;
	ManaCost.Attribute = UMALAttributeSet::GetManaAttribute();
	ManaCost.ModifierOp = EGameplayModOp::Additive;
	ManaCost.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(-50.f));
	Modifiers.Add(ManaCost);
}
