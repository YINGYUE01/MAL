// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "UI/ViewModel/MALUIViewModelBase.h"
#include "MALStatViewModel.generated.h"

/**
 * FMALBarSmoothingState
 *
 * Per-channel runtime state for a two-layer bar (main + trailing).
 *
 * Plain C++ rather than UPROPERTY: it changes every frame while a bar drains and nothing outside
 * this view model ever reads it. One instance lives per vitals channel, so health and mana share
 * a single ticker instead of each owning one.
 */
struct FMALBarSmoothingState
{
	/** Fast-approach ratio bound to the main bar. */
	float Displayed = 0.0f;

	/** Slow-approach ratio bound to the trailing bar. */
	float Lagged = 0.0f;

	/** Seconds left before the trailing bar starts moving; refreshed by every new drop. */
	float HoldRemaining = 0.0f;

	/** False until the first value arrives, so binding mid-fight does not animate from zero. */
	bool bInitialized = false;
};

/**
 * UMALStatViewModel
 *
 * Presentation copy of the vitals the HUD and nameplates display.
 *
 * Every FieldNotify property is computed here, never in the widget: percentages are clamped
 * once, so a progress bar can bind straight to HealthPercent without doing math in UMG.
 *
 * Health and mana each expose three ratios - the raw value plus a fast and a slow copy - so a
 * bar can be paired with a trailing bar that drains behind it. The view model is deliberately
 * ASC-agnostic. Point it at a player ASC for the HUD, or at an enemy ASC for a world-space
 * nameplate, and the same FieldNotify properties drive both.
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

	/**
	 * Smoothed mana ratio for the main bar.
	 *
	 * Same two-stage drain as health. Binding ManaPercent straight to the bar would make it
	 * teleport the instant a spell is paid for, which reads as a UI glitch next to a smoothing
	 * health bar.
	 */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Stat")
	float DisplayedManaPercent = 0.0f;

	/**
	 * Trailing mana bar; same contract as LaggedHealthPercent.
	 *
	 * Bind a ProgressBar drawn behind the blue one and filled with a muted colour: it holds for a
	 * beat after a cast, then drains down, leaving the freshly spent chunk visible.
	 */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Stat")
	float LaggedManaPercent = 0.0f;

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

	/** Main-bar interp speed for mana; same meaning as DisplayedHealthCatchUpSpeed. */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Stat", meta = (ClampMin = "0.1"))
	float DisplayedManaCatchUpSpeed = 12.0f;

	/** How long the trailing mana bar holds still after a cast before it starts draining. */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Stat", meta = (ClampMin = "0.0"))
	float LaggedManaHoldTime = 0.35f;

	/** Trailing-bar interp speed for mana; same meaning as LaggedHealthCatchUpSpeed. */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Stat", meta = (ClampMin = "0.1"))
	float LaggedManaCatchUpSpeed = 4.0f;

	//~ Begin UMALUIViewModelBase
	virtual void OnAbilitySystemReady(UAbilitySystemComponent* InAbilitySystemComponent) override;
	virtual void OnAbilitySystemReleased() override;
	//~ End UMALUIViewModelBase

private:
	void RefreshHealthPercent();
	void RefreshManaPercent();
	void RefreshStaminaPercent();

	/**
	 * Record a new target for one bar pair.
	 *
	 * Settles both layers immediately on the first value and on every increase - there is no lost
	 * chunk to leave behind when healing or refilling. Returns true when a drop has to be animated,
	 * which is the caller's cue to start the shared ticker.
	 */
	bool RefreshBarPair(FMALBarSmoothingState& State, float Target, float HoldTime);

	/** Publish one pair's current values to its FieldNotify properties. */
	void PushHealthBars();
	void PushManaBars();

	/** Arms the shared ticker that advances both pairs. */
	void StartBarTicker();

	/** Advances both bar pairs; returns false once every layer has reached its target. */
	bool TickBars(float DeltaTime);

	/** Runtime state of the health bar pair. */
	FMALBarSmoothingState HealthBar;

	/** Runtime state of the mana bar pair. */
	FMALBarSmoothingState ManaBar;
};
