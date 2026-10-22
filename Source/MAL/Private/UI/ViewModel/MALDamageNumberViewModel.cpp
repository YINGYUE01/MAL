// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ViewModel/MALDamageNumberViewModel.h"

#include "Internationalization/Text.h"

FLinearColor FMALDamageNumberColors::Resolve(bool bCritical, bool bMagic, bool bKillingBlow) const
{
	// Loudest first: a killing blow stays recognisable even when it also crit on the magic channel.
	if (bKillingBlow)
	{
		return KillingBlow;
	}

	if (bCritical)
	{
		return bMagic ? CriticalMagic : Critical;
	}

	return bMagic ? Magic : Physical;
}

void UMALDamageNumberViewModel::SetColorPalette(const FMALDamageNumberColors& InColors)
{
	Colors = InColors;
}

void UMALDamageNumberViewModel::SetDamage(float InAmount, bool bInCritical, bool bInMagic, bool bInKillingBlow)
{
	UE_MVVM_SET_PROPERTY_VALUE(Amount, InAmount);
	UE_MVVM_SET_PROPERTY_VALUE(bCritical, bInCritical);
	UE_MVVM_SET_PROPERTY_VALUE(bMagic, bInMagic);
	UE_MVVM_SET_PROPERTY_VALUE(bKillingBlow, bInKillingBlow);

	UE_MVVM_SET_PROPERTY_VALUE(DamageColor, FSlateColor(Colors.Resolve(bInCritical, bInMagic, bInKillingBlow)));

	// Ceil rather than round: a hit that lands for 0.4 should still read as "1" instead of
	// showing a damage number of zero. Grouping separators are suppressed because "1,234" reads
	// as two values at a glance.
	const int32 Rounded = FMath::Max(1, FMath::CeilToInt(InAmount));
	UE_MVVM_SET_PROPERTY_VALUE(DisplayText, FText::AsNumber(Rounded, &FNumberFormattingOptions::DefaultNoGrouping()));
}
