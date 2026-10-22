// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "MALBuffGameplayEffect.generated.h"

/**
 * UMALBuffGameplayEffect
 *
 * 带持续时间的增益 GE（当前唯一效果：伤害加成）。
 *
 * 幅度不写死在资产里：技能侧通过 SetByCaller(MAL.Data.DamageBonus) 传入，
 * 持续时间同样由技能在 spec 上覆盖，因此同一个 GE 资产可以被不同技能复用
 * （例如 +30% / 10s 的小增益与 +80% / 30s 的大招增益）。
 *
 * 特效挂在 GE 的 GameplayCues 上，GE 到期 / 被驱散 / 目标死亡时 cue 自动移除，
 * 因此不需要在技能里手写定时器去关特效。
 */
UCLASS()
class MAL_API UMALBuffGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UMALBuffGameplayEffect();
};
