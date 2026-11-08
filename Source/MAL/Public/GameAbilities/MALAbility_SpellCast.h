// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameAbilities/MALAbility_RangedAttack.h"
#include "MALAbility_SpellCast.generated.h"

class UAbilityTask_ApplyRootMotionMoveToForce;
class UAbilityTask_ApplyRootMotionConstantForce;

/**
 * Ranged spell whose lift / hover / landing phases are timed by animation notifies.
 *
 * Animation owns the timeline:
 *   MAL Spell Lift (state)    -> begin lift, end -> hover
 *   MAL Spell Hover (state)   -> hold MOVE_Flying
 *   MAL Spell Landing (state) -> begin landing, end -> release gravity
 *
 * The ability keeps the root motion tasks and the movement-mode transitions; only the phase
 * timing comes from the montage.
 */
UCLASS()
class MAL_API UMALAbility_SpellCast : public UMALAbility_RangedAttack
{
	GENERATED_BODY()

public:
	UMALAbility_SpellCast();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	virtual void OnAbilityMontageEnded(bool bInterrupted) override;

	/** Notify entry points. Duration is the notify-state interval in seconds. */
	void NotifyBeginLift(float Duration);
	void NotifyBeginHover();
	void NotifyBeginLanding(float Duration);
	void NotifyFinishLanding();

protected:
	/** ConstantForce gives a continuous push; otherwise MoveToForce rises by LiftHeight. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Spell|Lift")
	bool bUseConstantForce = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Spell|Lift", meta = (EditCondition = "!bUseConstantForce", ClampMin = "0.0"))
	float LiftHeight = 200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Spell|Lift", meta = (EditCondition = "bUseConstantForce", ClampMin = "0.0"))
	float LiftStrength = 600.0f;

	/** MoveToForce: hold MOVE_Flying during the lift instead of letting gravity pull the body down. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Spell|Lift", meta = (EditCondition = "!bUseConstantForce"))
	bool bSetFlyingDuringLift = true;

	/** ConstantForce: leave gravity enabled so the upward force has to overcome it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Spell|Lift", meta = (EditCondition = "bUseConstantForce"))
	bool bLiftWithGravity = false;

private:
	enum class EPhase : uint8
	{
		Idle,
		Lifting,
		Hovering,
		Landing
	};

	FTimerHandle LiftTimeoutTimer;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_ApplyRootMotionMoveToForce> MoveLiftTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_ApplyRootMotionConstantForce> ForceLiftTask;

	FVector LiftStartLocation = FVector::ZeroVector;
	bool bLiftActive = false;
	bool bMontageFinished = false;
	bool bMotionSequenceActive = false;
	EPhase Phase = EPhase::Idle;

	void StartLift(float Duration);
	void StartLanding(float Duration);
	void StopCurrentTask();
	void StopLiftTasks();
	void FinishMotionSequence();
	void ApplyHoverMovementMode();

	UFUNCTION()
	void HandleLiftFinished();

	UFUNCTION()
	void HandleLiftTimeout();
};
