// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "MALBuffGameplayEffect.generated.h"

/**
 * UMALBuffGameplayEffect
 *
 * Timed buff GE (currently its only effect is a damage bonus).
 *
 * The magnitude is not baked into the asset: the ability passes it through
 * SetByCaller(MAL.Data.DamageBonus) and overrides the duration on the spec, so the same GE asset
 * serves different abilities (a small +30% / 10s buff and an ultimate +80% / 30s buff alike).
 *
 * The visuals hang off the GE's GameplayCues, which are removed automatically on expiry, dispel
 * or the target's death, so no ability-side timer is needed to tear them down.
 */
UCLASS()
class MAL_API UMALBuffGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UMALBuffGameplayEffect();
};
