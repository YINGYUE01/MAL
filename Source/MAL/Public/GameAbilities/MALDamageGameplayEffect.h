// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "MALDamageGameplayEffect.generated.h"

/**
 * UMALDamageGameplayEffect
 *
 * Code-side fallback damage GE: instant, adds a SetByCaller amount to MALAttributeSet.Damage
 * (the meta attribute). Once the project has a real GE_Damage asset, point GA_AttackAbility's
 * DamageGEClass at it to override this.
 */
UCLASS()
class MAL_API UMALDamageGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UMALDamageGameplayEffect();
};
