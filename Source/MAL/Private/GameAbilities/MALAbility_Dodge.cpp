// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameAbilities/MALAbility_Dodge.h"

#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Tags/MALGameplayTags.h"
#include "TimerManager.h"

UMALAbility_Dodge::UMALAbility_Dodge()
{
	const UMALGameplayTags* Tags = UMALGameplayTags::Get();

	ConfigureAbility(Tags->Ability_Player_Dodge, Tags->Input_Dodge, Tags->Status_Dodging);

	// The activation is the i-frame window: the executor discards damage on a target holding
	// this tag, so nothing else has to track a separate timer for it.
	ActivationOwnedTags.AddTag(Tags->Status_Invulnerable);

	// Cancelling the attack is the point of a dodge: without it the player can only dodge from
	// neutral, which is exactly when they do not need it.
	CancelAbilitiesWithTag.AddTag(Tags->Status_Attacking);
}

void UMALAbility_Dodge::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbilityOrEnd(Handle, ActorInfo, ActivationInfo))
	{
		return;
	}

	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// The move input is the dodge direction; with no input the player still expects forward,
	// which is what a backstep bound to a single key usually means.
	FVector Direction = Character->GetLastMovementInputVector();
	if (Direction.IsNearlyZero())
	{
		Direction = Character->GetActorForwardVector();
	}
	Direction = Direction.GetSafeNormal();

	const float Duration = FMath::Max(DodgeDuration, 0.05f);
	const float Speed = DodgeDistance / Duration;

	// A small lift keeps the capsule from catching on a step edge during the burst. It is
	// applied to the same launch so the dodge stays one movement instead of two.
	const FVector Impulse(Direction.X * Speed, Direction.Y * Speed, bLiftWhileDodging ? 200.0f : 0.0f);
	Character->LaunchCharacter(Impulse, true, true);

	UE_LOG(LogTemp, Log, TEXT("%s: dodging %s over %.2fs (%.0f cm/s)."),
		*Character->GetName(), *Direction.ToCompactString(), Duration, Speed);

	// Optional: the montage is presentation. The ability still ends on its own clock.
	if (GetAbilityMontage())
	{
		PlayAbilityMontage(NAME_None);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(DodgeEndTimer, this,
			&UMALAbility_Dodge::HandleDodgeEnd, Duration, false);
	}
}

void UMALAbility_Dodge::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DodgeEndTimer);
	}
	DodgeEndTimer.Invalidate();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UMALAbility_Dodge::HandleDodgeEnd()
{
	DodgeEndTimer.Invalidate();

	if (!IsActive())
	{
		return;
	}

	// Not cancelled: the dodge completed, and the ASC should not treat it as interrupted.
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}
