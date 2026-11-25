// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Engine/DataAsset.h"
#include "MALBossSkillKit.generated.h"

/**
 * One entry of the Boss skill kit: the ability plus everything the selector needs to score it.
 *
 * This is the single place a Boss skill is authored. The same list feeds both the grant pass
 * (every entry's AbilityClass is granted on the server) and the selection pass (entries with
 * bSelectable are scored), so an ability can no longer be "granted but never selected" or
 * "selected but never granted".
 */
USTRUCT(BlueprintType)
struct FMALBossSkillEntry
{
	GENERATED_BODY()

	/** Ability granted to the Boss and activated when this entry is selected. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Boss|Skill")
	TSubclassOf<UGameplayAbility> AbilityClass;

	/**
	 * Grant-only entry: the ability is granted but the selector never picks it.
	 *
	 * Use it for abilities triggered from Blueprint or a scripted beat (openers, utilities);
	 * everything the Boss picks on its own must stay selectable.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Boss|Skill")
	bool bSelectable = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Boss|Attack")
	float MinRange = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Boss|Attack")
	float MaxRange = 350.0f;

	/** Range the attack is tuned around, used by the distance score. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Boss|Attack")
	float PreferredRange = 200.0f;

	/** How far from PreferredRange the distance score reaches zero. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Boss|Attack")
	float RangeTolerance = 120.0f;

	/**
	 * Weight per Boss phase. Index 0 is phase 1.
	 * A missing or zero entry means the attack is unavailable in that phase.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Boss|Attack")
	TArray<float> PhaseWeights;

	/** Require a clear AI line of sight before this attack can be selected. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Boss|Attack")
	bool bRequiresLineOfSight = false;

	/** Radial attacks ignore facing and always receive a full angle score. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Boss|Attack")
	bool bRadial = false;

	/** Multipliers for the target's current state. Only weight the score, never force a pick. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Boss|Attack")
	float CastingMultiplier = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Boss|Attack")
	float AttackingMultiplier = 1.35f;

	/** Bonus when the target is behind the Boss (useful for sweeps). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Boss|Attack")
	float BehindMultiplier = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Boss|Attack")
	float LowHealthMultiplier = 1.6f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Boss|Attack")
	float LowHealthThreshold = 0.3f;

	/** Hard cap on consecutive uses of this entry. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Boss|Attack")
	int32 MaxConsecutive = 2;

	/** Weight this entry has in Phase, or zero when the entry is not available there. */
	float GetPhaseWeight(int32 Phase) const
	{
		return PhaseWeights.IsValidIndex(Phase) ? FMath::Max(PhaseWeights[Phase], 0.0f) : 0.0f;
	}

	/** GA class name, used in logs and on the score table. No separate display name to keep in sync. */
	FString GetDebugName() const
	{
		return AbilityClass ? AbilityClass->GetName() : FString(TEXT("<none>"));
	}
};

/**
 * The Boss skill kit: granting and selection share this one asset.
 *
 * The character grants every entry's AbilityClass on the server; the selector scores the
 * entries marked selectable, phase weights included. Selection state (recent picks, cadence)
 * deliberately lives on the selector component, while cooldowns live where GAS owns them: on
 * each ability's CooldownGameplayEffectClass. The kit therefore carries no Cooldown field -
 * the selector skips entries whose ability is still cooling down (GetCooldownTimeRemaining),
 * so there is exactly one cooldown source.
 *
 * Boss bar presentation (display name, activation radius) is NOT here: it lives on the enemy
 * character defaults, because it describes the actor rather than its move set.
 */
UCLASS(BlueprintType)
class MAL_API UMALBossSkillKit : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Every Boss skill. Selectable entries are scored; grant-only entries are only granted. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Boss")
	TArray<FMALBossSkillEntry> Skills;

	/** Health fraction below which phase 2 begins. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Boss|Phase", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Phase2Threshold = 0.66f;

	/** Health fraction below which phase 3 begins. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Boss|Phase", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Phase3Threshold = 0.33f;

	/** Duration of the phase-transition beat after the current attack has finished. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Boss|Phase", meta = (ClampMin = "0.0"))
	float PhaseTransitionDuration = 1.0f;

	/** Extra quiet time after the transition before the next attack may start. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Boss|Phase", meta = (ClampMin = "0.0"))
	float PhaseTransitionGrace = 0.8f;

	/** 0, 1 or 2 for the supplied health fraction. */
	UFUNCTION(BlueprintPure, Category = "MAL|Boss|Phase")
	int32 GetPhaseForHealthPercent(float HealthPercent) const;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};