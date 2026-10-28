// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/MALSkillTreeDefinition.h"
#include "Templates/Function.h"
#include "UI/Widgets/MALUserWidget.h"
#include "MALSkillNodeWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UWidget;
class UMALSkillNodeViewModel;

/**
 * UMALSkillNodeWidget
 *
 * One tile of the skill grid: icon, name, cost and a state overlay.
 *
 * The row repaints itself from its view model instead of expecting a view binding, because it
 * has to light up four different things from one state change -- the two overlays, the fade,
 * the label and the selection frame -- and a view binding targets one property on one widget at
 * a time. Named child widgets and one refresh function also mean the blueprint only has to
 * name its widgets correctly; there is no second place to wire the source.
 *
 * Every part is optional. A tile authored with just an icon and a name still builds, and the
 * refresh skips whatever the blueprint left out.
 *
 * The tile never unlocks anything. Its button reports the click upward and the menu decides,
 * because the menu is the object that also feeds the detail column.
 */
UCLASS(Abstract)
class MAL_API UMALSkillNodeWidget : public UMALUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * Point the tile at one node.
	 *
	 * Returns false when the view model is null. Unlike a view binding this cannot fail silently:
	 * the tile repaints from the model directly, so a null one simply leaves the tile blank.
	 */
	bool SetNodeViewModel(UMALSkillNodeViewModel* InViewModel);

	/** The node behind this tile, or null before the menu built it. */
	UFUNCTION(BlueprintPure, Category = "MAL|UI|Skill")
	UMALSkillNodeViewModel* GetNodeViewModel() const { return NodeViewModel; }

	/**
	 * Re-read the view model into this tile's widgets.
	 *
	 * Called by the menu after every refresh, and again on construct. Public so a blueprint can
	 * force a repaint when it changes something the state does not cover.
	 */
	UFUNCTION(BlueprintCallable, Category = "MAL|UI|Skill")
	void RefreshFromNodeViewModel();

	/** Raised when the tile is clicked. The menu routes it to its own SelectNode. */
	TFunction<void(UMALSkillNodeViewModel*)> SelectionRequested;

protected:
	/**
	 * How far the tile fades while it sits in that state. An owned tile always renders at 1.
	 *
	 * Two numbers rather than one because the two unowned states are meant to read at different
	 * weights: a locked tile already carries a lock over its icon, so it can sit back further
	 * than a reachable one, which should only lose enough contrast to read as not yet lit.
	 * Tuned on the blueprint's defaults; 1.0 on either switches the fade off for that state.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|UI|Skill", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LockedOpacity = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|UI|Skill", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AvailableOpacity = 0.6f;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> IconImage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	/** "3" or "3 pts" -- the widget prints the number only, so a unit belongs in the blueprint. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CostText;

	/** Locked / Available / Unlocked, from the view model's ready-made label. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StateText;

	/**
	 * Drawn over the icon while the node is locked.
	 *
	 * Declared as a plain UWidget so a Border, an Image or a whole Overlay can fill the slot; the
	 * tile only toggles its visibility.
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> LockedOverlay;

	/**
	 * Drawn while the node is owned -- a glow, a coloured frame, a checkmark.
	 *
	 * The mirror of LockedOverlay, and available is the one state with neither: losing the lock
	 * while staying faded is the whole message, so it needs no third widget to say it.
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> UnlockedOverlay;

	/** Outline shown while this tile is the one feeding the detail column. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SelectionFrame;

	/** Whole-tile click target. A locked tile still selects: the panel then says why. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> SelectButton;

	//~ Begin UUserWidget
	virtual void NativeConstruct() override;
	//~ End UUserWidget

	/**
	 * Hook for anything the three bound widgets cannot express: rarity borders, glow, a
	 * play-forward animation on unlock. Called at the end of every refresh.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "MAL|UI|Skill", meta = (DisplayName = "On Node Refreshed"))
	void OnNodeRefreshed(EMALSkillNodeState State, bool bCanUnlock);

private:
	UFUNCTION()
	void HandleSelectClicked();

	UPROPERTY(Transient)
	TObjectPtr<UMALSkillNodeViewModel> NodeViewModel;
};
