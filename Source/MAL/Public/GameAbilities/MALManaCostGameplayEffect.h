// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "MALManaCostGameplayEffect.generated.h"

/**
 * UMALManaCostGameplayEffect
 *
 * 技能费用 GE：Instant，向 MALAttributeSet.Mana 追加一个负值。
 *
 * 必须用固定的 Additive 负值，不能用 SetByCaller：
 * UAbilitySystemComponent::CanApplyAttributeModifiers 只对 Additive 修饰符做
 * "CurrentValue + Magnitude < 0" 的判定，SetByCaller 的幅度在 CheckCost 阶段还是 0，
 * 无法拒绝法力不足的施放。蓝图子类改 Magnitude 即可调整消耗。
 */
UCLASS()
class MAL_API UMALManaCostGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UMALManaCostGameplayEffect();
};
