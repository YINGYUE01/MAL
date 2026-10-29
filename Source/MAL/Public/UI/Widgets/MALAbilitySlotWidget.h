// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/MALUserWidget.h"
#include "MALAbilitySlotWidget.generated.h"

class UImage;
class UProgressBar;
class UTextBlock;
class UTexture2D;
class UMALAbilitySlotViewModel;

/**
 * UMALAbilitySlotWidget
 *
 * One drawn key of the ability bar.
 *
 * The bar owns its slot view models and creates them at runtime, so a slot cannot be handed one
 * through a source wired in the widget blueprint: it has to claim the entry at its own index.
 * Resolving that here rather than from the HUD keeps the slot usable anywhere that can reach the
 * UI subsystem, and lets the HUD stay a plain container.
 *
 * Place one instance per slot and set SlotIndex; nothing else in the blueprint has to change when
 * abilities are unlocked or the bar is rebuilt.
 */
UCLASS(Abstract)
class MAL_API UMALAbilitySlotWidget : public UMALUserWidget
{
	GENERATED_BODY()

public:
	/** Index into the bar's Slots array, in draw order. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MAL|UI|Ability")
	int32 SlotIndex = 0;

	/**
	 * Icon drawn in the slot.
	 *
	 * Set this from the HUD to skin a key without going through an ability asset. When it is
	 * empty the icon comes from the granted ability, so a finished asset needs no wiring here.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MAL|UI|Ability|Appearance")
	TObjectPtr<UTexture2D> IconOverride;

	/**
	 * Artwork drawn while the slot has no ability to describe itself.
	 *
	 * A key starts out naming a skill that is not unlocked yet, so a placeholder keeps the bar
	 * readable; a granted ability carrying an icon of its own takes over from it. This is what
	 * makes the bar follow the equipped weapon: the same key shows the placeholder again as soon
	 * as the new kit stops granting a skill there.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MAL|UI|Ability|Appearance")
	TObjectPtr<UTexture2D> DefaultIcon;

	/** Tint for DefaultIcon. Dimmer than IconTint so a key without a skill does not read as ready. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MAL|UI|Ability|Appearance")
	FLinearColor DefaultIconTint = FLinearColor(0.55f, 0.55f, 0.55f, 1.0f);

	/** Label under the icon; empty falls back to the ability's own display name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MAL|UI|Ability|Appearance")
	FText DisplayNameOverride;

	/** Tint applied to the icon whenever the slot has artwork to draw. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MAL|UI|Ability|Appearance")
	FLinearColor IconTint = FLinearColor::White;

	/** Replaces the icon at runtime, for loadouts and anything else that swaps a key's artwork. */
	UFUNCTION(BlueprintCallable, Category = "MAL|UI|Ability|Appearance")
	void SetSlotIcon(UTexture2D* InIcon);

	/** Replaces the label at runtime; same contract as SetSlotIcon. */
	UFUNCTION(BlueprintCallable, Category = "MAL|UI|Ability|Appearance")
	void SetSlotDisplayName(const FText& InName);

	/** The view model behind this slot, or null until the HUD has been created. */
	UFUNCTION(BlueprintPure, Category = "MAL|UI|Ability")
	UMALAbilitySlotViewModel* GetSlotViewModel() const;

	/** The icon widget itself, for blueprints that would rather drive its brush directly. */
	UFUNCTION(BlueprintPure, Category = "MAL|UI|Ability|Appearance")
	UImage* GetIconImage() const;

protected:
	/**
	 * Slot parts, all optional.
	 *
	 * A minimal slot blueprint that only wants a frame still compiles, and the repaint below
	 * skips whatever the blueprint left out instead of demanding a fixed widget tree.
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> FrameImage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> IconImage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> CooldownOverlay;

	/** Optional thin bar that drains as the cooldown runs out. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> CooldownProgress;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CooldownText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	/** Retry interval while the HUD, and with it the bar view model, is still being created. */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Ability", meta = (ClampMin = "0.02"))
	float ResolveRetryInterval = 0.1f;

	/**
	 * Fired after every default repaint.
	 *
	 * Slot blueprints override this for artwork the base repaint does not know about. It runs
	 * last, so overrides win; anything left alone keeps tracking the view model.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "MAL|UI|Ability")
	void OnSlotUpdated();

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void TryResolveSlotViewModel();
	void StopResolveRetryTimer();
	void ReleaseSlotViewModel();

	/** Repaint from the slot's current values. Driven by the view model, not by a widget tick. */
	void RefreshFromViewModel();

	UPROPERTY(Transient)
	TObjectPtr<UMALAbilitySlotViewModel> SlotViewModel;

	/** Icon currently on the widget; guards against re-uploading the brush on every tick. */
	TWeakObjectPtr<UTexture2D> LastIcon;

	/** Countdown string currently on the widget; rebuilt only when the digit changes. */
	FText LastCooldownLabel;

	FTimerHandle ResolveRetryTimer;
};
