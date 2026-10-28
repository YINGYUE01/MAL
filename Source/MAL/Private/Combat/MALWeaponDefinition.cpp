// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/MALWeaponDefinition.h"

FPrimaryAssetId UMALWeaponDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId("WeaponDefinition", GetFName());
}
