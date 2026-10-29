// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MALClassDefinition.generated.h"

class UMALAbilitySet;
class UMALSkillTreeDefinition;
class UCurveTable;

/**
 * UMALClassDefinition
 * 
 * Defines a character class/profession (e.g. Warrior, Mage).
 * Contains a list of ability sets to grant to the character.
 */
UCLASS(BlueprintType)
class MAL_API UMALClassDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Ability sets granted to characters using this class definition */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Class Definition")
	TArray<TObjectPtr<UMALAbilitySet>> GrantedAbilitySets;

	/**
	 * Unused since skills moved onto the weapon.
	 *
	 * The pool now hangs off UMALWeaponDefinition::SkillTree, so a swap changes the menu as well
	 * as the kit. Kept so existing class definitions keep loading; nothing reads this field.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Progression")
	TObjectPtr<UMALSkillTreeDefinition> SkillTree;

	// --- Progression ---

	/**
	 * Highest level a character of this class can reach.
	 *
	 * Defaulted well above 1 so a class definition that forgets this field still levels; capping at
	 * the starting level by accident would be a silent no-op rather than an obvious mistake.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Progression", meta = (ClampMin = "1"))
	int32 MaxCharacterLevel = 50;

	/**
	 * Attribute points granted for every level gained.
	 *
	 * A level-up does not write the primary stats: it pays out points and the player decides
	 * where they go. Zero disables the award without stopping experience.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Progression", meta = (ClampMin = "0"))
	int32 AttributePointsPerLevel = 3;

	/**
	 * Skill points granted for every level gained.
	 *
	 * A second currency rather than a share of the attribute points: where a point goes into a
	 * stat, a skill point buys a whole ability, and letting the player trade one for the other
	 * would make every build either all stats or all skills. Zero disables skill unlocking
	 * without touching the tree.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Progression", meta = (ClampMin = "0"))
	int32 SkillPointsPerLevel = 1;

	/**
	 * Cap on points that can be put into one primary stat. Zero means no cap.
	 *
	 * Per class because how lopsided a build may become is part of the class identity, not a rule
	 * of the progression system.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Progression", meta = (ClampMin = "0"))
	int32 MaxAttributePointsPerStat = 0;

	/**
	 * Experience needed to leave a level, evaluated with the current level as the horizontal axis.
	 * Row 1 is therefore the cost of reaching level 2.
	 *
	 * Missing table or row means the class gains no experience: the threshold reads as zero and the
	 * level-up loop stops on it. Authored per class because the pacing of a class is part of what
	 * makes it feel different.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Progression")
	TObjectPtr<UCurveTable> ExperienceToNextLevelTable;

	/** Row in ExperienceToNextLevelTable holding the threshold curve. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Progression")
	FName ExperienceToNextLevelRowName = TEXT("Experience");

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
