// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "MALBossSkillKitMigration.generated.h"

/**
 * One-shot migration for the single-entry Boss skill design (2026-11-24):
 *
 *  1. Creates DA_BossSkillKit_Default (UMALBossSkillKit) with the Boss's skills copied from
 *     the retired DA_BossAttackSet_Default menu, plus the grant-only GA_Combo2Ability entry.
 *  2. Updates BP_Boss: assigns BossSkillKit, sets BossDisplayName/BossBarActivationRadius on
 *     the character defaults, and clears InitialAbilities so the kit is the only grant source.
 *  3. Saves both assets.
 *
 * Idempotent: running it again overwrites the kit contents and re-applies the BP defaults.
 *
 *  -run=MALBossSkillKitMigration
 */
UCLASS()
class UMALBossSkillKitMigrationCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UMALBossSkillKitMigrationCommandlet();

	virtual int32 Main(const FString& Params) override;
};