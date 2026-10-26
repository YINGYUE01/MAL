// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ViewModel/MALAttributeRowViewModel.h"

#include "UObject/UnrealType.h"

void UMALAttributeRowViewModel::InitializePrimary(EMALPrimaryStat InStat)
{
	PrimaryStat = InStat;
	bCanSpendPoints = true;
	InitializeAttribute(MALGetPrimaryAttribute(InStat), EMALStatFormat::Integer);
}

void UMALAttributeRowViewModel::InitializeSecondary(EMALSecondaryStat InStat)
{
	// The primary stat is left at its sentinel: a derived row never reports one it could spend on.
	bCanSpendPoints = false;
	InitializeAttribute(MALGetSecondaryAttribute(InStat), MALGetSecondaryStatFormat(InStat));
}

void UMALAttributeRowViewModel::InitializeAttribute(const FGameplayAttribute& InAttribute, EMALStatFormat InFormat)
{
	Attribute = InAttribute;
	Format = InFormat;

	// GetDisplayNameText resolves to the DisplayName metadata on the property and falls back to
	// the property name, so every stat gets a readable label without a table here.
	const FText Label = Attribute.IsValid() && Attribute.GetUProperty()
		? Attribute.GetUProperty()->GetDisplayNameText()
		: FText::GetEmpty();

	// Set through the macro so a view that is already bound picks the label up; a plain
	// assignment would leave it showing whatever it had at bind time.
	UE_MVVM_SET_PROPERTY_VALUE(AttributeName, Label);

	// Painted before an ability system exists so the row never shows an empty right column.
	ApplyValue(0.0f);
}

void UMALAttributeRowViewModel::ApplyValue(float Value)
{
	const FText Formatted = MALFormatStatValue(Value, Format);
	UE_MVVM_SET_PROPERTY_VALUE(DisplayValue, Formatted);

	// Raised as well as set: see ValueChanged for why the binding alone is not enough.
	if (ValueChanged)
	{
		ValueChanged(Formatted);
	}
}

void UMALAttributeRowViewModel::RequestAddPoint()
{
	// A derived row is shown, not spent on. The menu never wires its button; this covers a
	// blueprint that calls the function directly.
	if (!bCanSpendPoints || !AddPointRequested)
	{
		return;
	}

	AddPointRequested(PrimaryStat);
}

void UMALAttributeRowViewModel::OnAbilitySystemReady(UAbilitySystemComponent* InAbilitySystemComponent)
{
	Super::OnAbilitySystemReady(InAbilitySystemComponent);

	if (!Attribute.IsValid())
	{
		// A row configured without a stat has nothing to follow; it keeps its last value.
		return;
	}

	// BindAttribute primes the callback with the current reading, so the row starts out correct
	// without reading the attribute a second time here.
	BindAttribute(Attribute, [this](const FOnAttributeChangeData& Data)
	{
		ApplyValue(Data.NewValue);
	});
}
