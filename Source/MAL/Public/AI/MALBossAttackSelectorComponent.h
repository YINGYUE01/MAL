// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/MALBossAttackSet.h"
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

	/** Shared move set and phase thresholds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Boss")
	TObjectPtr<UMALBossAttackSet> AttackSet;

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
	 * The winner is uniform among these, so close scores produce real variation.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Boss|Selection", meta = (ClampMin = "1", ClampMax = "4"))
	int32 TopCandidateCount = 3;

	/**
	 * Pick an attack for Target. Pure: it does not start cooldowns or mark the attack used.
	 * ActivateLastSelectedAttack does that on a successful activation.
	 */
	UFUNCTION(BlueprintCallable, Category = "MAL|Boss|Selection")
	bool SelectAttack(AActor* Target, FMALBossAttackEntry& OutAttack, FString& OutDebug);

	/** Activate the entry produced by the last successful SelectAttack. Server only. */
	UFUNCTION(BlueprintCallable, Category = "MAL|Boss|Selection")
	bool ActivateLastSelectedAttack();

	/** Called when the selected ability has ended; starts cooldown and cadence. */
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
	/** World seconds at which each class becomes ready again. */
	TMap<TSubclassOf<UGameplayAbility>, float> ReadyTimes;

	/** Last few classes, newest at the end. */
	TArray<TSubclassOf<UGameplayAbility>> RecentAttacks;

	/** Start times of attacks inside the rolling budget window. */
	TArray<float> AttackTimes;

	FMALBossAttackEntry LastSelectedAttack;
	bool bHasSelection = false;
	bool bAttackInProgress = false;

	TSubclassOf<UGameplayAbility> ActiveAttackClass;
	float ActiveAttackCooldown = 0.0f;

	float NextAttackAllowedTime = 0.0f;
	float PhaseTransitionEndTime = 0.0f;
	int32 CurrentPhase = 0;
	int32 PendingPhase = 0;
	bool bHasPendingPhase = false;

	int32 PlayerHitStreak = 0;
	int32 PlayerEvadeStreak = 0;

	FString LastSelectionDebug;

	AMALCharacterBase* GetOwnerCharacter() const;
	float GetHealthPercent() const;
	float GetPhaseInterval(int32 Phase) const;
	int32 GetPhaseBudget(int32 Phase) const;
	bool HasLineOfSight(AActor* Target) const;
	int32 GetConsecutiveUses(const TSubclassOf<UGameplayAbility>& AbilityClass) const;
	void UpdatePhaseState();
	void StartPhaseTransition(int32 NewPhase);
	void TrimRecentAttacks();
	void WriteDebug(const FString& Text);
};
