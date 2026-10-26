// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "MALSecondaryStat.generated.h"

/**
 * The stats derived from the primary ones.
 *
 * None of these can be spent on: a point lands on a primary and these follow from it through
 * the secondary attribute effect, so the panel shows them and stops there.
 */
UENUM(BlueprintType)
enum class EMALSecondaryStat : uint8
{
	MaxHealth,
	HealthRegen,
	MaxStamina,
	StaminaRegen,
	MaxMana,
	ManaRegen,
	PhysicalAttack,
	MagicAttack,
	PhysicalDefense,
	MagicDefense,
	CritChance,
	CritDamage,
	AttackSpeed,
	DamageBonus,

	/** Sentinel so arrays sized by stat stay in step with the list above. */
	Count UMETA(Hidden)
};

/**
 * How a stat's number reads on screen.
 *
 * The secondaries do not share one unit: MaxHealth is a plain count, CritChance is a percentage
 * and AttackSpeed is a multiplier just above 1. Rounding them all to whole numbers would print
 * 1.02 as 1 and lose the part the player is spending points to move.
 */
enum class EMALStatFormat : uint8
{
	/** Whole number: 600. */
	Integer,

	/** One decimal place and a trailing percent sign: 8.0%. */
	Percent,

	/** Whole number and a trailing percent sign: 170%. */
	PercentWhole,

	/** Two decimal places, no suffix: 1.10. */
	Multiplier,
};

/** The attribute Stat is stored in. Invalid for the Count sentinel. */
MAL_API FGameplayAttribute MALGetSecondaryAttribute(EMALSecondaryStat Stat);

/** How Stat should be printed. Integer for anything not listed in the implementation. */
MAL_API EMALStatFormat MALGetSecondaryStatFormat(EMALSecondaryStat Stat);

/**
 * Value as the row should show it.
 *
 * Lives here rather than in the view model so the panel and any future readout format a stat
 * the same way, and so the rule for a stat is one switch case instead of a per-widget setting.
 */
MAL_API FText MALFormatStatValue(float Value, EMALStatFormat Format);
