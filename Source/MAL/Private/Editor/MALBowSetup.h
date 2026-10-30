// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "MALBowSetup.generated.h"

/**
 * Creates and wires the bow shot content: the GA_BowShotAbility blueprint, the DA_AbilitySet_Bow
 * data asset and the ranged fields on DA_Weapon_Bow.
 *
 * Kept as a commandlet rather than hand-authored assets so the configuration is reviewable and
 * reproducible: the ability blueprint is a plain subclass with CDO values, which is exactly the
 * part of content work that does not need a human in the editor.
 *
 *  -run=MALBowSetup -Mode=dump    prints the bow definition, the ability blueprint and the
 *                                 character's loadout before anything is touched.
 *  -run=MALBowSetup -Mode=apply   creates whatever is missing and points the weapon at it.
 */
UCLASS()
class UMALBowSetupCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UMALBowSetupCommandlet();

	virtual int32 Main(const FString& Params) override;
};
