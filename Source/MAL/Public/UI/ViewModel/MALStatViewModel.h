// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "UI/ViewModel/MALUIViewModelBase.h"
#include "MALStatViewModel.generated.h"

/**
 * UMALStatViewModel
 *
 * Presentation copy of the vitals the HUD and nameplates display.
 *
 * Every FieldNotify property is computed here, never in the widget: percentages are clamped
 * once, so a progress bar can bind straight to HealthPercent without doing math in UMG.
 *
 * The view model is deliberately ASC-agnostic. Point it at a player ASC for the HUD, or at an
 * enemy ASC for a world-space nameplate, and the same FieldNotify properties drive both.
 */
UCLASS(BlueprintType)
class MAL_API UMALStatViewModel : public UMALUIViewModelBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Stat")
	float Health = 0.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Stat")
	float MaxHealth = 0.0f;

	/** Pre-clamped 0..1 ratio; bind a ProgressBar percent here instead of dividing in UMG. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Stat")
	float HealthPercent = 0.0f;

	/**
	 * Smoothed health ratio for the main bar.
	 *
	 * HealthPercent jumps the instant a GameplayEffect lands. Binding the main bar to this value
	 * instead makes it drain over a fraction of a second, so a hit reads as damage taken rather
	 * than as a teleport. Healing settles immediately, matching the trailing bar.
	 */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Stat")
	float DisplayedHealthPercent = 0.0f;

	/**
	 * Trailing bar.
	 *
	 * Mirrors HealthPercent instantly when healing, but on damage it stays where it was for a
	 * short beat and then drains down. Bind a second ProgressBar - drawn behind the red one and
	 * filled with a muted colour - to this property: the overlap is hidden by the red bar, so only
	 * the freshly lost chunk shows through.
	 */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Stat")
	float LaggedHealthPercent = 0.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Stat")
	bool bIsDead = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Stat")
	float Mana = 0.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Stat")
	float MaxMana = 0.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Stat")
	float ManaPercent = 0.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Stat")
	float Stamina = 0.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Stat")
	float MaxStamina = 0.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Stat")
	float StaminaPercent = 0.0f;

protected:
	/**
	 * Interp speed for the main bar. Exponential, so the first part of the loss shows immediately
	 * and the last few percent settle quickly; 12.0 reads as a fast drain rather than a snap.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Stat", meta = (ClampMin = "0.1"))
	float DisplayedHealthCatchUpSpeed = 12.0f;

	/** How long the trailing bar holds still after a hit before it starts draining. */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Stat", meta = (ClampMin = "0.0"))
	float LaggedHealthHoldTime = 0.35f;

	/**
	 * Interp speed for the catch-up. Exponential, so the first part of the gap drains fast and the
	 * tail slows down; 4.0 empties a full bar in roughly a second.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Stat", meta = (ClampMin = "0.1"))
	float LaggedHealthCatchUpSpeed = 4.0f;

	//~ Begin UMALUIViewModelBase
	virtual void OnAbilitySystemReady(UAbilitySystemComponent* InAbilitySystemComponent) override;
	virtual void OnAbilitySystemReleased() override;
	//~ End UMALUIViewModelBase

private:
	void RefreshHealthPercent();
	void RefreshManaPercent();
	void RefreshStaminaPercent();

	/**
	 * React to a new HealthPercent: settle immediately, or start the two-stage drain where the
	 * main bar drops fast and the trailing bar follows slowly.
	 */
	void UpdateHealthBars();

	void StartHealthBarTicker();
	void StopHealthBarTicker();

	/** Advances both bars; returns false once each has reached its target. */
	bool TickHealthBars(float DeltaTime);

	/** Time left before the trailing bar starts moving; a new hit refills it. */
	float LaggedHealthHoldRemaining = 0.0f;

	/** False until the first value arrives, so binding mid-fight does not animate from zero. */
	bool bHealthBarsInitialized = false;

	FTSTicker::FDelegateHandle HealthBarTicker;
};
