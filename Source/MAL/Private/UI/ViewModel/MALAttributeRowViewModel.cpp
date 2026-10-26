// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ViewModel/MALAttributeRowViewModel.h"

#include "UObject/UnrealType.h"

void UMALAttributeRowViewModel::Initialize(EMALPrimaryStat InStat)
{
	PrimaryStat = InStat;
	Attribute = MALGetPrimaryAttribute(InStat);

	// GetDisplayNameText resolves to the DisplayName metadata on the property and falls back to
	// the property name, so every stat gets a readable label without a table here.
	const FText Label = Attribute.IsValid() && Attribute.GetUProperty()
		? Attribute.GetUProperty()->GetDisplayNameText()
		: FText::GetEmpty();

	// Set through the macro so a view that is already bound picks the label up; a plain
	// assignment would leave it showing whatever it had at bind time.
	UE_MVVM_SET_PROPERTY_VALUE(AttributeName, Label);
}

void UMALAttributeRowViewModel::RequestAddPoint()
{
	if (AddPointRequested)
	{
		AddPointRequested(PrimaryStat);
	}
}

void UMALAttributeRowViewModel::OnAbilitySystemReady(UAbilitySystemComponent* InAbilitySystemComponent)
{
	Super::OnAbilitySystemReady(InAbilitySystemComponent);

	if (!Attribute.IsValid())
	{
		// A row configured without a stat has nothing to follow; it keeps its last value.
		return;
	}

	BindAttribute(Attribute, [this](const FOnAttributeChangeData& Data)
	{
		// The attribute is a float by necessity and the row prints whole numbers, so the
		// rounding lives here instead of in every consumer.
		UE_MVVM_SET_PROPERTY_VALUE(DisplayValue, FMath::RoundToInt(Data.NewValue));
	});
}
