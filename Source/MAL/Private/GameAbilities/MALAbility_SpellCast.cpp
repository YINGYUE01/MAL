// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameAbilities/MALAbility_SpellCast.h"

#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionMoveToForce.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "TimerManager.h"

UMALAbility_SpellCast::UMALAbility_SpellCast()
{
	// Motion is entirely notify-driven; there is no automatic lift on activation.
}

void UMALAbility_SpellCast::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	StopLiftTasks();
	bMontageFinished = !AbilityMontage;
	Phase = EPhase::Idle;
	bMotionSequenceActive = false;

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UMALAbility_SpellCast::NotifyBeginLift(float Duration)
{
	if (Duration <= 0.0f)
	{
		return;
	}

	// A second lift notify in the same montage is a no-op unless the previous chain was reset.
	if (Phase != EPhase::Idle)
	{
		return;
	}

	StartLift(Duration);
}

void UMALAbility_SpellCast::NotifyBeginHover()
{
	if (!bMotionSequenceActive)
	{
		return;
	}

	// End the lift task before switching the phase: EndTask can dispatch OnTimedOut synchronously.
	StopCurrentTask();
	Phase = EPhase::Hovering;
	ApplyHoverMovementMode();
}

void UMALAbility_SpellCast::NotifyBeginLanding(float Duration)
{
	if (!bMotionSequenceActive)
	{
		return;
	}

	if (Duration <= 0.0f)
	{
		FinishMotionSequence();
		return;
	}

	// Landing may begin directly from Lifting when there is no Hover state; both are valid.
	StopCurrentTask();
	Phase = EPhase::Landing;
	ApplyHoverMovementMode();
	StartLanding(Duration);
}

void UMALAbility_SpellCast::NotifyFinishLanding()
{
	if (Phase == EPhase::Landing)
	{
		Phase = EPhase::Idle;
		StopCurrentTask();
		FinishMotionSequence();
	}
}

void UMALAbility_SpellCast::OnAbilityMontageEnded(bool bInterrupted)
{
	if (bInterrupted)
	{
		StopLiftTasks();
		Super::OnAbilityMontageEnded(true);
		return;
	}

	bMontageFinished = true;

	// A landing task may legitimately outlive the montage; let its callback close the ability.
	if (Phase == EPhase::Landing)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(MontageTimeoutHandle);
		}
		return;
	}

	// No landing notify was reached. Do not leave the caster hovering forever.
	StopLiftTasks();
	Super::OnAbilityMontageEnded(false);
}

void UMALAbility_SpellCast::StartLift(float Duration)
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	UWorld* World = GetWorld();
	if (!Avatar || !World)
	{
		return;
	}

	LiftStartLocation = Avatar->GetActorLocation();
	bMotionSequenceActive = true;
	Phase = EPhase::Lifting;

	if (bUseConstantForce)
	{
		ForceLiftTask = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
			this, NAME_None, FVector::UpVector, LiftStrength, Duration,
			/*bIsAdditive*/ false, nullptr,
			ERootMotionFinishVelocityMode::SetVelocity, FVector::ZeroVector, 0.0f,
			bLiftWithGravity);

		if (!ForceLiftTask)
		{
			bMotionSequenceActive = false;
			Phase = EPhase::Idle;
			return;
		}

		bLiftActive = true;
		ForceLiftTask->OnFinish.AddDynamic(this, &UMALAbility_SpellCast::HandleLiftFinished);
		ForceLiftTask->ReadyForActivation();
	}
	else
	{
		const FVector TargetLocation = LiftStartLocation + FVector(0.0f, 0.0f, LiftHeight);
		MoveLiftTask = UAbilityTask_ApplyRootMotionMoveToForce::ApplyRootMotionMoveToForce(
			this, NAME_None, TargetLocation, Duration,
			/*bSetNewMovementMode*/ bSetFlyingDuringLift,
			bSetFlyingDuringLift ? MOVE_Flying : MOVE_None,
			/*bRestrictSpeedToExpected*/ true, nullptr,
			ERootMotionFinishVelocityMode::SetVelocity, FVector::ZeroVector, 0.0f);

		if (!MoveLiftTask)
		{
			bMotionSequenceActive = false;
			Phase = EPhase::Idle;
			return;
		}

		bLiftActive = true;
		MoveLiftTask->OnTimedOut.AddDynamic(this, &UMALAbility_SpellCast::HandleLiftFinished);
		MoveLiftTask->OnTimedOutAndDestinationReached.AddDynamic(this, &UMALAbility_SpellCast::HandleLiftFinished);
		MoveLiftTask->ReadyForActivation();
	}

	World->GetTimerManager().SetTimer(LiftTimeoutTimer, this,
		&UMALAbility_SpellCast::HandleLiftTimeout, Duration + 1.0f, false);
}

