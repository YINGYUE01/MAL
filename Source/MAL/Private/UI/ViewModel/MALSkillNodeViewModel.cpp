// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ViewModel/MALSkillNodeViewModel.h"

#include "GameAbilities/MALGameplayAbility.h"
#include "Engine/Texture2D.h"

void UMALSkillNodeViewModel::Initialize(const FMALSkillNodeDefinition& InNode)
{
	// Set through the macro even though nothing is bound yet: Initialize is also the path a
	// caller takes to rebuild an existing row, and a plain assignment would not repaint it.
	UE_MVVM_SET_PROPERTY_VALUE(SkillTag, InNode.SkillTag);
	UE_MVVM_SET_PROPERTY_VALUE(Cost, FMath::Max(InNode.Cost, 0));
	UE_MVVM_SET_PROPERTY_VALUE(RequiredLevel, FMath::Max(InNode.RequiredLevel, 1));
	Tier = InNode.Tier;

	FText NodeName = InNode.DisplayName;
	FText NodeDescription = InNode.Description;
	TObjectPtr<UTexture2D> NodeIcon = InNode.Icon.IsNull() ? nullptr : InNode.Icon.LoadSynchronous();

	// The class default object is the only other source: an ungranted ability has no spec, so
	// the node's own fields are all a locked row has to draw with.
	const UMALGameplayAbility* AbilityDefaults = InNode.AbilityClass
		? Cast<UMALGameplayAbility>(InNode.AbilityClass->GetDefaultObject())
		: nullptr;
	if (AbilityDefaults)
	{
		if (NodeName.IsEmpty())
		{
			NodeName = AbilityDefaults->DisplayName;
		}

		if (!NodeIcon)
		{
			NodeIcon = AbilityDefaults->Icon;
		}
	}

	// Still empty past both sources: name the tag rather than leave a blank row. A skill tag is
	// always "MAL.Skill.Something", so the trailing segment reads as a name.
	if (NodeName.IsEmpty())
	{
		FString TagString = InNode.SkillTag.ToString();
		int32 LastDot = INDEX_NONE;
		if (TagString.FindLastChar(TEXT('.'), LastDot))
		{
			TagString.RightChopInline(LastDot + 1);
		}
		NodeName = FText::FromString(TagString);
	}

	UE_MVVM_SET_PROPERTY_VALUE(DisplayName, NodeName);
	UE_MVVM_SET_PROPERTY_VALUE(Description, NodeDescription);
	UE_MVVM_SET_PROPERTY_VALUE(Icon, NodeIcon);

	// A fresh node starts locked with no explanation; the menu pushes the real state as soon as
	// it has a player state to judge with.
	UE_MVVM_SET_PROPERTY_VALUE(NodeState, EMALSkillNodeState::Locked);
	UE_MVVM_SET_PROPERTY_VALUE(RequirementText, FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(bCanUnlock, false);
	UE_MVVM_SET_PROPERTY_VALUE(bIsSelected, false);
	RefreshStateText();
}

void UMALSkillNodeViewModel::ApplyState(EMALSkillNodeState InState, const FText& InReason)
{
	// The only writer of bCanUnlock. Reachable and affordable are separate facts in the
	// component; here they collapse into the one boolean a button can bind.
	const bool bInCanUnlock =
		InState == EMALSkillNodeState::Available && InReason.IsEmpty();

	UE_MVVM_SET_PROPERTY_VALUE(NodeState, InState);
	UE_MVVM_SET_PROPERTY_VALUE(RequirementText, InReason);
	UE_MVVM_SET_PROPERTY_VALUE(bCanUnlock, bInCanUnlock);
	RefreshStateText();
}

void UMALSkillNodeViewModel::SetSelected(bool bInSelected)
{
	UE_MVVM_SET_PROPERTY_VALUE(bIsSelected, bInSelected);
}

void UMALSkillNodeViewModel::RefreshStateText()
{
	FText Label;
	switch (NodeState)
	{
	case EMALSkillNodeState::Unlocked:
		Label = NSLOCTEXT("MALSkillNode", "StateUnlocked", "Unlocked");
		break;

	case EMALSkillNodeState::Available:
		Label = NSLOCTEXT("MALSkillNode", "StateAvailable", "Available");
		break;

	case EMALSkillNodeState::Locked:
	default:
		Label = NSLOCTEXT("MALSkillNode", "StateLocked", "Locked");
		break;
	}

	UE_MVVM_SET_PROPERTY_VALUE(StateText, Label);
}
