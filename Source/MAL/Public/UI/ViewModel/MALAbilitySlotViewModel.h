// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpec.h"
#include "GameplayTagContainer.h"
#include "UI/ViewModel/MALUIViewModelBase.h"
#include "MALAbilitySlotViewModel.generated.h"

class UTexture2D;

/** Native repaint signal, fired once per Refresh() so views never have to poll. */
DECLARE_MULTICAST_DELEGATE(FMALAbilitySlotRefreshed);

/**
 * UMALAbilitySlotViewModel
 *
 * One fixed key slot on the ability bar.
 *
 * A slot is keyed by input tag rather than by ability, because the bar is laid out as a fixed set
 * of keys and the ability behind a key changes as the player unlocks and upgrades it. An empty
 * slot is a normal state, not an error: a key with nothing granted still draws its frame so the
 * bar does not reflow the moment an ability is learned.
 *
 * Cooldown and cost state are polled from the ASC instead of listened to. A cooldown is a
 * duration effect that only surfaces to gameplay as a tag count and a remaining time, so a tag
 * event would still have to be paired with a timer to draw the sweep; polling both values on one
 * shared ticker is less machinery and cannot drift out of sync with the actual cooldown.
 */
UCLASS(BlueprintType)
class MAL_API UMALAbilitySlotViewModel : public UMALUIViewModelBase
{
	GENERATED_BODY()

public:
	/** Input tag this slot listens to. Set once by the bar, before the ASC is bound. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Ability")
	FGameplayTag InputTag;

	/** Ability's display name; empty while the slot has no ability. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Ability")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Ability")
	TObjectPtr<UTexture2D> Icon;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Ability")
	bool bHasAbility = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Ability")
	bool bIsOnCooldown = false;

	/** 1.0 the instant the cooldown starts, 0.0 once the ability is ready again. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Ability")
	float CooldownPercent = 0.0f;

	/** Seconds left, kept fractional so the readout can count down in tenths. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Ability")
	float CooldownRemaining = 0.0f;

	/** False when the ability's cost is more than the owner currently has. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Ability")
	bool bCanAfford = true;

	void SetInputTag(const FGameplayTag& InInputTag);

	/** Re-read the granted ability; call after an unlock or upgrade moved it to another class. */
	void RebindAbility();

	/** Pull cooldown and cost state from the ASC. Driven by the owning bar's ticker. */
	void Refresh();

	/** Subscribed by the slot widget; the bar drives it, the widget only listens. */
	FMALAbilitySlotRefreshed OnSlotRefreshed;

protected:
	//~ Begin UMALUIViewModelBase
	virtual void OnAbilitySystemReady(UAbilitySystemComponent* InAbilitySystemComponent) override;
	virtual void OnAbilitySystemReleased() override;
	//~ End UMALUIViewModelBase

private:
	/** Blank every field. Used when no ASC is bound, and when one is released. */
	void ClearSlot();

	/** Spec currently filling this slot; null while no ability is granted for InputTag. */
	FGameplayAbilitySpec* FindAbilitySpec() const;
};
