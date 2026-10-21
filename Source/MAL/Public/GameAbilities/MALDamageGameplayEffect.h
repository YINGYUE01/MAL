// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "MALDamageGameplayEffect.generated.h"

/**
 * UMALDamageGameplayEffect
 *
 * 代码内置的伤害 GE 兜底：Instant，向 MALAttributeSet.Damage（meta）加 SetByCaller 伤害。
 * 项目里有正式的 GE_Damage 资产时，把 GA_AttackAbility 的 DamageGEClass 指向资产即可覆盖它。
 */
UCLASS()
class MAL_API UMALDamageGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UMALDamageGameplayEffect();
};
