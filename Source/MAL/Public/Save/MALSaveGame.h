// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/MALSkillTreeDefinition.h"
#include "GameFramework/SaveGame.h"
#include "GameplayTagContainer.h"
#include "MALSaveGame.generated.h"

/**
 * UMALSaveGame
 *
 * The whole persistent profile of one player, in one flat object.
 *
 * Flat on purpose: there is exactly one slot and one character, so a struct per system would
 * only add a layer between the file and the numbers. ExperienceToNextLevel is deliberately
 * absent -- it is derived from the class definition's curve and would go stale the moment that
 * curve is retuned, while the level and the banked experience are facts about the player.
 */
UCLASS()
class MAL_API UMALSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/** Character level. Never below 1. */
	UPROPERTY()
	int32 Level = 1;

	/** Experience banked inside the current level. */
	UPROPERTY()
	int32 Experience = 0;

	/** Attribute points earned and not yet spent. */
	UPROPERTY()
	int32 AttributePoints = 0;

	/** Skill points earned and not yet spent. */
	UPROPERTY()
	int32 SkillPoints = 0;

	/**
	 * Skill tree nodes the player has bought, each with the weapon that owns it.
	 *
	 * One entry per purchase across every weapon rather than per weapon a list: nothing is
	 * granted for a weapon that is not in hand, so the flat record is both smaller and enough to
	 * rebuild any weapon's kit by filtering.
	 */
	UPROPERTY()
	TArray<FMALUnlockedSkill> UnlockedSkills;

	/**
	 * Points already put into each primary stat, indexed by EMALPrimaryStat.
	 *
	 * Saved separately because UMALProgressionComponent keeps this off the attribute set (the
	 * primaries are also written by equipment and effects, so they cannot be asked what was
	 * spent). Without it a load would restore the bank but forget the build.
	 */
	UPROPERTY()
	TArray<int32> AllocatedPoints;
};
