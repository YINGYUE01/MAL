// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/MALSkillTreeDefinition.h"

#include "GameAbilities/MALGameplayAbility.h"

FText MALGetSkillNodeDisplayName(const FMALSkillNodeDefinition& Node)
{
	if (!Node.DisplayName.IsEmpty())
	{
		return Node.DisplayName;
	}

	// An ungranted ability has no spec, so its class default object is the only other source.
	if (Node.AbilityClass)
	{
		const UMALGameplayAbility* AbilityDefaults = Cast<UMALGameplayAbility>(Node.AbilityClass->GetDefaultObject());
		if (AbilityDefaults && !AbilityDefaults->DisplayName.IsEmpty())
		{
			return AbilityDefaults->DisplayName;
		}
	}

	// Last resort, so a row is never blank. Splits on the last dot when there is one and keeps
	// the whole string otherwise: that covers both the dotted MAL.Skill.* convention and a flat
	// tag such as Skill_Whirlwind, which would otherwise print its underscores to the player.
	FString TagString = Node.SkillTag.ToString();
	int32 LastDot = INDEX_NONE;
	if (TagString.FindLastChar(TEXT('.'), LastDot))
	{
		TagString.RightChopInline(LastDot + 1);
	}

	return FText::FromString(TagString);
}

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