void UMALAbility_SpellCast::StartLanding(float Duration)
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	UWorld* World = GetWorld();
	if (!Avatar || !World)
	{
		FinishMotionSequence();
		return;
	}

	if (FVector::DistSquared(Avatar->GetActorLocation(), LiftStartLocation) <= 1.0f)
	{
		FinishMotionSequence();
		return;
	}

	MoveLiftTask = UAbilityTask_ApplyRootMotionMoveToForce::ApplyRootMotionMoveToForce(
		this, NAME_None, LiftStartLocation, Duration,
		/*bSetNewMovementMode*/ false, MOVE_None,
		/*bRestrictSpeedToExpected*/ true, nullptr,
		ERootMotionFinishVelocityMode::SetVelocity, FVector::ZeroVector, 0.0f);

	if (!MoveLiftTask)
	{
		FinishMotionSequence();
		return;
	}

	bLiftActive = true;
	Phase = EPhase::Landing;
	MoveLiftTask->OnTimedOut.AddDynamic(this, &UMALAbility_SpellCast::HandleLiftFinished);
	MoveLiftTask->OnTimedOutAndDestinationReached.AddDynamic(this, &UMALAbility_SpellCast::HandleLiftFinished);
	MoveLiftTask->ReadyForActivation();

	World->GetTimerManager().SetTimer(LiftTimeoutTimer, this,
		&UMALAbility_SpellCast::HandleLiftTimeout, Duration + 1.0f, false);
}

void UMALAbility_SpellCast::ApplyHoverMovementMode()
{
	if (ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			Movement->SetMovementMode(MOVE_Flying);
		}
	}
}

void UMALAbility_SpellCast::HandleLiftFinished()
{
	if (Phase == EPhase::Landing)
	{
		MoveLiftTask = nullptr;
		ForceLiftTask = nullptr;
		bLiftActive = false;

		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(LiftTimeoutTimer);
		}

		FinishMotionSequence();
		return;
	}

	// Lifting completed; wait for the hover/landing notify to advance the phase.
	bLiftActive = false;
	MoveLiftTask = nullptr;
	ForceLiftTask = nullptr;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LiftTimeoutTimer);
	}
}

void UMALAbility_SpellCast::HandleLiftTimeout()
{
	if (!IsActive())
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("%s: spell motion task never reported its end; releasing the caster"), *GetName());

	StopLiftTasks();
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
}

void UMALAbility_SpellCast::StopCurrentTask()
{
	if (IsValid(MoveLiftTask))
	{
		MoveLiftTask->EndTask();
	}
	MoveLiftTask = nullptr;

	if (IsValid(ForceLiftTask))
	{
		ForceLiftTask->EndTask();
	}
	ForceLiftTask = nullptr;

	bLiftActive = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LiftTimeoutTimer);
	}
}

void UMALAbility_SpellCast::StopLiftTasks()
{
	const bool bWasMoving = bMotionSequenceActive;

	// Mark idle first so a synchronous OnTimedOut from EndTask cannot treat this as a normal
	// landing completion and call EndAbility re-entrantly.
	Phase = EPhase::Idle;
	StopCurrentTask();
	bMotionSequenceActive = false;

	// Interrupted while lifted/hovering: do not leave the body in MOVE_Flying with no task.
	if (bWasMoving)
	{
		if (ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
		{
			if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
			{
				Movement->SetMovementMode(MOVE_Falling);
			}
		}
	}
}

void UMALAbility_SpellCast::FinishMotionSequence()
{
	Phase = EPhase::Idle;
	bMotionSequenceActive = false;
	bLiftActive = false;

	if (ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			Movement->SetMovementMode(MOVE_Falling);
		}
	}

	if (IsActive() && bMontageFinished)
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
	}
}

void UMALAbility_SpellCast::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	StopLiftTasks();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
