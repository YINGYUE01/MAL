// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ViewModel/MALAbilitySlotViewModel.h"

#include "AbilitySystemComponent.h"
#include "GameAbilities/MALGameplayAbility.h"

void UMALAbilitySlotViewModel::SetInputTag(const FGameplayTag& InInputTag)
{
	if (InputTag == InInputTag)
	{
		return;
	}

	InputTag = InInputTag;
	RebindAbility();
}

void UMALAbilitySlotViewModel::RebindAbility()
{
	// A forced sample: Refresh re-reads the ability on every call now, so a rebind is only the
	// "do it now" variant of the same work rather than a separate path.
	Refresh();
}

void UMALAbilitySlotViewModel::Refresh()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC)
	{
		ClearSlot();
		OnSlotRefreshed.Broadcast();
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = ASC->AbilityActorInfo.Get();
	FGameplayAbilitySpec* Spec = FindAbilitySpec();
	const UMALGameplayAbility* Ability = (Spec && Spec->Ability)
		? Cast<UMALGameplayAbility>(Spec->Ability)
		: nullptr;

	// Identity is sampled here as well, not only on a rebind: an unlock only adds a spec to the
	// ASC and tells nobody, so a slot that re-read its ability on rebind alone kept drawing the
	// empty frame until the next possession handed the bar a new ASC. The setters compare before
	// broadcasting, so repeating this on the cooldown tick costs the comparison and nothing else.
	UE_MVVM_SET_PROPERTY_VALUE(bHasAbility, Ability != nullptr);
	UE_MVVM_SET_PROPERTY_VALUE(DisplayName, Ability ? Ability->DisplayName : FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(Icon, Ability ? Ability->Icon : TObjectPtr<UTexture2D>());

	if (!Spec || !Ability || !ActorInfo)
	{
		UE_MVVM_SET_PROPERTY_VALUE(bIsOnCooldown, false);
		UE_MVVM_SET_PROPERTY_VALUE(CooldownPercent, 0.0f);
		UE_MVVM_SET_PROPERTY_VALUE(CooldownRemaining, 0.0f);
		UE_MVVM_SET_PROPERTY_VALUE(bCanAfford, true);
		OnSlotRefreshed.Broadcast();
		return;
	}

	// The engine derives both numbers from the cooldown tags on the owner, so an ability whose
	// cooldown effect grants no tags reports zeros here and the slot stays ready.
	float TimeRemaining = 0.0f;
	float CooldownDuration = 0.0f;
	Spec->Ability->GetCooldownTimeRemainingAndDuration(Spec->Handle, ActorInfo, TimeRemaining, CooldownDuration);

	const bool bCoolingDown = TimeRemaining > 0.0f;

	UE_MVVM_SET_PROPERTY_VALUE(bIsOnCooldown, bCoolingDown);
	UE_MVVM_SET_PROPERTY_VALUE(CooldownPercent,
		(bCoolingDown && CooldownDuration > 0.0f) ? FMath::Clamp(TimeRemaining / CooldownDuration, 0.0f, 1.0f) : 0.0f);
	UE_MVVM_SET_PROPERTY_VALUE(CooldownRemaining, bCoolingDown ? TimeRemaining : 0.0f);
	UE_MVVM_SET_PROPERTY_VALUE(bCanAfford, Spec->Ability->CheckCost(Spec->Handle, ActorInfo));

	OnSlotRefreshed.Broadcast();
}

void UMALAbilitySlotViewModel::OnAbilitySystemReady(UAbilitySystemComponent* InAbilitySystemComponent)
{
	Super::OnAbilitySystemReady(InAbilitySystemComponent);

	RebindAbility();
}

void UMALAbilitySlotViewModel::OnAbilitySystemReleased()
{
	Super::OnAbilitySystemReleased();

	// Not Refresh(): the base class swaps the cached ASC after this call, so a read here would
	// still see the old component. Blank the slot directly instead.
	ClearSlot();
}

void UMALAbilitySlotViewModel::ClearSlot()
{
	UE_MVVM_SET_PROPERTY_VALUE(bHasAbility, false);
	UE_MVVM_SET_PROPERTY_VALUE(DisplayName, FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(Icon, TObjectPtr<UTexture2D>());
	UE_MVVM_SET_PROPERTY_VALUE(bIsOnCooldown, false);
	UE_MVVM_SET_PROPERTY_VALUE(CooldownPercent, 0.0f);
	UE_MVVM_SET_PROPERTY_VALUE(CooldownRemaining, 0.0f);
	UE_MVVM_SET_PROPERTY_VALUE(bCanAfford, true);
}

FGameplayAbilitySpec* UMALAbilitySlotViewModel::FindAbilitySpec() const
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC || !InputTag.IsValid())
	{
		return nullptr;
	}

	// The input tag is stored in the spec's dynamic source tags, which is the same field the ASC
	// reads when it routes input, so the slot and the activation path can never disagree.
	for (FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			return &Spec;
		}
	}

	return nullptr;
}
