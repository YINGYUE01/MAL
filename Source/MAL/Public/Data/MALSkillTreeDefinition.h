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
 * One entry of the skill pool. The node owns only what is unique to the tree: its stable identity,
 * the ability it grants, and the progression gates. Presentation (name, description, icon) and the
 * input binding come from the ability's class default object, so the tree cannot disagree with the
 * ability it grants.
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

	/** The ability granted once the node is unlocked. Presentation and input come from it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Skill")
	TSubclassOf<UMALGameplayAbility> AbilityClass;

	/**
	 * True when the weapon grants this node without a purchase (basic attack, dash, ...).
	 *
	 * Default nodes are always owned while the weapon is equipped and never enter the save, so
	 * they replace the old per-weapon ability sets without becoming purchases.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Skill")
	bool bStartsUnlocked = false;

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
 * Name to show for a node, taken from the first source that has one: the ability's class default
 * object, then the tag's last segment. Ungranted abilities have no spec, so the CDO is the only
 * authored source for a locked row.
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
