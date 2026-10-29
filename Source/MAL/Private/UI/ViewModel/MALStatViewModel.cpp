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

	/**
	 * Advance one bar pair by a frame.
	 *
	 * The main layer approaches the target fast while the trailing layer waits out its hold and
	 * then follows slowly, which is what makes a drop read as "taken" instead of as a teleport.
	 * Pure math on the state; the caller decides how the result reaches FieldNotify.
	 *
	 * Returns true while either layer still has catching up to do.
	 */
	bool TickBarPair(FMALBarSmoothingState& State,
		float Target,
		float DisplayedSpeed,
		float LaggedSpeed,
		float DeltaTime)
	{
		bool bKeepTicking = false;

		// Main bar: fast exponential approach. It runs even while the trailing bar is still
		// holding, so the leading edge has already settled by the time the tail starts to move.
		const float Displayed = FMath::FInterpTo(State.Displayed, Target, DeltaTime, DisplayedSpeed);
		if (FMath::IsNearlyEqual(Displayed, Target, 0.001f))
		{
			State.Displayed = Target;
		}
		else
		{
			State.Displayed = Displayed;
			bKeepTicking = true;
		}

		// Trailing bar: hold, then a slower approach.
		if (State.HoldRemaining > 0.0f)
		{
			State.HoldRemaining -= DeltaTime;
			bKeepTicking = true;
		}
		else
		{
			const float Lagged = FMath::FInterpTo(State.Lagged, Target, DeltaTime, LaggedSpeed);
			if (FMath::IsNearlyEqual(Lagged, Target, 0.002f))
			{
				// Snap: an exponential approach never quite arrives, and leaving the ticker alive
				// would cost a core tick every frame for the rest of the session.
				State.Lagged = Target;
			}
			else
			{
				State.Lagged = Lagged;
				bKeepTicking = true;
			}
		}

		return bKeepTicking;
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
	StopTicker();
	HealthBar = FMALBarSmoothingState();
	ManaBar = FMALBarSmoothingState();

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
	UE_MVVM_SET_PROPERTY_VALUE(DisplayedManaPercent, 0.0f);
	UE_MVVM_SET_PROPERTY_VALUE(LaggedManaPercent, 0.0f);
	UE_MVVM_SET_PROPERTY_VALUE(Stamina, 0.0f);
	UE_MVVM_SET_PROPERTY_VALUE(MaxStamina, 0.0f);
	UE_MVVM_SET_PROPERTY_VALUE(StaminaPercent, 0.0f);
}

void UMALStatViewModel::RefreshHealthPercent()
{
	UE_MVVM_SET_PROPERTY_VALUE(HealthPercent, ComputePercent(Health, MaxHealth));
	UE_MVVM_SET_PROPERTY_VALUE(bIsDead, Health <= 0.0f && MaxHealth > 0.0f);

	if (RefreshBarPair(HealthBar, HealthPercent, LaggedHealthHoldTime))
	{
		StartBarTicker();
	}

	PushHealthBars();
}

void UMALStatViewModel::RefreshManaPercent()
{
	UE_MVVM_SET_PROPERTY_VALUE(ManaPercent, ComputePercent(Mana, MaxMana));

	if (RefreshBarPair(ManaBar, ManaPercent, LaggedManaHoldTime))
	{
		StartBarTicker();
	}

	PushManaBars();
}

void UMALStatViewModel::RefreshStaminaPercent()
{
	UE_MVVM_SET_PROPERTY_VALUE(StaminaPercent, ComputePercent(Stamina, MaxStamina));
}

bool UMALStatViewModel::RefreshBarPair(FMALBarSmoothingState& State, float Target, float HoldTime)
{
	if (!State.bInitialized)
	{
		// First value after binding. Start already settled: otherwise creating the HUD would play
		// a full-to-current drain before the player has been touched.
		State.bInitialized = true;
		State.Displayed = Target;
		State.Lagged = Target;
		State.HoldRemaining = 0.0f;
		return false;
	}

	if (Target >= State.Lagged)
	{
		// Healing, a refill, or a value that did not move down. There is no lost chunk to leave
		// behind, so both layers settle at once instead of crawling.
		State.Displayed = Target;
		State.Lagged = Target;
		State.HoldRemaining = 0.0f;
		return false;
	}

	// Drop. Restart the hold so a burst of hits keeps the chunk visible for a beat; the main bar
	// starts draining on the very next tick regardless.
	State.HoldRemaining = HoldTime;
	return true;
}

void UMALStatViewModel::PushHealthBars()
{
	UE_MVVM_SET_PROPERTY_VALUE(DisplayedHealthPercent, HealthBar.Displayed);
	UE_MVVM_SET_PROPERTY_VALUE(LaggedHealthPercent, HealthBar.Lagged);
}

void UMALStatViewModel::PushManaBars()
{
	UE_MVVM_SET_PROPERTY_VALUE(DisplayedManaPercent, ManaBar.Displayed);
	UE_MVVM_SET_PROPERTY_VALUE(LaggedManaPercent, ManaBar.Lagged);
}

bool UMALStatViewModel::TickBars(float DeltaTime)
{
	bool bKeepTicking = false;

	// Both pairs advance every frame they are still running; the ticker only stops once neither
	// has anything left to move. Written as explicit ifs because a bitwise |= on two bools trips
	// -Wbitwise-instead-of-logical, which this project builds with as an error.
	if (TickBarPair(HealthBar, HealthPercent,
		DisplayedHealthCatchUpSpeed, LaggedHealthCatchUpSpeed, DeltaTime))
	{
		bKeepTicking = true;
	}
	PushHealthBars();

	if (TickBarPair(ManaBar, ManaPercent,
		DisplayedManaCatchUpSpeed, LaggedManaCatchUpSpeed, DeltaTime))
	{
		bKeepTicking = true;
	}
	PushManaBars();

	return bKeepTicking;
}

void UMALStatViewModel::StartBarTicker()
{
	StartTicker([this](float DeltaTime)
	{
		return TickBars(DeltaTime);
	});
}
