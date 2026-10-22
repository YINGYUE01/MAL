// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameAbilities/MALAbility_Buff.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "GameAbilities/MALBuffGameplayEffect.h"
#include "GameAbilities/MALManaCostGameplayEffect.h"
#include "GameplayEffect.h"
#include "Tags/MALGameplayTags.h"

UMALAbility_Buff::UMALAbility_Buff()
{
	const UMALGameplayTags* Tags = UMALGameplayTags::Get();

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(Tags->Ability_Buff);
	SetAssetTags(AssetTags);

	InputActivatedTag = Tags->Input_Buff;

	// Owned for as long as the ability is active, which with a montage configured is exactly
	// the length of the cast. AMALPlayerController checks this tag to lock Move/Jump so the
	// character does not slide through the animation. Dropped automatically on EndAbility.
	ActivationOwnedTags.AddTag(Tags->Status_Casting);

	// Defaults so a bare Blueprint subclass works without wiring anything up. Both are meant
	// to be pointed at assets in the project (GE_Buff_DamageUp / GE_BuffCost).
	BuffEffectClass = UMALBuffGameplayEffect::StaticClass();
	CostGameplayEffectClass = UMALManaCostGameplayEffect::StaticClass();
}

void UMALAbility_Buff::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// Pay first. CanActivateAbility already rejected unaffordable casts through CheckCost;
	// committing here re-checks against the live value, which matters for predicted costs.
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// The buff lands before the cast animation even starts. The cost is already paid, so an
	// interrupted cast must not also cost the player the effect.
	ApplyBuff();

	// Nothing left to drive: the GE owns the buff from here on, and the GameplayCue it
	// carries owns the aura. Buffs with no cast animation end on this frame.
	if (!ActivationMontage)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// Play the montage on its DefaultSlot (full body). Slot routing lives in the montage
	// asset; the engine task handles replication, blend-out and cancellation correctly. The
	// ability stays active for the length of the cast and ends on the callbacks below.
	UAbilityTask_PlayMontageAndWait* PlayTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		ActivationMontage,
		1.0f,                  // Rate
		NAME_None,             // StartSection
		true,                  // bStopWhenAbilityEnds
		1.0f,                  // AnimRootMotionTranslationScale
		0.0f,                  // StartTimeSeconds
		false);                // bAllowInterruptAfterBlendOut

	if (!PlayTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	PlayTask->OnCompleted.AddDynamic(this, &UMALAbility_Buff::OnMontageCompleted);
	PlayTask->OnInterrupted.AddDynamic(this, &UMALAbility_Buff::OnMontageInterrupted);
	PlayTask->OnCancelled.AddDynamic(this, &UMALAbility_Buff::OnMontageCancelled);
	PlayTask->ReadyForActivation();
}

void UMALAbility_Buff::ApplyBuff()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC || !BuffEffectClass)
	{
		return;
	}

	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(this);

	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(BuffEffectClass, GetAbilityLevel(), Context);
	if (!Spec.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("UMALAbility_Buff: MakeOutgoingSpec failed for %s"), *GetNameSafe(BuffEffectClass.Get()));
		return;
	}

	// Magnitude and lifetime travel on the spec, so a single GE asset backs every buff
	// skill and designers only tune numbers on the ability.
	Spec.Data->SetSetByCallerMagnitude(UMALGameplayTags::Get()->Data_DamageBonus, DamageBonusMagnitude);
	Spec.Data->SetDuration(BuffDuration, /*bLockDuration*/ true);

	ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
}

void UMALAbility_Buff::OnMontageCompleted()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UMALAbility_Buff::OnMontageInterrupted()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
}

void UMALAbility_Buff::OnMontageCancelled()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
}
