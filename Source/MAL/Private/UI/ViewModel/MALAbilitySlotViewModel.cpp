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
	const FGameplayAbilitySpec* Spec = FindAbilitySpec();
	const UMALGameplayAbility* Ability = Spec ? Cast<UMALGameplayAbility>(Spec->Ability) : nullptr;

	UE_MVVM_SET_PROPERTY_VALUE(bHasAbility, Ability != nullptr);
	UE_MVVM_SET_PROPERTY_VALUE(DisplayName, Ability ? Ability->DisplayName : FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(Icon, Ability ? Ability->Icon : TObjectPtr<UTexture2D>());

	Refresh();
}

void UMALAbilitySlotViewModel::Refresh()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC)
	{
		OnSlotRefreshed.Broadcast();
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = ASC->AbilityActorInfo.Get();
	FGameplayAbilitySpec* Spec = FindAbilitySpec();

	if (!Spec || !Spec->Ability || !ActorInfo)
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

	// Not RebindAbility(): the base class swaps the cached ASC after this call, so a rebind here
	// would still read the old component. Clear the slot directly instead.
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
