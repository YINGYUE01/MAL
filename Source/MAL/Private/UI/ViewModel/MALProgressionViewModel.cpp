// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ViewModel/MALProgressionViewModel.h"

#include "AbilitySystem/Attributes/MALAttributeSet.h"
#include "AbilitySystem/Attributes/MALProgressionAttributeSet.h"
#include "AbilitySystemComponent.h"

void UMALProgressionViewModel::OnAbilitySystemReady(UAbilitySystemComponent* InAbilitySystemComponent)
{
	Super::OnAbilitySystemReady(InAbilitySystemComponent);

	BindAttribute(UMALProgressionAttributeSet::GetLevelAttribute(), [this](const FOnAttributeChangeData& Data)
	{
		// The attribute is a float by necessity; the display is an int. Rounding here keeps
		// every consumer from repeating the job.
		UE_MVVM_SET_PROPERTY_VALUE(DisplayLevel, FMath::Max(FMath::RoundToInt(Data.NewValue), 1));
	});

	BindAttribute(UMALProgressionAttributeSet::GetExperienceAttribute(), [this](const FOnAttributeChangeData& Data)
	{
		UE_MVVM_SET_PROPERTY_VALUE(Experience, Data.NewValue);
		RefreshExperiencePercent();
	});

	BindAttribute(UMALProgressionAttributeSet::GetExperienceToNextLevelAttribute(), [this](const FOnAttributeChangeData& Data)
	{
		UE_MVVM_SET_PROPERTY_VALUE(ExperienceToNextLevel, Data.NewValue);
		RefreshExperiencePercent();
	});

	BindAttribute(UMALProgressionAttributeSet::GetAttributePointsAttribute(), [this](const FOnAttributeChangeData& Data)
	{
		// Points are whole numbers in a float attribute; clamping at zero keeps a set that has not
		// been initialised from showing a debt.
		UE_MVVM_SET_PROPERTY_VALUE(AttributePoints, FMath::Max(FMath::RoundToInt(Data.NewValue), 0));
	});

	// The primaries live on the vitals set, which the same ASC owns. Rounding here keeps the
	// panel from formatting the same float in five places.
	BindAttribute(UMALAttributeSet::GetVigorAttribute(), [this](const FOnAttributeChangeData& Data)
	{
		UE_MVVM_SET_PROPERTY_VALUE(Vigor, FMath::RoundToInt(Data.NewValue));
	});

	BindAttribute(UMALAttributeSet::GetStrengthAttribute(), [this](const FOnAttributeChangeData& Data)
	{
		UE_MVVM_SET_PROPERTY_VALUE(Strength, FMath::RoundToInt(Data.NewValue));
	});

	BindAttribute(UMALAttributeSet::GetDexterityAttribute(), [this](const FOnAttributeChangeData& Data)
	{
		UE_MVVM_SET_PROPERTY_VALUE(Dexterity, FMath::RoundToInt(Data.NewValue));
	});

	BindAttribute(UMALAttributeSet::GetEnduranceAttribute(), [this](const FOnAttributeChangeData& Data)
	{
		UE_MVVM_SET_PROPERTY_VALUE(Endurance, FMath::RoundToInt(Data.NewValue));
	});

	BindAttribute(UMALAttributeSet::GetSpiritAttribute(), [this](const FOnAttributeChangeData& Data)
	{
		UE_MVVM_SET_PROPERTY_VALUE(Spirit, FMath::RoundToInt(Data.NewValue));
	});
}

void UMALProgressionViewModel::OnAbilitySystemReleased()
{
	Super::OnAbilitySystemReleased();

	// Same contract as the vitals view model: clear the copy so a reused HUD never shows the
	// previous character's progress.
	UE_MVVM_SET_PROPERTY_VALUE(DisplayLevel, 1);
	UE_MVVM_SET_PROPERTY_VALUE(Experience, 0.0f);
	UE_MVVM_SET_PROPERTY_VALUE(ExperienceToNextLevel, 0.0f);
	UE_MVVM_SET_PROPERTY_VALUE(ExperiencePercent, 0.0f);
	UE_MVVM_SET_PROPERTY_VALUE(AttributePoints, 0);
	UE_MVVM_SET_PROPERTY_VALUE(Vigor, 0);
	UE_MVVM_SET_PROPERTY_VALUE(Strength, 0);
	UE_MVVM_SET_PROPERTY_VALUE(Dexterity, 0);
	UE_MVVM_SET_PROPERTY_VALUE(Endurance, 0);
	UE_MVVM_SET_PROPERTY_VALUE(Spirit, 0);
}

void UMALProgressionViewModel::RefreshExperiencePercent()
{
	// A zero threshold (a set that has not been written yet) reports empty rather than full.
	const float Percent = (ExperienceToNextLevel > 0.0f)
		? FMath::Clamp(Experience / ExperienceToNextLevel, 0.0f, 1.0f)
		: 0.0f;

	UE_MVVM_SET_PROPERTY_VALUE(ExperiencePercent, Percent);
}