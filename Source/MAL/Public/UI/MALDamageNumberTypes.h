// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MALDamageNumberTypes.generated.h"

/**
 * FMALDamageNumberColors
 *
 * Authorable colour table for floating damage numbers.
 *
 * The palette lives on the view (UMALDamageNumberWidget) so re-skinning the numbers is a
 * blueprint edit rather than a recompile. The view model only turns a hit's flags into one of
 * these entries, which keeps the mapping rule in one place instead of spread across widgets.
 *
 * Every colour is authored in sRGB-linear as UMG expects; alpha is ignored by TextBlock colour
 * bindings, so leaving it at 1 is intentional.
 */
USTRUCT(BlueprintType)
struct FMALDamageNumberColors
{
	GENERATED_BODY()

	/** Physical hit that did not crit. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Damage")
	FLinearColor Physical = FLinearColor(1.0f, 0.94f, 0.86f);

	/** Magic hit that did not crit. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Damage")
	FLinearColor Magic = FLinearColor(0.42f, 0.62f, 1.0f);

	/** Critical physical hit. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Damage")
	FLinearColor Critical = FLinearColor(1.0f, 0.76f, 0.16f);

	/** Critical magic hit. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Damage")
	FLinearColor CriticalMagic = FLinearColor(0.86f, 0.42f, 1.0f);

	/**
	 * The hit that took the target from alive to dead.
	 * Deliberately the loudest entry: it outranks every other flag in Resolve().
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Damage")
	FLinearColor KillingBlow = FLinearColor(1.0f, 0.18f, 0.16f);

	/**
	 * Pick the colour for one hit.
	 *
	 * Priority is killing blow > critical > damage channel, so "critical magic killing blow"
	 * still reads as a kill. A channel-first order would make the loudest moments look plain.
	 */
	FLinearColor Resolve(bool bCritical, bool bMagic, bool bKillingBlow) const;
};
