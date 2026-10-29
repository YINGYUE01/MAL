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
 * FMALUnlockedSkill
 *
 * One bought node, paired with the weapon whose tree holds it.
 *
 * Ownership is per weapon rather than a flat set of tags. The same tag may appear in two
 * weapons' trees, and without the pairing the two purchases would collapse into one: buying a
 * node on one weapon would silently unlock the other's. Nothing here is skill-tree shaped
 * either, so a weapon can be repointed at another tree without stranding what was bought --
 * the node is looked up again under the same weapon on the next grant.
 */
USTRUCT()
struct FMALUnlockedSkill
{
	GENERATED_BODY()

	FMALUnlockedSkill() = default;

	FMALUnlockedSkill(const FGameplayTag& InWeaponTag, const FGameplayTag& InSkillTag)
		: WeaponTag(InWeaponTag)
		, SkillTag(InSkillTag)
	{
	}

	/** Weapon that owns the purchase, from its own WeaponTag. Compared exactly, not by hierarchy. */
	UPROPERTY()
	FGameplayTag WeaponTag;

	/** The node's own SkillTag. */
	UPROPERTY()
	FGameplayTag SkillTag;
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
 * The full skill pool of one weapon: every node the menu draws, owned or not.
 *
 * Held by UMALWeaponDefinition rather than globally, because which skills exist is part of what
 * makes a weapon a weapon -- switching weapons is meant to switch the whole kit, menu included.
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
