// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UI/Widgets/MALMenuWidget.h"
#include "MALSkillMenuWidget.generated.h"

class UButton;
class UImage;
class UScrollBox;
class UTextBlock;
class UMALSkillMenuViewModel;
class UMALSkillNodeViewModel;
class UMALSkillNodeWidget;

/**
 * UMALSkillMenuWidget
 *
 * The skill panel: the whole class pool down the left, the selected node's detail on the right.
 *
 * Rows are created at runtime from NodeWidgetClass, one per node of the player's tree, and the
 * list arrives already sorted by tier, so a new band begins wherever the tier changes. The band
 * heading itself is left to the blueprint -- CreateTierHeader returns whatever widget should
 * sit above the band, including nothing -- because what a heading looks like is not something
 * C++ can decide.
 *
 * The panel is built when it is shown rather than when the HUD starts, so it reads the player
 * state once, at a point where the player state is guaranteed to exist. Every later change
 * arrives through the view model.
 *
 * Must not pause the game, for the same reason the attribute panel does not: a paused world
 * stops ticking enhanced input, and the key that opened the panel would never arrive to close
 * it again.
 */
UCLASS(Abstract)
class MAL_API UMALSkillMenuWidget : public UMALMenuWidget
{
	GENERATED_BODY()

public:
	UMALSkillMenuWidget();

	/** Feed the detail column. Routed here by the rows. */
	UFUNCTION(BlueprintCallable, Category = "MAL|UI|Skill")
	void SelectNode(UMALSkillNodeViewModel* Node);

	/** Ask the authority to buy the selected node. Bound to the detail column's button. */
	UFUNCTION(BlueprintCallable, Category = "MAL|UI|Skill")
	void RequestUnlock();

	/** The panel's view model, or null before the panel was built. */
	UFUNCTION(BlueprintPure, Category = "MAL|UI|Skill")
	UMALSkillMenuViewModel* GetSkillMenuViewModel() const { return MenuViewModel; }

protected:
	/**
	 * Tile widget created once per node.
	 *
	 * Null leaves the list empty and logs which property to fill in.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Skill")
	TSubclassOf<UMALSkillNodeWidget> NodeWidgetClass;

	/** Gap under every tile, in slate units. */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Skill", meta = (ClampMin = "0.0"))
	float RowSpacing = 6.0f;

	/** Extra gap above a new tier band. */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Skill", meta = (ClampMin = "0.0"))
	float TierSpacing = 12.0f;

	/** Container the tiles are added to, top to bottom. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> NodesBox;

	/** Points still available to spend. A missing block only costs the number. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SkillPointsText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SelectedNameText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SelectedDescriptionText;

	/** Locked / Available / Unlocked for the selected node. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SelectedStateText;

	/** Why the selected node cannot be bought, or empty when it can. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SelectedRequirementText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> SelectedIconImage;

	/** Buys the selected node. Disabled whenever the view model says it cannot be bought. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> UnlockButton;

	//~ Begin UUserWidget
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End UUserWidget

	/**
	 * Build the heading that opens a tier band, or return null to draw the band without one.
	 *
	 * A blueprint event rather than a widget class property: a heading is a label, a divider, a
	 * frame or nothing at all depending on the design, and none of those accept a title through
	 * a common interface.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "MAL|UI|Skill", meta = (DisplayName = "Create Tier Header"))
	UWidget* CreateTierHeader(int32 Tier);
	virtual UWidget* CreateTierHeader_Implementation(int32 Tier);

	/** Re-read the selected node, the point count and the button state. */
	void RefreshDetails();

private:
	/** Create the view model and the rows. Returns false when the blueprint is not set up enough. */
	bool BuildPanel();

	/** Create one tile per node, opening a band wherever the tier changes. */
	bool BuildNodeRows();

	/** Re-read every tile and the detail column. Bound to the view model's refresh. */
	void HandleMenuRefreshed();

	/** Route the unlock request out through the controller, which owns the RPC. */
	void HandleUnlockRequested(FGameplayTag SkillTag);

	UPROPERTY(Transient)
	TObjectPtr<UMALSkillMenuViewModel> MenuViewModel;

	/** Tiles in draw order, so a refresh never has to walk the panel tree. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMALSkillNodeWidget>> NodeRows;

	FDelegateHandle MenuRefreshedHandle;

	/** Set once the panel exists. Construct runs again on every add-to-viewport. */
	bool bPanelBuilt = false;
};
