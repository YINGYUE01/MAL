// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Actor.h"
#include "MALGameplayCueNotify_Buff.generated.h"

class UNiagaraSystem;
class UNiagaraComponent;

/**
 * AMALGameplayCueNotify_Buff
 *
 * 通用"持续型增益特效" cue：OnActive 在目标的 SkeletalMesh 上生成 Niagara 系统并挂在
 * socket 上，OnRemove 立即销毁组件。
 *
 * 生命周期完全由 GameplayEffect 决定。GE 到期、被驱散或目标死亡都会走 OnRemove，
 * 所以特效不可能比 buff 多活一帧，也不需要技能侧自己开定时器去关。
 *
 * 蓝图子类放在 GameplayCueNotifyPaths（本项目为 /Game）下即可被 cue manager 扫描到；
 * 在子类里填 AuraEffect 并指定 socket。
 */
UCLASS()
class MAL_API AMALGameplayCueNotify_Buff : public AGameplayCueNotify_Actor
{
	GENERATED_BODY()

public:
	AMALGameplayCueNotify_Buff();

	virtual bool OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
	virtual bool OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;

protected:
	/** Niagara system spawned on the target while the effect is active. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Cue")
	TObjectPtr<UNiagaraSystem> AuraEffect;

	/** Socket on the target's skeletal mesh. Empty attaches to the mesh origin. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Cue")
	FName AttachSocketName = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Cue")
	FVector AttachLocationOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Cue")
	FRotator AttachRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Cue")
	FVector AttachScale = FVector(1.f);

	/** The spawned aura, destroyed when the effect stops. */
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> ActiveAura;
};
