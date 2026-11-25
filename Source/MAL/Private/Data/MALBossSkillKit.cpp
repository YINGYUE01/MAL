// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/MALBossSkillKit.h"

int32 UMALBossSkillKit::GetPhaseForHealthPercent(float HealthPercent) const
{
	const float Percent = FMath::Clamp(HealthPercent, 0.0f, 1.0f);

	// Thresholds are authored high-to-low. A missing/invalid pair degrades to the earliest phase
	// rather than dividing by an inverted range.
	if (Percent <= Phase3Threshold)
	{
		return 2;
	}

	if (Percent <= Phase2Threshold)
	{
		return 1;
	}

	return 0;
}

FPrimaryAssetId UMALBossSkillKit::GetPrimaryAssetId() const
{
	return FPrimaryAssetId("BossSkillKit", GetFName());
}