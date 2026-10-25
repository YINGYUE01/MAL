// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "MALDamageExecutionCalculation.generated.h"

/**
 * UMALDamageExecutionCalculation
 *
 * Damage execution (physical/magic channels plus crits).
 *
 * Formula:
 *   Base        = SetByCaller(MAL.Data.Damage)          // base damage from the ability (level/combo bonus included)
 *   Attack      = physical: PhysicalAttack / magic: MagicAttack      (Source)
 *   Defense     = physical: PhysicalDefense / magic: MagicDefense    (Target)
 *   DamageBonus = damage bonus percentage (Source), modified by buff/debuff GEs
 *   Damage      = Base × (1 + Attack / AttackScalingDivisor)
 *                       × (DefenseScalingConstant / (DefenseScalingConstant + Defense))
 *                       × (1 + DamageBonus / 100)
 *   Crit        = Roll[0, 100) < CritChance → Damage × CritDamage / 100
 *
 * The channel is chosen by the MAL.Damage.Type.Magic tag on the GE spec (dynamic or asset tags
 * both work) and defaults to physical. The crit result is written back into
 * FMALGameplayEffectContext.
 *
 * The result is written to MALAttributeSet.Damage (a Meta attribute), and
 * PostGameplayEffectExecute folds it into Health.
 */
UCLASS(BlueprintType, Blueprintable)
class MAL_API UMALDamageExecutionCalculation : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	UMALDamageExecutionCalculation();

	virtual void Execute_Implementation(
		const FGameplayEffectCustomExecutionParameters& ExecutionParams,
		FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;

protected:
	/** Attack scaling base: every point of Attack adds 1 / AttackScalingDivisor as a percentage. */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|Damage", meta = (ClampMin = "1.0"))
	float AttackScalingDivisor = 100.f;

	/** Defense mitigation base K: mitigation factor = K / (K + Defense). */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|Damage", meta = (ClampMin = "1.0"))
	float DefenseScalingConstant = 100.f;

	/** When disabled, the crit roll is skipped (debugging or special abilities). */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|Damage")
	bool bAllowCriticalHits = true;

private:
	/** Whether this execution uses the magic channel (reads MAL.Damage.Type.Magic off the spec). */
	static bool IsMagicDamageSpec(const FGameplayEffectSpec& Spec);
};
