// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ViewModel/MALStatViewModel.h"

#include "AbilitySystem/Attributes/MALAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Containers/Ticker.h"

namespace
{
	/** Clamped 0..1 ratio; a zero max (attributes not applied yet) reports empty rather than full. */
	float ComputePercent(float Current, float Max)
	{
		return (Max > 0.0f) ? FMath::Clamp(Current / Max, 0.0f, 1.0f) : 0.0f;
	}
}

void UMALStatViewModel::OnAbilitySystemReady(UAbilitySystemComponent* InAbilitySystemComponent)
{
	Super::OnAbilitySystemReady(InAbilitySystemComponent);

	BindAttribute(UMALAttributeSet::GetHealthAttribute(), [this](const FOnAttributeChangeData& Data)
	{
		UE_MVVM_SET_PROPERTY_VALUE(Health, Data.NewValue);
		RefreshHealthPercent();
	});

	BindAttribute(UMALAttributeSet::GetMaxHealthAttribute(), [this](const FOnAttributeChangeData& Data)
	{
		UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, Data.NewValue);
		RefreshHealthPercent();
	});

	BindAttribute(UMALAttributeSet::GetManaAttribute(), [this](const FOnAttributeChangeData& Data)
	{
		UE_MVVM_SET_PROPERTY_VALUE(Mana, Data.NewValue);
		RefreshManaPercent();
	});

	BindAttribute(UMALAttributeSet::GetMaxManaAttribute(), [this](const FOnAttributeChangeData& Data)
	{
		UE_MVVM_SET_PROPERTY_VALUE(MaxMana, Data.NewValue);
		RefreshManaPercent();
	});

	BindAttribute(UMALAttributeSet::GetStaminaAttribute(), [this](const FOnAttributeChangeData& Data)
	{
		UE_MVVM_SET_PROPERTY_VALUE(Stamina, Data.NewValue);
		RefreshStaminaPercent();
	});

	BindAttribute(UMALAttributeSet::GetMaxStaminaAttribute(), [this](const FOnAttributeChangeData& Data)
	{
		UE_MVVM_SET_PROPERTY_VALUE(MaxStamina, Data.NewValue);
		RefreshStaminaPercent();
	});
}

void UMALStatViewModel::OnAbilitySystemReleased()
{
	Super::OnAbilitySystemReleased();

	// The bars must not keep ticking against the previous character's values.
	StopHealthBarTicker();
	LaggedHealthHoldRemaining = 0.0f;
	bHealthBarsInitialized = false;

	// The ASC went away (respawn, level unload). Clear the presentation copy so a reused widget
	// never keeps showing the previous character's vitals.
	UE_MVVM_SET_PROPERTY_VALUE(Health, 0.0f);
	UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, 0.0f);
	UE_MVVM_SET_PROPERTY_VALUE(HealthPercent, 0.0f);
	UE_MVVM_SET_PROPERTY_VALUE(DisplayedHealthPercent, 0.0f);
	UE_MVVM_SET_PROPERTY_VALUE(LaggedHealthPercent, 0.0f);
	UE_MVVM_SET_PROPERTY_VALUE(bIsDead, false);
	UE_MVVM_SET_PROPERTY_VALUE(Mana, 0.0f);
	UE_MVVM_SET_PROPERTY_VALUE(MaxMana, 0.0f);
	UE_MVVM_SET_PROPERTY_VALUE(ManaPercent, 0.0f);
	UE_MVVM_SET_PROPERTY_VALUE(Stamina, 0.0f);
	UE_MVVM_SET_PROPERTY_VALUE(MaxStamina, 0.0f);
	UE_MVVM_SET_PROPERTY_VALUE(StaminaPercent, 0.0f);
}

void UMALStatViewModel::RefreshHealthPercent()
{
	UE_MVVM_SET_PROPERTY_VALUE(HealthPercent, ComputePercent(Health, MaxHealth));
	UE_MVVM_SET_PROPERTY_VALUE(bIsDead, Health <= 0.0f && MaxHealth > 0.0f);

	UpdateHealthBars();
}

