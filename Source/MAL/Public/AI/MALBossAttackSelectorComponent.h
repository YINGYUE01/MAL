// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/MALBossSkillKit.h"
#include "MALBossAttackSelectorComponent.generated.h"

class AActor;
class AMALCharacterBase;
class UAbilitySystemComponent;
class UGameplayAbility;

/**
 * Server-side Boss attack selector.
 *
 * Owns all per-instance decision state: cooldowns, recent picks, global cadence, phase
 * transition and the anti-frustration counters. The BT layer only asks "select" and then
 * "activate the selection", so the scoring rules stay testable without a behavior tree.
 */
UCLASS(ClassGroup = (MAL), meta = (BlueprintSpawnableComponent))
class MAL_API UMALBossAttackSelectorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMALBossAttackSelectorComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Seconds between the end of one attack and the start of the next, per phase. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Boss|Rhythm")
	TArray<float> PhaseAttackIntervals;

	/** Maximum attacks started inside BudgetWindow, per phase. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Boss|Rhythm")
	TArray<int32> PhaseAttackBudgets;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Boss|Rhythm", meta = (ClampMin = "1.0"))
	float BudgetWindow = 6.0f;

	/** Player hit streak that triggers a longer post-attack recovery. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Boss|Rhythm", meta = (ClampMin = "1"))
	int32 HitStreakRecoveryThreshold = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Boss|Rhythm", meta = (ClampMin = "0.0"))
	float HitStreakRecovery = 0.5f;

	/** Consecutive player dodges that add a small delay before the next attack. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Boss|Rhythm", meta = (ClampMin = "1"))
	int32 EvadeStreakDelayThreshold = 2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Boss|Rhythm", meta = (ClampMin = "0.0"))
	float EvadeStreakDelay = 0.25f;

	/**
	 * Number of highest-scoring attacks the final pick is drawn from.
	 * Inside this window the pick is weighted by score, so the strongest option is favoured while
	 * weaker ones keep a real chance; close scores still produce visible variation.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Boss|Selection", meta = (ClampMin = "1", ClampMax = "4"))
	int32 TopCandidateCount = 3;

	/**
	 * Debug: draw one fixed top-left row per kit entry (name + current score).
	 *
	 * Rows use stable debug-message keys, so a row keeps its position and only its number
	 * changes. Scores are shown without the per-pick jitter, otherwise the display would
	 * flicker every tick.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Boss|Debug")
	bool bDrawSkillScores = true;

	/**
	 * Pick an attack for Target. Pure: it does not start the ability nor mark it used.
	 * ActivateLastSelectedAttack does that on a successful activation; cooldowns belong to GAS
	 * (each ability's CooldownGameplayEffectClass), never to this component.
	 */
	UFUNCTION(BlueprintCallable, Category = "MAL|Boss|Selection")
	bool SelectAttack(AActor* Target, FMALBossSkillEntry& OutAttack, FString& OutDebug);

	/** Activate the entry produced by the last successful SelectAttack. Server only. */
	UFUNCTION(BlueprintCallable, Category = "MAL|Boss|Selection")
	bool ActivateLastSelectedAttack();

	/** Called when the selected ability has ended; starts the cadence beat for the next pick. */
	UFUNCTION(BlueprintCallable, Category = "MAL|Boss|Selection")
	void NotifyAttackEnded();

	UFUNCTION(BlueprintPure, Category = "MAL|Boss|Selection")
	bool IsAttackInProgress() const { return bAttackInProgress; }

	UFUNCTION(BlueprintPure, Category = "MAL|Boss|Selection")
	bool IsPhaseTransitioning() const;

	UFUNCTION(BlueprintPure, Category = "MAL|Boss|Selection")
	int32 GetCurrentPhase() const { return CurrentPhase; }

	UFUNCTION(BlueprintPure, Category = "MAL|Boss|Selection")
	float GetNextAttackAllowedTime() const;

	UFUNCTION(BlueprintPure, Category = "MAL|Boss|Selection")
	FString GetLastSelectionDebug() const { return LastSelectionDebug; }

	/** Hooks for the combat layer: call when the Boss lands a hit / the player dodges it. */
	UFUNCTION(BlueprintCallable, Category = "MAL|Boss|Rhythm")
	void NotifyPlayerHit();

	UFUNCTION(BlueprintCallable, Category = "MAL|Boss|Rhythm")
	void NotifyPlayerEvaded();

private:
	/** Last few classes, newest at the end. */
	TArray<TSubclassOf<UGameplayAbility>> RecentAttacks;

	/** Start times of attacks inside the rolling budget window. */
	TArray<float> AttackTimes;

	FMALBossSkillEntry LastSelectedAttack;
	bool bHasSelection = false;
	bool bAttackInProgress = false;

	TSubclassOf<UGameplayAbility> ActiveAttackClass;

	float NextAttackAllowedTime = 0.0f;
	float PhaseTransitionEndTime = 0.0f;
	int32 CurrentPhase = 0;
	int32 PendingPhase = 0;
	bool bHasPendingPhase = false;

	int32 PlayerHitStreak = 0;
	int32 PlayerEvadeStreak = 0;

	FString LastSelectionDebug;

	AMALCharacterBase* GetOwnerCharacter() const;

	/**
	 * The owner's skill kit, read straight from the character.
	 *
	 * Deliberately not a component property: granting (character) and scoring (this component)
	 * must read the same list, and a second assignable copy is how the two used to drift.
	 */
	const UMALBossSkillKit* GetSkillKit() const;

	float GetHealthPercent() const;
	float GetPhaseInterval(int32 Phase) const;
	int32 GetPhaseBudget(int32 Phase) const;
	bool HasLineOfSight(AActor* Target) const;
	int32 GetConsecutiveUses(const TSubclassOf<UGameplayAbility>& AbilityClass) const;

	/**
	 * True while the ability's own CooldownGameplayEffect is still running.
	 *
	 * Read straight from GAS (GetCooldownTimeRemaining) instead of tracking a duplicate timer:
	 * a skill forced from Blueprint consumes the same cooldown the selector sees.
	 */
	bool IsSkillOnCooldown(const TSubclassOf<UGameplayAbility>& AbilityClass) const;

	/** Anti-repeat multiplier for one entry: 0.2 if it was the last pick, 0.5 if within the last three. */
	float GetRepeatPenalty(const FMALBossSkillEntry& Entry) const;

	/**
	 * Score one entry against the current target state, without the per-pick jitter.
	 *
	 * SelectAttack and DrawSkillScores both call this, so the numbers on screen are the numbers
	 * the picker uses. Filters (range, LOS, cooldown, phase weight) are the caller's job.
	 */
	float ScoreEntry(
		const FMALBossSkillEntry& Entry,
		float Distance,
		float FacingDot,
		bool bTargetBehind,
		float TargetHealthPercent,
		bool bTargetCasting,
		bool bTargetAttacking) const;

	/** Screen-left score table, one fixed row per kit entry. */
	void DrawSkillScores();
	void UpdatePhaseState();
	void StartPhaseTransition(int32 NewPhase);
	void TrimRecentAttacks();
	void WriteDebug(const FString& Text);
};
