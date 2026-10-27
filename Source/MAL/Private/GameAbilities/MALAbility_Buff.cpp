// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameAbilities/MALAbility_Buff.h"

#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "GameAbilities/MALBuffGameplayEffect.h"
#include "GameAbilities/MALManaCostGameplayEffect.h"
#include "GameplayEffect.h"
#include "Tags/MALGameplayTags.h"
#include "TimerManager.h"

UMALAbility_Buff::UMALAbility_Buff()
{
	const UMALGameplayTags* Tags = UMALGameplayTags::Get();

	// Status.Casting is owned for as long as the ability is active, which with a montage
	// configured is exactly the length of the cast. AMALPlayerController checks this tag to lock
	// Move/Jump so the character does not slide through the animation. Blocking is left off: a
	// second cast mid-animation used to be allowed, and that behaviour is not this change's to
	// alter.
	ConfigureAbility(Tags->Ability_Buff, Tags->Input_Buff, Tags->Status_Casting, /*bBlocksSameStatusActivation*/ false);

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
	if (!CommitAbilityOrEnd(Handle, ActorInfo, ActivationInfo))
	{
		return;
	}

	// The buff lands before the cast animation even starts. The cost is already paid, so an
	// interrupted cast must not also cost the player the effect.
	ApplyBuff();

	// Nothing left to drive: the GE owns the buff from here on, and the GameplayCue it
	// carries owns the aura. Buffs with no cast animation end on this frame.
	if (!AbilityMontage)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// The base plays the montage on its DefaultSlot and ends the ability from its callbacks, so
	// the ability lives exactly as long as the cast. Slot routing lives in the montage asset.
	if (!PlayAbilityMontage())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// A montage that never reports back would hold Status.Casting -- and with it the movement
	// lock -- for the rest of the session. The timeout ends the cast one beat after the
	// animation should have finished; on the normal path OnAbilityMontageEnded clears it first.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(CastTimeoutHandle, this,
			&UMALAbility_Buff::HandleCastTimeout, AbilityMontage->GetPlayLength() + 0.5f, false);
	}
}

void UMALAbility_Buff::OnAbilityMontageEnded(bool bInterrupted)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CastTimeoutHandle);
	}

	Super::OnAbilityMontageEnded(bInterrupted);
}

void UMALAbility_Buff::HandleCastTimeout()
{
	// Reaching here means the montage never reported finishing, which is a bug in the asset or
	// in the animation setup rather than an expected path -- hence the warning.
	UE_LOG(LogTemp, Warning,
		TEXT("UMALAbility_Buff: %s never reported its montage ending. Ending the cast so %s is released."),
		*GetName(), TEXT("MAL.Status.Casting"));

	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
}

void UMALAbility_Buff::PostLoad()
{
	Super::PostLoad();

	// AbilityMontage replaced ActivationMontage when the montage moved to the base class. Assets
	// saved before that still carry the old property; copy it once so existing blueprints keep
	// the cast animation they were configured with.
	if (!ActivationMontage)
	{
		return;
	}

	if (!AbilityMontage)
	{
		AbilityMontage = ActivationMontage;
	}
	else if (AbilityMontage != ActivationMontage)
	{
		// Both set: the new property is the one that is still visible in the details panel, so
		// it wins. Logged because the old reference is about to be dropped for good.
		UE_LOG(LogTemp, Warning,
			TEXT("%s: both AbilityMontage (%s) and deprecated ActivationMontage (%s) are set; keeping AbilityMontage"),
			*GetName(), *GetNameSafe(AbilityMontage), *GetNameSafe(ActivationMontage));
	}

	ActivationMontage = nullptr;
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
