// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "MALPrimaryStat.generated.h"

/**
 * The primary stats an attribute point can be spent on.
 *
 * An enum rather than a raw FGameplayAttribute because the point-spending entry point is called
 * from UI, and it keeps the set of spendable stats closed: a caller cannot aim a point at
 * MaxHealth by naming it.
 */
UENUM(BlueprintType)
enum class EMALPrimaryStat : uint8
{
	Vigor,
	Strength,
	Dexterity,
	Endurance,
	Spirit,

	/** Sentinel so arrays sized by stat stay in step with the list above. */
	Count UMETA(Hidden)
};

/**
 * The attribute Stat is stored in. Invalid for the Count sentinel.
 *
 * One switch shared by the panel that draws the stats and the component that spends a point
 * into one, so the two can never disagree about which attribute a stat means.
 */
MAL_API FGameplayAttribute MALGetPrimaryAttribute(EMALPrimaryStat Stat);
