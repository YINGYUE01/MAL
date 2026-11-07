// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "MALBossAttackSet.generated.h"

/**
 * One selectable Boss attack.
 *
 * The selector treats AbilityClass as the identity: two attacks may share an ability tag, but
 * a class is unique, so cooldowns and the recent-attack list cannot collide.
 */
USTRUCT(BlueprintType)
struct FMALBossAttackEntry
{
	GENERATED_BODY()

	/** Ability granted to the Boss and activated when this entry is selected. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Boss|Attack")
	TSubclassOf<UGameplayAbility> AbilityClass;

	/** Name used in logs and debug output. Falls back to the class name when empty. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Boss|Attack")
	FName DebugName;

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

	/** Cooldown in seconds, measured from the end of the previous use. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Boss|Attack")
	float Cooldown = 6.0f;

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

	FString GetDebugName() const
	{
		return DebugName.IsNone() && AbilityClass ? AbilityClass->GetName() : DebugName.ToString();
	}
};

/**
 * Data asset holding the Boss move set and its phase thresholds.
 *
 * Selection state (cooldowns, recent picks, global cadence) deliberately lives on the selector
 * component, not here: this asset is shared configuration, while each Boss instance needs its
 * own timers.
 */
UCLASS(BlueprintType)
class MAL_API UMALBossAttackSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Boss")
	TArray<FMALBossAttackEntry> Attacks;

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
