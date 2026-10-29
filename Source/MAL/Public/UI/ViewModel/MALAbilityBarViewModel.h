// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "GameplayTagContainer.h"
#include "UI/ViewModel/MALUIViewModelBase.h"
#include "MALAbilityBarViewModel.generated.h"

class UMALAbilitySlotViewModel;

/**
 * UMALAbilityBarViewModel
 *
 * The row of fixed key slots under the vitals.
 *
 * Owns one slot per input tag and one ticker for all of them: cooldown time is a property of the
 * owner, not of a slot, so refreshing four slots from a single tick is cheaper than four timers
 * and keeps every sweep in the same frame.
 *
 * The layout comes from input tags rather than from the ASC's granted abilities. Binding the bar
 * to whatever happens to be granted would make keys appear and disappear as abilities are
 * unlocked; a fixed key set keeps muscle memory stable and lets an empty slot render as such.
 */
UCLASS(BlueprintType)
class MAL_API UMALAbilityBarViewModel : public UMALUIViewModelBase
{
	GENERATED_BODY()

public:
	/** Slots in draw order; the order follows the tags handed to InitializeSlots. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Ability")
	TArray<TObjectPtr<UMALAbilitySlotViewModel>> Slots;

	/**
	 * Build one slot per input tag.
	 *
	 * Safe to call before the ASC exists: slots resolve their ability as soon as one is bound.
	 * Calling it again rebuilds the bar, which is what a class or loadout change would do.
	 */
	UFUNCTION(BlueprintCallable, Category = "MAL|UI|Ability")
	void InitializeSlots(const TArray<FGameplayTag>& SlotInputTags);

	/** Re-resolve every slot's ability, e.g. after a skill tree unlock granted or upgraded one. */
	UFUNCTION(BlueprintCallable, Category = "MAL|UI|Ability")
	void RefreshSlots();

protected:
	/**
	 * How often cooldown state is sampled.
	 *
	 * The sweep reads the remaining time itself, so this only sets how often the numbers and the
	 * cost check are refreshed. It runs per frame so the tenths counter and the progress bar move
	 * continuously; a slot rebuilds its label only when the digit actually changes, so the extra
	 * ticks cost a pair of ASC lookups each and nothing more.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Ability", meta = (ClampMin = "0.01"))
	float CooldownRefreshInterval = 1.0f / 60.0f;

	//~ Begin UMALUIViewModelBase
	virtual void OnAbilitySystemReady(UAbilitySystemComponent* InAbilitySystemComponent) override;
	virtual void OnAbilitySystemReleased() override;
	//~ End UMALUIViewModelBase

private:
	void ApplyAbilitySystemToSlots(UAbilitySystemComponent* InAbilitySystemComponent);

	/** Resets the refresh cadence and arms the ticker that sweeps every slot. */
	void StartCooldownTicker();

	/** Advances the refresh cadence; always returns true so the ticker survives idle gaps. */
	bool TickCooldowns(float DeltaTime);

	float RefreshAccumulator = 0.0f;
};
