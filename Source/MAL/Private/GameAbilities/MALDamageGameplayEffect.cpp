// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameAbilities/MALDamageGameplayEffect.h"
#include "AbilitySystem/Execution/MALDamageExecutionCalculation.h"
#include "GameplayEffect.h"

UMALDamageGameplayEffect::UMALDamageGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// Damage is no longer applied through a plain SetByCaller modifier: it goes through
	// the execution so attack/defense scaling and the crit roll are always applied.
	// Modifiers intentionally stays empty.
	FGameplayEffectExecutionDefinition DamageExecution;
	DamageExecution.CalculationClass = UMALDamageExecutionCalculation::StaticClass();
	Executions.Add(DamageExecution);
}
