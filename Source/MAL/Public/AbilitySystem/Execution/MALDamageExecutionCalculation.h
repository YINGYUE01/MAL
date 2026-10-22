// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "MALDamageExecutionCalculation.generated.h"

/**
 * UMALDamageExecutionCalculation
 *
 * 伤害结算执行类（物理/魔法双通道 + 暴击）。
 *
 * 公式:
 *   Base    = SetByCaller(MAL.Data.Damage)          // 技能侧算好的基础伤害（含等级/连招加成）
 *   Attack  = 物理: PhysicalAttack / 魔法: MagicAttack      (Source)
 *   Defense = 物理: PhysicalDefense / 魔法: MagicDefense    (Target)
 *   Damage  = Base × (1 + Attack / AttackScalingDivisor)
 *                   × (DefenseScalingConstant / (DefenseScalingConstant + Defense))
 *   Crit    = Roll[0, 100) < CritChance → Damage × CritDamage / 100
 *
 * 通道由 GE spec 上的 MAL.Damage.Type.Magic 标签决定（动态标签或 GE 资产标签均可），
 * 缺省按物理结算。暴击结果回写 FMALGameplayEffectContext。
 *
 * 输出写入 MALAttributeSet.Damage（Meta 属性），由 PostGameplayEffectExecute 结算进 Health。
 */
UCLASS(BlueprintType, Blueprintable)
class MAL_API UMALDamageExecutionCalculation : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	UMALDamageExecutionCalculation();

	virtual void Execute_Implementation(
		const FGameplayEffectCustomExecutionParameters& ExecutionParams,
		FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;

protected:
	/** 攻击力换算基数：每 1 点 Attack 提供 1 / AttackScalingDivisor 的百分比加成。 */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|Damage", meta = (ClampMin = "1.0"))
	float AttackScalingDivisor = 100.f;

	/** 防御减伤基数 K：减伤系数 = K / (K + Defense)。 */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|Damage", meta = (ClampMin = "1.0"))
	float DefenseScalingConstant = 100.f;

	/** 关闭后跳过暴击判定（调试或特殊技能使用）。 */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|Damage")
	bool bAllowCriticalHits = true;

private:
	/** 该次结算是否走魔法通道（读取 Spec 上的 MAL.Damage.Type.Magic 标签）。 */
	static bool IsMagicDamageSpec(const FGameplayEffectSpec& Spec);
};
