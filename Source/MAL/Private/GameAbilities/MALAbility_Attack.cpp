// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameAbilities/MALAbility_Attack.h"

#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Character/MALCharacterBase.h"
#include "Combat/MALComboComponent.h"
#include "GameplayTagContainer.h"
#include "Tags/MALGameplayTags.h"

UMALAbility_Attack::UMALAbility_Attack()
{
	const UMALGameplayTags* Tags = UMALGameplayTags::Get();

	// Asset tag, activation input and the Status.Attacking it owns while running. The status
	// tag doubles as an activation blocker, which is what stops two swings overlapping.
	ConfigureAbility(Tags->Ability_Attack, Tags->Input_LMB, Tags->Status_Attacking);

	// The combo component already releases recovery, and it does so in a window-aware way: an
	// extra release on the melee base would end the ability between two steps.
	PostHitReleaseDelay = -1.f;

	// Default 4-step combo section names (override in Blueprint to match the montage).
	ComboSections = { FName("Combo1"), FName("Combo2"), FName("Combo3"), FName("Combo4") };
}

void UMALAbility_Attack::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// Combo state has to exist before the montage plays: the first notify can fire on the very
	// first frame, and the state machine is what decides which section that notify belongs to.
	if (UMALComboComponent* ComboComponent = GetComboComponent())
	{
		// Abilities are instanced and reused; a stale binding from a previous activation
		// would advance the combo twice.
		ComboComponent->OnComboAdvanced.RemoveAll(this);
		ComboComponent->OnComboAdvanced.AddUObject(this, &UMALAbility_Attack::OnComboAdvanced);
		ComboComponent->OnRecoveryExpired.RemoveAll(this);
		ComboComponent->OnRecoveryExpired.AddUObject(this, &UMALAbility_Attack::ReleaseRecovery);

		// The montage section is the only value both sides agree on, so let the state machine
		// chain from it instead of from its own cached step.
		ComboComponent->MontageStepQuery.BindUObject(this, &UMALAbility_Attack::GetCurrentComboStepFromMontage);
		ComboComponent->BeginCombo(ComboSections, ComboInputBufferWindow, RecoveryReleaseDelay);
	}

	// Commit, bind the weapon window, and play the montage -- all in the melee base.
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UMALAbility_Attack::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	UE_LOG(LogTemp, Log, TEXT("%s: combo ended. Cancelled: %s"), *GetName(), bWasCancelled ? TEXT("true") : TEXT("false"));

	if (UMALComboComponent* ComboComponent = GetComboComponent())
	{
		ComboComponent->OnComboAdvanced.RemoveAll(this);
		ComboComponent->OnRecoveryExpired.RemoveAll(this);
		ComboComponent->MontageStepQuery.Unbind();
		ComboComponent->EndCombo();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

FName UMALAbility_Attack::GetMontageStartSection() const
{
	return ComboSections.IsValidIndex(0) ? ComboSections[0] : NAME_None;
}

int32 UMALAbility_Attack::GetHitComboStep() const
{
	return GetCurrentComboStepFromMontage();
}

void UMALAbility_Attack::OpenComboWindow()
{
	if (UMALComboComponent* ComboComponent = GetComboComponent())
	{
		ComboComponent->OpenWindow();
	}
}

void UMALAbility_Attack::CloseComboWindow()
{
	if (UMALComboComponent* ComboComponent = GetComboComponent())
	{
		ComboComponent->CloseWindow();
	}
}

void UMALAbility_Attack::EnableWeaponCollision()
{
	// A new hit window means this section still owns the ability: a release left over from the
	// previous section would end it in the middle of the swing.
	if (UMALComboComponent* ComboComponent = GetComboComponent())
	{
		ComboComponent->CancelRecovery();
	}

	Super::EnableWeaponCollision();
}

void UMALAbility_Attack::DisableWeaponCollision()
{
	Super::DisableWeaponCollision();

	// The hit window is over; everything left in the montage is recovery. Do not keep the
	// character locked for animation nobody is going to watch.
	if (UMALComboComponent* ComboComponent = GetComboComponent())
	{
		ComboComponent->ScheduleRecovery();
	}
}

void UMALAbility_Attack::OnInputPressed(const FInputActionValue& Value)
{
	// Let the base forward/broadcast first, then hand the press to the combo state machine.
	Super::OnInputPressed(Value);

	if (UMALComboComponent* ComboComponent = GetComboComponent())
	{
		ComboComponent->PressAttack();
	}
}

UMALComboComponent* UMALAbility_Attack::GetComboComponent() const
{
	if (const AMALCharacterBase* Character = Cast<AMALCharacterBase>(GetAvatarActorFromActorInfo()))
	{
		return Character->GetComboComponent();
	}

	return nullptr;
}

void UMALAbility_Attack::OnComboAdvanced(int32 Step, FName Section)
{
	// Use the ability API (ASC::CurrentMontageJumpToSection under the hood): it sends a
	// reliable server RPC from clients and replicates the section to other clients.
	MontageJumpToSection(Section);

	UE_LOG(LogTemp, Log, TEXT("%s: advanced to combo step %d (section %s)"),
		*GetName(), Step, *Section.ToString());
}

int32 UMALAbility_Attack::GetCurrentComboStepFromMontage() const
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		const FName Section = ASC->GetCurrentMontageSectionName();
		const int32 Index = ComboSections.IndexOfByKey(Section);
		if (Index != INDEX_NONE)
		{
			return Index;
		}
	}

	// Montage is between sections (transition frame, or no montage yet): fall back to the
	// state machine's current step.
	if (const UMALComboComponent* ComboComponent = GetComboComponent())
	{
		return ComboComponent->GetStep();
	}

	return 0;
}

void UMALAbility_Attack::ReleaseRecovery()
{
	if (!IsActive())
	{
		return;
	}

	// EndAbility stops the montage through bStopWhenAbilityEnds, so the tail fades out on the
	// montage's own blend-out time instead of snapping. Status.Attacking drops with the ability,
	// which is exactly what AMALPlayerController gates Move and Jump on.
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}
