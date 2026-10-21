// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/MALClassDefinition.h"

FPrimaryAssetId UMALClassDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId("ClassDefinition", GetFName());
}
