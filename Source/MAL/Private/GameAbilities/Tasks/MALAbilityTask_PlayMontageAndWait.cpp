// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameAbilities/Tasks/MALAbilityTask_PlayMontageAndWait.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemLog.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MALAbilityTask_PlayMontageAndWait)

static bool GMALAggressiveEndTask = true;
static FAutoConsoleVariableRef CVarMALAggressiveEndTask(
	TEXT("AbilitySystem.MALPlayMontage.AggressiveEndTask"),
	GMALAggressiveEndTask,
	TEXT("When true, MALPlayMontageAndWait will aggressively end task after blend out to avoid duplicate callbacks."));

UMALAbilityTask_PlayMontageAndWait::UMALAbilityTask_PlayMontageAndWait()
{
	Rate = 1.0f;
	StartSection = NAME_None;
	StartTimeSeconds = 0.0f;
	AnimRootMotionTranslationScale = 1.0f;
	bStopWhenAbilityEnds = true;
	bMontageFinished = false;
	bAllowInterruptAfterBlendOut = false;
}

void UMALAbilityTask_PlayMontageAndWait::Activate()
{
	if (!Ability)
	{
		return;
	}

	bool bPlayedMontage = false;

	if (UAbilitySystemComponent* ASC = AbilitySystemComponent.Get())
	{
		const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
		UAnimInstance* AnimInstance = ActorInfo ? ActorInfo->GetAnimInstance() : nullptr;
		if (AnimInstance && Montage)
		{
			// Play through ASC for proper GAS integration (replication, montage tracking, etc.)
			// Slot routing is determined by the Montage asset's SlotAnimTracks configuration in the editor.
			// Ensure your montage asset has its slot track set to the desired name (e.g. "UpperBody").
			float MontageLength = ASC->PlayMontage(Ability, Ability->GetCurrentActivationInfo(), Montage, Rate, StartSection, StartTimeSeconds);

			if (MontageLength > 0.f)
			{
				// Playing could fire a callback that kills this ability - check if we're still valid
				if (!ShouldBroadcastAbilityTaskDelegates())
				{
					return;
				}

				// Bind to ability cancelled event
				InterruptedHandle = Ability->OnGameplayAbilityCancelled.AddUObject(this, &UMALAbilityTask_PlayMontageAndWait::OnGameplayAbilityCancelled);

				// Bind to montage events using per-montage delegates
				BlendedInDelegate.BindUObject(this, &UMALAbilityTask_PlayMontageAndWait::OnMontageBlendedIn);
				AnimInstance->Montage_SetBlendedInDelegate(BlendedInDelegate, Montage);

				BlendingOutDelegate.BindUObject(this, &UMALAbilityTask_PlayMontageAndWait::OnMontageBlendingOut);
				AnimInstance->Montage_SetBlendingOutDelegate(BlendingOutDelegate, Montage);

				MontageEndedDelegate.BindUObject(this, &UMALAbilityTask_PlayMontageAndWait::OnMontageEnded);
				AnimInstance->Montage_SetEndDelegate(MontageEndedDelegate, Montage);

				// Handle root motion scale
				ACharacter* Character = Cast<ACharacter>(GetAvatarActor());
				if (Character && (Character->GetLocalRole() == ROLE_Authority ||
					(Character->GetLocalRole() == ROLE_AutonomousProxy &&
						Ability->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalPredicted)))
				{
					Character->SetAnimRootMotionTranslationScale(AnimRootMotionTranslationScale);
				}

				bPlayedMontage = true;
			}
		}
		else
		{
			ABILITY_LOG(Warning, TEXT("UMALAbilityTask_PlayMontageAndWait: AnimInstance is null or Montage is null"));
		}
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("UMALAbilityTask_PlayMontageAndWait: AbilitySystemComponent is null"));
	}

	if (!bPlayedMontage)
	{
		ABILITY_LOG(Warning, TEXT("UMALAbilityTask_PlayMontageAndWait: Failed to play montage %s in ability %s"),
			*GetNameSafe(Montage), *Ability->GetName());
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnCancelled.Broadcast();
		}
	}

	SetWaitingOnAvatar();
}

void UMALAbilityTask_PlayMontageAndWait::OnMontageBlendedIn(UAnimMontage* InMontage)
{
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		// No OnBlendedIn delegate in our public API, but we could add one later
	}
}

