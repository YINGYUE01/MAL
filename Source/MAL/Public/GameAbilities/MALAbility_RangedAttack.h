// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameAbilities/MALGameplayAbility.h"
#include "Combat/MALDamageExecutor.h"
#include "Templates/SubclassOf.h"
#include "Engine/EngineTypes.h"
#include "MALAbility_RangedAttack.generated.h"

class AMALProjectile;
class UGameplayEffect;

/**
 * UMALAbility_RangedAttack
 *
 * A shot: plays an optional cast montage, waits FireDelay, then spawns AMALProjectile from the
 * character's muzzle socket. Everything about the shot that a designer tunes lives here
 * (projectile class, speed, penetration, damage, cue), so a new ranged enemy is this class
 * plus a montage and a projectile blueprint.
 *
 * Damage is not resolved here: the projectile carries an FMALWeaponHitRequest built at fire
 * time and hands it to UMALDamageExecutor on the first blocking hit. That keeps friendly fire,
 * dead targets and immunity checks in the one place melee already uses.
 *
 * Activation is tagged MAL.Ability.Attack so the behaviour tree's generic attack task can
 * drive a ranged enemy without a separate branch.
 */
UCLASS()
class MAL_API UMALAbility_RangedAttack : public UMALGameplayAbility
{
	GENERATED_BODY()

public:
	UMALAbility_RangedAttack();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	/**
	 * Called by the MAL Ranged Fire anim notify on the release frame.
	 *
	 * The shot is timed by the montage rather than by a timer, exactly like a melee swing opens
	 * its hit window from a notify. Safe on clients (nothing happens there) and idempotent: a
	 * montage that reports the notify twice still fires once per activation.
	 */
	void NotifyFireProjectile();

protected:
	/** Server only: spawn the projectile, called from the MAL Ranged Fire notify. */
	void FireProjectile();

	/** Payload handed to the projectile; the target and impact point are filled per hit. */
	FMALWeaponHitRequest BuildDamageRequest() const;

	/**
	 * Target the shot is aimed at: the AI controller's current combat target, or null.
	 * A player-fired variant can override this to return its lock-on target instead.
	 */
	virtual AActor* ResolveAimTarget(AActor* Avatar) const;

	/** World direction from the muzzle toward the aim point, falling back to the pawn's aim. */
	FVector ResolveAimDirection(const FVector& MuzzleLocation, AActor* Avatar) const;

	/** What flies. Empty logs a warning and ends the ability without firing. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ranged")
	TSubclassOf<AMALProjectile> ProjectileClass;

	/**
	 * Socket the shot leaves from. None reads the equipped weapon definition, which is what a
	 * bow enemy with a weapon asset wants; set it explicitly for enemies without one.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ranged")
	FName MuzzleSocketName = NAME_None;

	/** Seconds the ability stays active after the shot when no montage is set. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ranged", meta = (ClampMin = "0.0"))
	float RecoveryTime = 0.4f;

	/** Height above the target's origin the shot is aimed at (chest instead of feet). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ranged|Aim")
	float TargetAimHeightOffset = 40.0f;

	/** Cruise speed of the projectile. Zero keeps the projectile blueprint's default. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ranged", meta = (ClampMin = "0.0"))
	float ProjectileSpeed = 5000.0f;

	/** Extra bodies the shot may pass through after the first. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ranged", meta = (ClampMin = "0"))
	int32 Penetrations = 0;

	/**
	 * Aim ahead of a moving target by the projectile's travel time.
	 *
	 * A 5000 cm/s arrow needs about 0.2 s to cross a room, which is enough for a running
	 * player to step out of a shot aimed at where they were.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ranged|Aim")
	bool bLeadTarget = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ranged|Damage")
	TSubclassOf<UGameplayEffect> DamageGEClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ranged|Damage", meta = (ClampMin = "0.0"))
	float BaseDamage = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ranged|Feedback")
	FGameplayTag ImpactCueTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ranged|Feedback", meta = (ClampMin = "0.0"))
	float HitStopDuration = 0.04f;

private:
	FTimerHandle RecoveryTimer;

	/** Guards against a second shot from a reused, instanced ability. */
	bool bFired = false;
};
