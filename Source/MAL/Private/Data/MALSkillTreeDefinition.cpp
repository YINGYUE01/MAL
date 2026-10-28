// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/MALSkillTreeDefinition.h"

const FMALSkillNodeDefinition* UMALSkillTreeDefinition::FindNode(FGameplayTag SkillTag) const
{
	if (!SkillTag.IsValid())
	{
		return nullptr;
	}

	return Nodes.FindByPredicate([&SkillTag](const FMALSkillNodeDefinition& Node)
	{
		return Node.SkillTag == SkillTag;
	});
}

FPrimaryAssetId UMALSkillTreeDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId("SkillTree", GetFName());
}
