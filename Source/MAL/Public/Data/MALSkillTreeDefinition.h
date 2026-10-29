// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "MALSkillTreeDefinition.generated.h"

class UMALGameplayAbility;

/**
 * What a skill tree node looks like from the player's side.
 *
 * Locked is the resting state of an unearned node; Available means every requirement is met and
 * only the points are missing or the button is not pressed yet; Unlocked is permanent until the
 * save is deleted.
 */
UENUM(BlueprintType)
enum class EMALSkillNodeState : uint8
{
	/** Level or prerequisites not met. */
	Locked,

	/** Requirements met, waiting for the player to spend the points. */
	Available,

	/** Already owned. */
	Unlocked
};

/**
 * FMALSkillNodeDefinition
 *
 * One entry of the skill pool. Describes the skill whether or not the player owns it, which is
 * what lets the menu draw a locked node: an ungranted ability has no spec, so anything the panel
 * needs on that node has to live on the node itself.
 */
USTRUCT(BlueprintType)
struct FMALSkillNodeDefinition
{
	GENERATED_BODY()

	/**
	 * Stable identity of the node.
	 *
	 * Prerequisites and the save file both reference this rather than the ability class: a class
	 * can be renamed or replaced by a reskin while the player's save has to keep pointing at the
	 * same node.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Skill")
	FGameplayTag SkillTag;

	/** Left empty, the menu falls back to the ability's own DisplayName. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Skill")
	FText DisplayName;

	/** Shown in the detail panel. Lives here because abilities carry no description. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Skill", meta = (MultiLine = "true"))
	FText Description;

	/** Left empty, the menu falls back to the ability's own Icon. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Skill")
	TSoftObjectPtr<UTexture2D> Icon;

	/** The ability granted once the node is unlocked. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Skill")
	TSubclassOf<UMALGameplayAbility> AbilityClass;

	/**
	 * Input this ability answers once granted.
	 *
	 * Must be a tag that is actually routed: a spec carrying an input tag no action ever sends
	 * is granted and silently unreachable. Empty grants the ability with no input binding, which
	 * is only useful for passive skills.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Skill")
	FGameplayTag InputTag;

	/**
	 * Weapon the player has to be holding for this node to be buyable and granted.
	 *
	 * Empty is a general skill: always buyable, always granted, regardless of what is in hand.
	 * Set, the node is gated on the equipped weapon's own WeaponTag, matched by hierarchy.
	 *
	 * Ownership and availability are deliberately separate. Buying the node writes its tag into
	 * the player's unlocked set for good; swapping to another weapon only takes the ability back
	 * for as long as that weapon is equipped.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Skill")
	FGameplayTag RequiredWeaponTag;

	/** Character level needed before the node can be bought. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Skill", meta = (ClampMin = "1"))
	int32 RequiredLevel = 1;

	/** Skill points charged on unlock. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Skill", meta = (ClampMin = "0"))
	int32 Cost = 1;

	/** Draw order band. The menu lays one row out per tier, in ascending order. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Skill")
	int32 Tier = 0;

	/** Nodes that must all be unlocked first. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Skill")
	TArray<FGameplayTag> Prerequisites;
};

/**
 * Name to show for a node, taken from the first source that has one: the node itself, the
 * ability's class default object, then the tag's last segment.
 *
 * Shared rather than duplicated: the menu prints it as a row's title and the progression
 * component prints the same text inside "Requires ...", so the two must not disagree about
 * what a node is called.
 */
MAL_API FText MALGetSkillNodeDisplayName(const FMALSkillNodeDefinition& Node);

/**
 * UMALSkillTreeDefinition
 *
 * The full skill pool of one class: every node the menu draws, owned or not.
 *
 * Held by UMALClassDefinition rather than globally, because which skills exist is part of what
 * makes a class a class -- the same reasoning that puts the ability sets and the stat cap there.
 */
UCLASS(BlueprintType)
class MAL_API UMALSkillTreeDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Every node in the pool. Order here is the order the menu writes them in. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Skill Tree")
	TArray<FMALSkillNodeDefinition> Nodes;

	/** The node carrying SkillTag, or null when the tree has no such entry. */
	const FMALSkillNodeDefinition* FindNode(FGameplayTag SkillTag) const;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
