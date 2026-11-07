// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameAbilities/MALAbility_HitReact.h"

#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Character/MALCharacterBase.h"
#include "Components/MALAbilitySystemComponent.h"
#include "Engine/World.h"
#include "Tags/MALGameplayTags.h"

UMALAbility_HitReact::UMALAbility_HitReact()
{
	// One instance per actor: the montage task needs to survive re-activation attempts, and a
	// second hit while the first reaction runs must not spawn a parallel copy.
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	bRetriggerInstancedAbility = false;

	// The server decides when a hit lands; clients receive the activation and the montage from
	// the ability system, so no custom multicast is involved.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;

	const UMALGameplayTags* Tags = UMALGameplayTags::Get();

	// Status.HitReacting both marks the body as busy and blocks a second reaction while this one
	// runs. No input tag: the ability is only ever triggered by the damage event below.
	ConfigureAbility(Tags->Ability_Common_HitReact, FGameplayTag(), Tags->Status_HitReacting, true);

	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = Tags->Event_HitReact;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);
}

void UMALAbility_HitReact::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UWorld* World = GetWorld();
	AMALCharacterBase* Character = ActorInfo ? Cast<AMALCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	UAnimMontage* Montage = Character ? Character->GetHitReactMontage() : nullptr;

	if (!World || !Character || !Montage)
	{
		UE_LOG(LogTemp, Verbose, TEXT("UMALAbility_HitReact: no world/character/montage on %s"),
			*GetNameSafe(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr));

		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility*/ true, /*bWasCancelled*/ false);
		return;
	}

	// Rate limit: a damage-over-time tick should not restart the full-body animation every frame.
	if (ReactCooldown > 0.0f && (World->GetTimeSeconds() - LastReactTime) < ReactCooldown)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility*/ true, /*bWasCancelled*/ false);
		return;
	}

	if (!CommitAbilityOrEnd(Handle, ActorInfo, ActivationInfo))
	{
		// CommitAbilityOrEnd already ended the ability on a failed cost/cooldown.
		return;
	}

	LastReactTime = World->GetTimeSeconds();

	// The reaction owns the body: cancel the swing or cast so the flinch is not blended away by
	// an animation that keeps running underneath it.
	//
	// CancelAbilities matches the target ability's AssetTags, not its ActivationOwnedTags. The
	// old Status.Attacking/Status.Casting tags therefore cancelled nothing: an attack kept its
	// montage task alive under the flinch, and whichever montage lost the slot could leave its
	// owned status tag behind. Cancel by ability identity instead.
	
	if (UMALAbilitySystemComponent* AbilitySystem = Character->GetMALAbilitySystemComponent())
	{
		const UMALGameplayTags* Tags = UMALGameplayTags::Get();

		FGameplayTagContainer CancelTags;
		CancelTags.AddTag(Tags->Ability_Player_Attack);
		CancelTags.AddTag(Tags->Ability_Player_Buff);
		CancelTags.AddTag(Tags->Ability_Enemy_Attack);

		// Legacy flat tags: assets that have not been migrated to the ownership hierarchy yet.
		// CancelTags.AddTag(Tags->Ability_Attack);
		// CancelTags.AddTag(Tags->Ability_Buff);

		AbilitySystem->CancelAbilities(&CancelTags, /*WithoutTags*/ nullptr, /*Ignore*/ this);
	}

	// GetAbilityMontage returns the character's montage; the base task handles replication,
	// blend-out and ending the ability when the animation finishes.
	if (!PlayAbilityMontage())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility*/ true, /*bWasCancelled*/ false);
	}
}

UAnimMontage* UMALAbility_HitReact::GetAbilityMontage() const
{
	const AMALCharacterBase* Character = Cast<AMALCharacterBase>(GetAvatarActorFromActorInfo());
	return Character ? Character->GetHitReactMontage() : nullptr;
}
