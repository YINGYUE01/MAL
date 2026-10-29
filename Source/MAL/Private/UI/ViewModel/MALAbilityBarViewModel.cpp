// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ViewModel/MALAbilityBarViewModel.h"

#include "AbilitySystemComponent.h"
#include "UI/ViewModel/MALAbilitySlotViewModel.h"

void UMALAbilityBarViewModel::InitializeSlots(const TArray<FGameplayTag>& SlotInputTags)
{
	TArray<TObjectPtr<UMALAbilitySlotViewModel>> NewSlots;
	NewSlots.Reserve(SlotInputTags.Num());

	for (const FGameplayTag& SlotInputTag : SlotInputTags)
	{
		UMALAbilitySlotViewModel* Slot = NewObject<UMALAbilitySlotViewModel>(this);
		Slot->SetInputTag(SlotInputTag);
		NewSlots.Add(Slot);
	}

	UE_MVVM_SET_PROPERTY_VALUE(Slots, NewSlots);

	// A HUD can be rebuilt on top of a live ASC, so freshly built slots bind straight away
	// instead of waiting for the next possession.
	ApplyAbilitySystemToSlots(GetAbilitySystemComponent());
}

void UMALAbilityBarViewModel::RefreshSlots()
{
	for (const TObjectPtr<UMALAbilitySlotViewModel>& Slot : Slots)
	{
		if (Slot)
		{
			Slot->RebindAbility();
		}
	}
}

void UMALAbilityBarViewModel::OnAbilitySystemReady(UAbilitySystemComponent* InAbilitySystemComponent)
{
	Super::OnAbilitySystemReady(InAbilitySystemComponent);

	ApplyAbilitySystemToSlots(InAbilitySystemComponent);
	StartCooldownTicker();
}

void UMALAbilityBarViewModel::OnAbilitySystemReleased()
{
	Super::OnAbilitySystemReleased();

	StopTicker();

	// Passing nullptr walks each slot through its own release path, which clears the icon and
	// the cooldown so a detached bar cannot keep showing the previous owner's abilities.
	ApplyAbilitySystemToSlots(nullptr);
}

void UMALAbilityBarViewModel::ApplyAbilitySystemToSlots(UAbilitySystemComponent* InAbilitySystemComponent)
{
	for (const TObjectPtr<UMALAbilitySlotViewModel>& Slot : Slots)
	{
		if (Slot)
		{
			Slot->SetAbilitySystemComponent(InAbilitySystemComponent);
		}
	}
}

void UMALAbilityBarViewModel::StartCooldownTicker()
{
	RefreshAccumulator = 0.0f;

	StartTicker([this](float DeltaTime)
	{
		return TickCooldowns(DeltaTime);
	});
}

bool UMALAbilityBarViewModel::TickCooldowns(float DeltaTime)
{
	RefreshAccumulator += DeltaTime;
	if (RefreshAccumulator < CooldownRefreshInterval)
	{
		return true;
	}

	RefreshAccumulator = 0.0f;

	for (const TObjectPtr<UMALAbilitySlotViewModel>& Slot : Slots)
	{
		if (Slot)
		{
			Slot->Refresh();
		}
	}

	return true;
}
