// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Templates/Function.h"
#include "UI/ViewModel/MALUIViewModelBase.h"
#include "MALSkillMenuViewModel.generated.h"

class AMALPlayerState;
class UMALProgressionComponent;
class UMALSkillNodeViewModel;

/** Fired after the panel's visible content was recomputed. Bound by the menu widget. */
DECLARE_MULTICAST_DELEGATE(FMALSkillMenuRefreshedSignature);

/**
 * UMALSkillMenuViewModel
 *
 * The whole skill panel: every node of the class's pool, the points left, and which node the
 * detail column is showing.
 *
 * Owns one node view model per entry of UMALSkillTreeDefinition::Nodes. That is the opposite of
 * what the attribute menu does -- one view model per row, created by the widget -- because the
 * tree is a fixed authored list rather than an enum the widget can enumerate. Building them
 * here means the widget only has to place what it is given, and the state rules stay in one
 * class instead of being split between a view model and a builder loop.
 *
 * RefreshAll is the only place node states are computed. Everything that can change an answer
 * -- the level attribute, the skill point attribute, the unlocked set -- funnels into it, so
 * there is no path where one node disagrees with another about the current level.
 *
 * The class never sends anything. RequestUnlock stops at UnlockRequested, and the widget routes
 * that to the player controller: a view model has no controller, and on a client it cannot
 * reach one through the player state either, whose owner does not replicate.
 */
UCLASS(BlueprintType)
class MAL_API UMALSkillMenuViewModel : public UMALUIViewModelBase
{
	GENERATED_BODY()

public:
	/**
	 * Every node of the player's class tree, in draw order: ascending Tier, and inside a tier
	 * the order the tree asset lists them in.
	 *
	 * Sorted here rather than in the widget, because the tier bands are only meaningful relative
	 * to each other and every consumer wants the same answer. A widget can therefore start a new
	 * section whenever GetTier changes and never has to sort.
	 */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Skill")
	TArray<TObjectPtr<UMALSkillNodeViewModel>> Nodes;

	/** Skill points earned and not yet spent. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Skill")
	int32 SkillPoints = 0;

	/** Node the detail column is showing, or null before anything was picked. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Skill")
	TObjectPtr<UMALSkillNodeViewModel> SelectedNode;

	/** Whether the selected node can be bought right now. Bind the unlock button's enabled state. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Skill")
	bool bCanUnlockSelected = false;

	/**
	 * Point the panel at a player and build its rows.
	 *
	 * Safe to call again after a respawn: the node set is rebuilt from scratch and the selection
	 * is dropped, because the previous view models belong to a state that no longer exists.
	 */
	UFUNCTION(BlueprintCallable, Category = "MAL|UI|Skill")
	void InitializeFromPlayerState(AMALPlayerState* InPlayerState);

	/** Recompute every node against the current level, points and unlocked set. */
	UFUNCTION(BlueprintCallable, Category = "MAL|UI|Skill")
	void RefreshAll();

	/** Feed the detail column. Null clears it. */
	UFUNCTION(BlueprintCallable, Category = "MAL|UI|Skill")
	void SelectNode(UMALSkillNodeViewModel* Node);

	/**
	 * Ask for the selected node to be bought.
	 *
	 * Does nothing unless the node is unlockable this instant, so a stale button press cannot
	 * spend points that went to another node in the meantime. The state is not changed here: the
	 * authority charges the points, grants the ability and broadcasts, and the resulting refresh
	 * is what moves the node.
	 */
	UFUNCTION(BlueprintCallable, Category = "MAL|UI|Skill")
	void RequestUnlock();

	/** Set by the widget before the panel is shown; null leaves the unlock button inert. */
	TFunction<void(FGameplayTag)> UnlockRequested;

	/**
	 * Fired at the end of RefreshAll and SelectNode.
	 *
	 * A native delegate rather than FieldNotify listeners: the cheap C++ path into a FieldNotify
	 * property is a blueprint view binding, and the widget here has to repaint several child
	 * widgets from one event, which is exactly what a delegate does and FieldNotify cannot.
	 */
	FMALSkillMenuRefreshedSignature OnMenuRefreshed;

protected:
	//~ Begin UMALUIViewModelBase
	virtual void OnAbilitySystemReady(UAbilitySystemComponent* InAbilitySystemComponent) override;
	virtual void OnAbilitySystemReleased() override;
	//~ End UMALUIViewModelBase

private:
	/** Build one node view model per tree entry and sort them into tier order. */
	void BuildNodes();

	/** Listen to the progression component's own change notification. Safe to call repeatedly. */
	void BindProgression();

	void UnbindProgression();

	/** Recompute bCanUnlockSelected from the current selection. */
	void RefreshSelection();

	/** Native delegate handler, not a UFUNCTION: nothing here is driven by reflection. */
	void HandleSkillsChanged();

	/** Player state the nodes are judged against. Weak: the panel outlives neither one. */
	TWeakObjectPtr<AMALPlayerState> CachedPlayerState;

	/** Component currently subscribed to, so a re-init can unsubscribe from the old one. */
	TWeakObjectPtr<UMALProgressionComponent> BoundProgression;

	FDelegateHandle SkillsChangedHandle;
};
