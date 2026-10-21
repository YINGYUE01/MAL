// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/MALAbilitySet.h"

FPrimaryAssetId UMALAbilitySet::GetPrimaryAssetId() const
{
	return FPrimaryAssetId("AbilitySet", GetFName());
}
