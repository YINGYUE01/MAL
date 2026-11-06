// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameAbilities/MALGameplayAbility.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "Components/MALAbilitySystemComponent.h"
#include "Engine/CurveTable.h"
#include "Curves/RealCurve.h"
#include "Engine/World.h"

UMALGameplayAbility::UMALGameplayAbility()
{
	// Default settings for MAL abilities
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

float UMALGameplayAbility::GetLevelScaledValue(FName RowName, float DefaultValue) const
{
	if (LevelScalingTable && !RowName.IsNone())
	{
		if (const FRealCurve* Curve = LevelScalingTable->FindCurve(
			RowName, TEXT("UMALGameplayAbility::GetLevelScaledValue"), false))
		{
			return Curve->Eval(static_cast<float>(GetAbilityLevel()));
		}
	}

	return DefaultValue;
}

void UMALGameplayAbility::ConfigureAbility(FGameplayTag InAbilityTag, FGameplayTag InInputTag, FGameplayTag InStatusTag, bool bBlocksSameStatusActivation)
{
	FGameplayTagContainer AssetTags;
	if (InAbilityTag.IsValid())
	{
		AssetTags.AddTag(InAbilityTag);
	}
	SetAssetTags(AssetTags);

	if (InInputTag.IsValid())
	{
		InputActivatedTag = InInputTag;
	}

	if (InStatusTag.IsValid())
	{
		// Owned for as long as the ability runs. AMALPlayerController reads these to lock
		// Move/Jump, so the lock and the ability share one lifetime.
		ActivationOwnedTags.AddTag(InStatusTag);

		// Holding the tag is also what makes two abilities of the same kind refuse to
		// overlap; asking for the block is the common case, hence the default.
		if (bBlocksSameStatusActivation)
		{
			ActivationBlockedTags.AddTag(InStatusTag);
		}
	}
}

UAbilityTask_PlayMontageAndWait* UMALGameplayAbility::PlayAbilityMontage(FName StartSection)
{
	// Resolved through a virtual because a subclass may take the montage from somewhere other
	// than the ability asset, such as the equipped weapon.
	UAnimMontage* Montage = GetAbilityMontage();
	if (!Montage)
	{
		return nullptr;
	}

	// A previous activation/task may have left a watchdog; never let two time out one ability.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MontageTimeoutHandle);
	}

	UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		Montage,
		1.0f,                  // Rate
		StartSection,
		true,                  // bStopWhenAbilityEnds
		1.0f,                  // AnimRootMotionTranslationScale
		0.0f,                  // StartTimeSeconds
		false);                // bAllowInterruptAfterBlendOut

	if (!Task)
	{
		return nullptr;
	}

	Task->OnCompleted.AddDynamic(this, &UMALGameplayAbility::HandleMontageCompleted);
	Task->OnInterrupted.AddDynamic(this, &UMALGameplayAbility::HandleMontageInterrupted);
	Task->OnCancelled.AddDynamic(this, &UMALGameplayAbility::HandleMontageCancelled);
	Task->ReadyForActivation();

	// Fail-safe. The montage is expected to end the ability through the delegates above; this
	// only fires when no delegate arrives (bad slot, animation layer swallowing the montage,
	// external StopAnimMontage without an interrupt callback, ...).
	if (UWorld* World = GetWorld())
	{
		const float Timeout = FMath::Max(Montage->GetPlayLength(), 0.1f) + 0.5f;
		World->GetTimerManager().SetTimer(
			MontageTimeoutHandle, this, &UMALGameplayAbility::HandleMontageTimeout, Timeout, false);
	}

	return Task;
}

void UMALGameplayAbility::HandleMontageCompleted()
{
	OnAbilityMontageEnded(false);
}

void UMALGameplayAbility::HandleMontageInterrupted()
{
	OnAbilityMontageEnded(true);
}

void UMALGameplayAbility::HandleMontageCancelled()
{
	OnAbilityMontageEnded(true);
}

void UMALGameplayAbility::OnAbilityMontageEnded(bool bInterrupted)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MontageTimeoutHandle);
	}

	// Nothing else is driving this ability: the animation running out is the end of it.
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, bInterrupted);
}

void UMALGameplayAbility::HandleMontageTimeout()
{
	if (!IsActive())
	{
		return;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("%s: montage %s never reported an end; forcing EndAbility so owned tags are released. Tags: %s"),
		*GetName(), *GetNameSafe(GetAbilityMontage()), *GetAssetTags().ToString());

	// Route through the virtual so subclasses (e.g. Buff) can clear their own timers too.
	OnAbilityMontageEnded(true);
}

void UMALGameplayAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MontageTimeoutHandle);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

bool UMALGameplayAbility::CommitAbilityOrEnd(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	// CommitAbility re-checks against live values, which matters for predicted costs: the
	// CanActivateAbility check ran before activation was confirmed.
	if (CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		return true;
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	return false;
}

void UMALGameplayAbility::OnInputPressed(const FInputActionValue& Value)
{
	// FInputActionValue::Get<FVector>() works for Bool/Axis1D/Axis2D/Axis3D and preserves
	// the axes that are available, so Blueprint can inspect the real input value.
	OnAbilityInputPressed.Broadcast(Value.Get<FVector>());
}

void UMALGameplayAbility::OnInputReleased(const FInputActionValue& Value)
{
	OnAbilityInputReleased.Broadcast(Value.Get<FVector>());
}

void UMALGameplayAbility::InputPressed(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	FInputActionValue Value;
	if (const UMALAbilitySystemComponent* MALASC =
		Cast<UMALAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo()))
	{
		Value = MALASC->GetLastInputValue(InputActivatedTag);
	}

	OnInputPressed(Value);
}

void UMALGameplayAbility::InputReleased(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	FInputActionValue Value;
	if (const UMALAbilitySystemComponent* MALASC =
		Cast<UMALAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo()))
	{
		Value = MALASC->GetLastInputValue(InputActivatedTag);
	}

	OnInputReleased(Value);
}
