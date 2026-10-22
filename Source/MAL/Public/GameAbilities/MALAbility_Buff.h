// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MALGameplayAbility.h"
#include "MALAbility_Buff.generated.h"

class UGameplayEffect;
class UAnimMontage;

/**
 * UMALAbility_Buff
 *
 * 自我增益技能：施放时扣一次法力，给自己挂一个带持续时间的伤害加成 GE。
 *
 * 配了 ActivationMontage 时，技能会存活到施法动画播完或被中断；没配时施放当帧就结束。
 * buff 的存续与技能本身的生命周期无关，完全交给 GE：到期、被驱散、目标死亡都会
 * 自动移除，连带 GameplayCue 生成的特效一起消失。所以这里不需要任何计时器。
 *
 * 费用走 CostGameplayEffectClass（UMALManaCostGameplayEffect 的蓝图子类）。
 * 法力不足时 CheckCost 会在 CanActivateAbility 阶段直接拒绝，根本进不到 ActivateAbility。
 */
UCLASS()
class MAL_API UMALAbility_Buff : public UMALGameplayAbility
{
	GENERATED_BODY()

public:
	UMALAbility_Buff();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

protected:
	/** Buff effect applied to the caster. Falls back to UMALBuffGameplayEffect. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ability|Buff")
	TSubclassOf<UGameplayEffect> BuffEffectClass;

	/** Damage bonus granted while the buff is up, in percent (30 => +30%). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ability|Buff|Effect", meta = (ClampMin = "0.0", ClampMax = "500.0"))
	float DamageBonusMagnitude = 30.f;

	/** How long the buff lasts, in seconds. Overrides the GE asset's own duration. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ability|Buff|Effect", meta = (ClampMin = "0.1"))
	float BuffDuration = 15.f;

	/**
	 * Cast montage played on activation. Leave it empty and the ability applies the buff and
	 * ends on the same frame, which is the behaviour for buffs with no cast animation.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ability|Buff|Animation")
	TObjectPtr<UAnimMontage> ActivationMontage;

	/** Applies BuffEffectClass to the caster with the configured magnitude and duration. */
	void ApplyBuff();

	// --- Montage callbacks ---

	UFUNCTION()
	void OnMontageCompleted();

	UFUNCTION()
	void OnMontageInterrupted();

	UFUNCTION()
	void OnMontageCancelled();
};
