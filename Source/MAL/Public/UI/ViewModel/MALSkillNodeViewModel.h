// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/MALSkillTreeDefinition.h"
#include "GameplayTagContainer.h"
#include "UI/ViewModel/MALUIViewModelBase.h"
#include "MALSkillNodeViewModel.generated.h"

class UTexture2D;

/**
 * UMALSkillNodeViewModel
 *
 * One node of the skill tree, as the menu draws it.
 *
 * The class does not look anything up. Its authored half is a snapshot of
 * FMALSkillNodeDefinition taken once at Initialize, and its live half -- the state, the reason
 * and whether the button works -- is pushed in by UMALSkillMenuViewModel, which is the only
 * object here holding a player state. Splitting it that way is what allows one node view model
 * per node while a single instance per row widget keeps its own data.
 *
 * Display data falls back to the ability's class default object when the node leaves it blank,
 * so a tree an author filled in with only tags and classes still reads properly. Description
 * has no fallback: an ability carries none.
 */
UCLASS(BlueprintType)
class MAL_API UMALSkillNodeViewModel : public UMALUIViewModelBase
{
	GENERATED_BODY()

public:
	/**
	 * Snapshot the node's authored data.
	 *
	 * Called once, before the view model is handed to a widget. Icon is resolved here rather
	 * than lazily on bind: four icons on first open is one frame of loading, while a lazy
	 * resolve would put the same cost on whichever frame the player happens to scroll into.
	 */
	void Initialize(const FMALSkillNodeDefinition& InNode);

	/**
	 * Re-evaluate the live half. Called by the menu on every refresh.
	 *
	 * InReason is the text CanUnlockSkill produced: empty when the node can be bought, a
	 * sentence naming the missing requirement otherwise.
	 */
	void ApplyState(EMALSkillNodeState InState, const FText& InReason);

	/** Mark whether the detail panel is showing this node. */
	void SetSelected(bool bInSelected);

	UFUNCTION(BlueprintPure, Category = "MAL|UI|Skill")
	FGameplayTag GetSkillTag() const { return SkillTag; }

	/** Tier band this node is drawn in. Fixed at Initialize; the menu sorts rows by it. */
	UFUNCTION(BlueprintPure, Category = "MAL|UI|Skill")
	int32 GetTier() const { return Tier; }

	/** True when the node is reachable and affordable right now, i.e. the unlock button works. */
	UFUNCTION(BlueprintPure, Category = "MAL|UI|Skill")
	bool CanBeUnlocked() const { return bCanUnlock; }

	/** Stable identity of the node; the unlock request and the save file both carry it. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Skill")
	FGameplayTag SkillTag;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Skill")
	FText DisplayName;

	/** Detail-panel body text. Empty unless the node defines one. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Skill")
	FText Description;

	/**
	 * Resolved icon, or null when neither the node nor its ability supplies one.
	 *
	 * A hard pointer rather than the soft one the definition holds: a view binding cannot
	 * resolve a TSoftObjectPtr into a brush, and the node widget would then have to fetch it
	 * itself.
	 */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Skill")
	TObjectPtr<UTexture2D> Icon;

	/** Where the node stands. Drives the greyed overlay and the unlock button. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Skill")
	EMALSkillNodeState NodeState = EMALSkillNodeState::Locked;

	/** Points charged on unlock. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Skill")
	int32 Cost = 1;

	/** Character level the node needs. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Skill")
	int32 RequiredLevel = 1;

	/**
	 * Why the node cannot be bought, or empty when it can.
	 *
	 * Kept separate from NodeState so a node that is merely unaffordable still reads as
	 * reachable: it is shown lit, with this line underneath explaining the wait.
	 */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Skill")
	FText RequirementText;

	/** Reachable and affordable. The unlock button binds its enabled state here. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Skill")
	bool bCanUnlock = false;

	/** True while this node feeds the detail panel. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Skill")
	bool bIsSelected = false;

	/** Ready-made label for NodeState, so a blueprint needs no enum switch of its own. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Skill")
	FText StateText;

private:
	/** Tier band, cached from the definition. */
	int32 Tier = 0;

	/** Rewrite StateText from NodeState. */
	void RefreshStateText();
};
