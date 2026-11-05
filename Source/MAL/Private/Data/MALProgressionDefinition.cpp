// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/MALProgressionDefinition.h"

FPrimaryAssetId UMALProgressionDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId("ProgressionDefinition", GetFName());
}
