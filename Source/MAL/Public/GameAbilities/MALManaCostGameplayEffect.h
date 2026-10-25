// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "MALManaCostGameplayEffect.generated.h"

/**
 * UMALManaCostGameplayEffect
 *
 * Ability cost GE: instant, adds a negative amount to MALAttributeSet.Mana.
 *
 * It has to use a fixed additive negative value rather than SetByCaller:
 * UAbilitySystemComponent::CanApplyAttributeModifiers only runs its
 * "CurrentValue + Magnitude < 0" check for additive modifiers, and a SetByCaller magnitude is
 * still 0 during CheckCost, so the cast could not be rejected for low mana. A blueprint subclass
 * only has to change Magnitude to tune the cost.
 */
UCLASS()
class MAL_API UMALManaCostGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UMALManaCostGameplayEffect();
};
