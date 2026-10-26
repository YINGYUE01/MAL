// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/Attributes/MALProgressionAttributeSet.h"
#include "Net/UnrealNetwork.h"

UMALProgressionAttributeSet::UMALProgressionAttributeSet()
{
	// Seeded here rather than by a GameplayEffect asset: an attribute nobody has touched reads as
	// zero, and a level 0 player would show as level 0 and divide the HUD bar by zero until the
	// first kill. The threshold starts at 1 rather than 0 so the ratio is defined from frame one.
	InitLevel(1.0f);
	InitExperience(0.0f);
	InitExperienceToNextLevel(1.0f);
	InitAttributePoints(0.0f);
}

void UMALProgressionAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// COND_None, like the vitals set: progression is what the HUD on the owning machine draws, and
	// REPNOTIFY_OnChanged keeps the cost at one update per actual change.
	DOREPLIFETIME_CONDITION_NOTIFY(UMALProgressionAttributeSet, Level, COND_None, REPNOTIFY_OnChanged);
	DOREPLIFETIME_CONDITION_NOTIFY(UMALProgressionAttributeSet, Experience, COND_None, REPNOTIFY_OnChanged);
	DOREPLIFETIME_CONDITION_NOTIFY(UMALProgressionAttributeSet, ExperienceToNextLevel, COND_None, REPNOTIFY_OnChanged);
	DOREPLIFETIME_CONDITION_NOTIFY(UMALProgressionAttributeSet, AttributePoints, COND_None, REPNOTIFY_OnChanged);
}

void UMALProgressionAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	// Only base-value writes pass through here; a value arriving over the wire lands in OnRep and
	// skips this entirely. So these are a guard on the authority's own writes, not a safety net
	// for bad data.
	if (Attribute == GetLevelAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.0f);
	}
	else if (Attribute == GetExperienceAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
	else if (Attribute == GetExperienceToNextLevelAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.0f);
	}
	else if (Attribute == GetAttributePointsAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
}

void UMALProgressionAttributeSet::OnRep_Level(const FGameplayAttributeData& OldLevel)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMALProgressionAttributeSet, Level, OldLevel);
}

void UMALProgressionAttributeSet::OnRep_Experience(const FGameplayAttributeData& OldExperience)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMALProgressionAttributeSet, Experience, OldExperience);
}

void UMALProgressionAttributeSet::OnRep_ExperienceToNextLevel(const FGameplayAttributeData& OldExperienceToNextLevel)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMALProgressionAttributeSet, ExperienceToNextLevel, OldExperienceToNextLevel);
}

void UMALProgressionAttributeSet::OnRep_AttributePoints(const FGameplayAttributeData& OldAttributePoints)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMALProgressionAttributeSet, AttributePoints, OldAttributePoints);
}