void UMALAbilityTask_PlayMontageAndWait::OnMontageBlendingOut(UAnimMontage* InMontage, bool bInterrupted)
{
	const bool bPlayingThisMontage = (InMontage == Montage) && Ability && Ability->GetCurrentMontage() == Montage;
	if (bPlayingThisMontage)
	{
		// Reset root motion scale
		ACharacter* Character = Cast<ACharacter>(GetAvatarActor());
		if (Character && (Character->GetLocalRole() == ROLE_Authority ||
			(Character->GetLocalRole() == ROLE_AutonomousProxy &&
				Ability->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalPredicted)))
		{
			Character->SetAnimRootMotionTranslationScale(1.f);
		}
	}

	if (bPlayingThisMontage && (bInterrupted || !bAllowInterruptAfterBlendOut))
	{
		if (UAbilitySystemComponent* ASC = AbilitySystemComponent.Get())
		{
			ASC->ClearAnimatingAbility(Ability);
		}
	}

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		if (bInterrupted)
		{
			bAllowInterruptAfterBlendOut = false;
			OnInterrupted.Broadcast();

			if (GMALAggressiveEndTask)
			{
				EndTask();
			}
		}
		else
		{
			OnBlendOut.Broadcast();
		}
	}
}

void UMALAbilityTask_PlayMontageAndWait::OnGameplayAbilityCancelled()
{
	if (StopPlayingMontage() || bAllowInterruptAfterBlendOut)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			bAllowInterruptAfterBlendOut = false;
			OnInterrupted.Broadcast();
		}
	}

	if (GMALAggressiveEndTask)
	{
		EndTask();
	}
}

void UMALAbilityTask_PlayMontageAndWait::OnMontageEnded(UAnimMontage* InMontage, bool bInterrupted)
{
	if (!bInterrupted)
	{
		bMontageFinished = true;
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnCompleted.Broadcast();
		}
	}
	else if (bAllowInterruptAfterBlendOut)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnInterrupted.Broadcast();
		}
	}

	EndTask();
}

void UMALAbilityTask_PlayMontageAndWait::OnDestroy(bool AbilityEnded)
{
	// Clear the ability cancelled delegate
	if (Ability)
	{
		Ability->OnGameplayAbilityCancelled.Remove(InterruptedHandle);
		if (AbilityEnded && bStopWhenAbilityEnds)
		{
			StopPlayingMontage();
		}
	}

	Super::OnDestroy(AbilityEnded);
}

bool UMALAbilityTask_PlayMontageAndWait::StopPlayingMontage()
{
	if (!Ability)
	{
		return false;
	}

	const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
	if (!ActorInfo)
	{
		return false;
	}

	UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();
	if (!AnimInstance)
	{
		return false;
	}

	UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	if (ASC && Ability)
	{
		if (ASC->GetAnimatingAbility() == Ability && ASC->GetCurrentMontage() == Montage)
		{
			// Unbind delegates so they don't fire during stop
			FAnimMontageInstance* MontageInstance = AnimInstance->GetActiveInstanceForMontage(Montage);
			if (MontageInstance)
			{
				MontageInstance->OnMontageBlendedInEnded.Unbind();
				MontageInstance->OnMontageBlendingOutStarted.Unbind();
				MontageInstance->OnMontageEnded.Unbind();
			}

			ASC->CurrentMontageStop();
			return true;
		}
	}

	return false;
}

bool UMALAbilityTask_PlayMontageAndWait::WasMontagePlayedSuccessfully() const
{
	return bMontageFinished;
}

FString UMALAbilityTask_PlayMontageAndWait::GetDebugString() const
{
	UAnimMontage* PlayingMontage = nullptr;
	if (Ability)
	{
		const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
		UAnimInstance* AnimInstance = ActorInfo ? ActorInfo->GetAnimInstance() : nullptr;
		if (AnimInstance)
		{
			PlayingMontage = AnimInstance->Montage_IsActive(Montage) ? ToRawPtr(Montage) : AnimInstance->GetCurrentActiveMontage();
		}
	}

	return FString::Printf(TEXT("MALPlayMontageAndWait. Montage: %s (Currently: %s), Slot: %s"),
		*GetNameSafe(Montage),
		*GetNameSafe(PlayingMontage),
		PlaySlotName != NAME_None ? *PlaySlotName.ToString() : TEXT("Default"));
}

UMALAbilityTask_PlayMontageAndWait* UMALAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
	UGameplayAbility* OwningAbility,
	FName TaskInstanceName,
	UAnimMontage* MontageToPlay,
	float InRate,
	FName InStartSection,
	bool bInStopWhenAbilityEnds,
	float InAnimRootMotionTranslationScale,
	float InStartTimeSeconds,
	FName InSlotName)
{
	UAbilitySystemGlobals::NonShipping_ApplyGlobalAbilityScaler_Rate(InRate);

	UMALAbilityTask_PlayMontageAndWait* NewTask = NewAbilityTask<UMALAbilityTask_PlayMontageAndWait>(OwningAbility, TaskInstanceName);
	if (!NewTask)
	{
		return nullptr;
	}

	NewTask->Montage = MontageToPlay;
	NewTask->Rate = InRate;
	NewTask->StartSection = InStartSection;
	NewTask->bStopWhenAbilityEnds = bInStopWhenAbilityEnds;
	NewTask->AnimRootMotionTranslationScale = InAnimRootMotionTranslationScale;
	NewTask->StartTimeSeconds = InStartTimeSeconds;
	NewTask->PlaySlotName = InSlotName;

	return NewTask;
}
