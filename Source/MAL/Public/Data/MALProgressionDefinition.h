// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MALProgressionDefinition.generated.h"

class UCurveTable;

/**
 * UMALProgressionDefinition
 *
 * Growth rules for a player character: level cap, points paid per level and the experience
 * curve. Split out of the old class definition because skill granting no longer belongs here:
 * a player's abilities come from the equipped weapon, and this asset only answers "how fast
 * does this character grow".
 */
UCLASS(BlueprintType)
class MAL_API UMALProgressionDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Highest level this character can reach. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Progression", meta = (ClampMin = "1"))
	int32 MaxCharacterLevel = 50;

	/** Attribute points granted for every level gained. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Progression", meta = (ClampMin = "0"))
	int32 AttributePointsPerLevel = 3;

	/** Skill points granted for every level gained. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Progression", meta = (ClampMin = "0"))
	int32 SkillPointsPerLevel = 1;

	/** Cap on points that can be put into one primary stat. Zero means no cap. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Progression", meta = (ClampMin = "0"))
	int32 MaxAttributePointsPerStat = 0;

	/**
	 * Experience needed to leave a level, evaluated with the current level as the horizontal
	 * axis. Row 1 is the cost of reaching level 2.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Progression")
	TObjectPtr<UCurveTable> ExperienceToNextLevelTable;

	/** Row in ExperienceToNextLevelTable holding the threshold curve. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Progression")
	FName ExperienceToNextLevelRowName = TEXT("Experience");

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