void UMALStatViewModel::UpdateHealthBars()
{
	if (!bHealthBarsInitialized)
	{
		// First value after binding. Start already settled: otherwise creating the HUD would play
		// a full-to-current drain before the player has taken a single hit.
		bHealthBarsInitialized = true;
		UE_MVVM_SET_PROPERTY_VALUE(DisplayedHealthPercent, HealthPercent);
		UE_MVVM_SET_PROPERTY_VALUE(LaggedHealthPercent, HealthPercent);
		return;
	}

	if (HealthPercent >= LaggedHealthPercent)
	{
		// Healing, or a value that did not move down. There is no lost chunk to leave behind, so
		// both bars settle at once instead of crawling.
		StopHealthBarTicker();
		LaggedHealthHoldRemaining = 0.0f;
		UE_MVVM_SET_PROPERTY_VALUE(DisplayedHealthPercent, HealthPercent);
		UE_MVVM_SET_PROPERTY_VALUE(LaggedHealthPercent, HealthPercent);
		return;
	}

	// Damage. Restart the hold so a burst of hits keeps the chunk visible for a beat; the main
	// bar starts draining on the very next tick.
	LaggedHealthHoldRemaining = LaggedHealthHoldTime;
	StartHealthBarTicker();
}

bool UMALStatViewModel::TickHealthBars(float DeltaTime)
{
	bool bKeepTicking = false;

	// Main bar: fast exponential approach. It runs even while the trailing bar is still holding,
	// so the red bar has already settled by the time the grey one starts to move.
	const float Displayed = FMath::FInterpTo(DisplayedHealthPercent, HealthPercent, DeltaTime, DisplayedHealthCatchUpSpeed);
	if (FMath::IsNearlyEqual(Displayed, HealthPercent, 0.001f))
	{
		UE_MVVM_SET_PROPERTY_VALUE(DisplayedHealthPercent, HealthPercent);
	}
	else
	{
		UE_MVVM_SET_PROPERTY_VALUE(DisplayedHealthPercent, Displayed);
		bKeepTicking = true;
	}

	// Trailing bar: hold, then a slower approach.
	if (LaggedHealthHoldRemaining > 0.0f)
	{
		LaggedHealthHoldRemaining -= DeltaTime;
		bKeepTicking = true;
	}
	else
	{
		const float Lagged = FMath::FInterpTo(LaggedHealthPercent, HealthPercent, DeltaTime, LaggedHealthCatchUpSpeed);
		if (FMath::IsNearlyEqual(Lagged, HealthPercent, 0.002f))
		{
			// Snap: an exponential approach never quite arrives, and leaving the ticker alive
			// would cost a core tick every frame for the rest of the session.
			UE_MVVM_SET_PROPERTY_VALUE(LaggedHealthPercent, HealthPercent);
		}
		else
		{
			UE_MVVM_SET_PROPERTY_VALUE(LaggedHealthPercent, Lagged);
			bKeepTicking = true;
		}
	}

	return bKeepTicking;
}

void UMALStatViewModel::StartHealthBarTicker()
{
	if (HealthBarTicker.IsValid())
	{
		return;
	}

	// Weak lambda: the core ticker outlives the UObject and has no way to learn it was collected.
	HealthBarTicker = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateWeakLambda(this, [this](float DeltaTime)
		{
			return TickHealthBars(DeltaTime);
		}));
}

void UMALStatViewModel::StopHealthBarTicker()
{
	if (HealthBarTicker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(HealthBarTicker);
		HealthBarTicker.Reset();
	}
}

void UMALStatViewModel::RefreshManaPercent()
{
	UE_MVVM_SET_PROPERTY_VALUE(ManaPercent, ComputePercent(Mana, MaxMana));
}

void UMALStatViewModel::RefreshStaminaPercent()
{
	UE_MVVM_SET_PROPERTY_VALUE(StaminaPercent, ComputePercent(Stamina, MaxStamina));
}
