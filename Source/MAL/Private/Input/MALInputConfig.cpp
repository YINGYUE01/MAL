// Copyright Epic Games, Inc. All Rights Reserved.

#include "Input/MALInputConfig.h"

UInputAction* UMALInputConfig::FindActionByTag(const FGameplayTag& InputTag) const
{
	for (const FMALInputActionMapping& Mapping : AbilityInputActions)
	{
		if (Mapping.InputTag == InputTag && Mapping.Action)
		{
			return Mapping.Action;
		}
	}
	return nullptr;
}

FGameplayTag UMALInputConfig::FindTagByAction(const UInputAction* Action) const
{
	for (const FMALInputActionMapping& Mapping : AbilityInputActions)
	{
		if (Mapping.Action == Action)
		{
			return Mapping.InputTag;
		}
	}
	return FGameplayTag();
}

FPrimaryAssetId UMALInputConfig::GetPrimaryAssetId() const
{
	return FPrimaryAssetId("InputConfig", GetFName());
}
