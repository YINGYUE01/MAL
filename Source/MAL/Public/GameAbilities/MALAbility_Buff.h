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
 * Self-buff ability: spends mana once and puts a timed damage-bonus GE on the caster.
 *
 * With AbilityMontage set, the ability lives until the cast animation finishes or is
 * interrupted; without one it applies the buff and ends on the same frame. The buff's lifetime
 * is unrelated to the ability's: the GE owns it, so expiry, dispels and the target's death all
 * remove it and the gameplay cue it spawned. No timer is needed here.
 *
 * The cost goes through CostGameplayEffectClass (a blueprint subclass of
 * UMALManaCostGameplayEffect). When mana is short, CheckCost rejects the cast during
 * CanActivateAbility, so ActivateAbility never runs.
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

	/**
	 * Cancels the cast timeout on the normal path.
	 *
	 * Status.Casting is held for as long as this ability runs, and AMALPlayerController reads it
	 * to lock Move/Jump -- so an ability that never ends locks the player out of moving, with no
	 * way back short of restarting the session.
	 */
	virtual void OnAbilityMontageEnded(bool bInterrupted) override;

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

	/** Applies BuffEffectClass to the caster with the configured magnitude and duration. */
	void ApplyBuff();

	/**
	 * Montage used to live on this class before it moved to UMALGameplayAbility::AbilityMontage.
	 * Kept only so assets saved earlier keep their reference; PostLoad copies it across.
	 */
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Set AbilityMontage instead."))
	TObjectPtr<UAnimMontage> ActivationMontage;

	virtual void PostLoad() override;

private:
	/** Ends the cast when the montage never reports back, releasing Status.Casting. */
	void HandleCastTimeout();

	/** Set when the montage starts playing; cleared when it ends normally. */
	FTimerHandle CastTimeoutHandle;
};
