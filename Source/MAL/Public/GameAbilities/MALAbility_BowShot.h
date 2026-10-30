// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Combat/MALDamageExecutor.h"
#include "GameAbilities/MALGameplayAbility.h"
#include "MALAbility_BowShot.generated.h"

class ACharacter;
class AMALProjectile;
class UAbilityTask_PlayMontageAndWait;
class UCameraShakeBase;
class UGameplayEffect;
class UMALWeaponComponent;
class UMALWeaponDefinition;

/**
 * FMALBowChargeTier
 *
 * One step of the draw. Holding the input past ChargeTime reaches it; the highest tier whose
 * ChargeTime has been met is the one that fires. Order matters: the list is read from the start.
 */
USTRUCT(BlueprintType)
struct FMALBowChargeTier
{
	GENERATED_BODY()

	/** Seconds the input must be held to reach this tier. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Bow", meta = (ClampMin = "0.0"))
	float ChargeTime = 0.0f;

	/** Scales BaseDamage. A quick tap should read as weaker than a full draw. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Bow", meta = (ClampMin = "0.0"))
	float DamageScale = 1.0f;

	/** Extra bodies the arrow passes through after the first. Zero stops on the first one hit. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Bow", meta = (ClampMin = "0"))
	int32 Penetrations = 0;

	/** Launch speed in cm/s. Zero keeps the projectile class default. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Bow", meta = (ClampMin = "0.0"))
	float Speed = 0.0f;
};

/**
 * UMALAbility_BowShot
 *
 * Hold-to-draw, release-to-fire bow shot bound to MAL.Input.LMB.
 *
 * The whole shot is one activation: pressing draws and enters the aiming stance, holding past a
 * tier threshold raises the damage, releasing fires. That is why the policy is OnInputTriggered
 * rather than WhileInputActive - a held-input ability is cancelled by the ASC on release, and a
 * cancelled shot is a shot that never leaves the bow. The release is read through
 * OnInputReleased instead, so the ability decides when it ends.
 *
 * Damage goes through UMALDamageExecutor via AMALProjectile, so friendly fire, corpses, hit
 * feedback and the WeaponHit event behave exactly as they do for melee.
 *
 * Rounds are spent through UMALWeaponComponent::TryConsumeAmmo, which reports true for a weapon
 * that does not track them (MagazineSize of INDEX_NONE). The bow ships unlimited; turning the
 * magazine on later needs no change here.
 */
UCLASS()
class MAL_API UMALAbility_BowShot : public UMALGameplayAbility
{
	GENERATED_BODY()

public:
	UMALAbility_BowShot();

	/** Fired by the input release: the shot leaves, and the ability ends with the animation. */
	void FireShot();

protected:
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

	virtual void OnInputReleased(const FInputActionValue& Value) override;

	virtual void OnAbilityMontageEnded(bool bInterrupted) override;

	// --- Damage ---

	/** Damage effect; null falls back to the executor's default effect. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Bow|Damage")
	TSubclassOf<UGameplayEffect> DamageGEClass;

	/** Damage at DamageScale 1, before the equipped weapon's multiplier and level scaling. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Bow|Damage", meta = (ClampMin = "0.0"))
	float BaseDamage = 18.0f;

	/** Cue played where the arrow lands. Resolved by tag, so the assets live in the cue notify. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Bow|Damage")
	FGameplayTag HitImpactCueTag;

	/**
	 * Local hit-stop requested on the shooter's client. Zero on purpose: freezing the shooter for
	 * landing an arrow reads as lag, unlike the confirmation a sword hit gives.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Bow|Damage", meta = (ClampMin = "0.0"))
	float HitStopDuration = 0.0f;

	/** Camera shake played on the shooter's client when the arrow connects. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Bow|Damage")
	TSubclassOf<UCameraShakeBase> CameraShakeClass;

	// --- Charge ---

	/** Tiers in ascending ChargeTime order. Empty fires one tier at DamageScale 1. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Bow|Charge")
	TArray<FMALBowChargeTier> ChargeTiers;

	/** Section of AbilityMontage the draw loops on. None starts at the beginning. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Bow|Animation")
	FName DrawSection = TEXT("Draw");

	/** Section jumped to on release. Missing section ends the ability after RecoverTime. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Bow|Animation")
	FName FireSection = TEXT("Fire");

	/** How long the ability outlives the shot when no fire section drives it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Bow|Animation", meta = (ClampMin = "0.0"))
	float RecoverTime = 0.25f;

	/**
	 * Seconds the ability stays active after the release, before the next shot is allowed.
	 *
	 * This is the shot's recovery, and it is deliberately not the fire animation's length: the
	 * animation is edited for looks, the recovery is tuned for feel, and tying the two together
	 * means every change to the fire section is also a change to how long the player waits.
	 *
	 * Negative plays the fire animation out instead, which is what a montage whose release the
	 * animator wants seen in full should use. Zero frees the next shot on the release frame.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Bow|Animation")
	float PostFireEndDelay = 0.2f;

	// --- Projectile ---

	/** Projectile class used when the equipped weapon names none. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Bow|Projectile")
	TSubclassOf<AMALProjectile> ProjectileClass;

	/** How far ahead the camera aim point is searched. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Bow|Projectile", meta = (ClampMin = "100.0"))
	float AimTraceDistance = 20000.0f;

	// --- Helpers ---

	/** Character owning this ability; null when the avatar is not a MAL character. */
	class AMALCharacterBase* GetMALCharacter() const;

	/** Weapon component of the owning character; null when it has none. */
	UMALWeaponComponent* GetWeaponComponent() const;

	/** Equipped definition, or null when unarmed. */
	const UMALWeaponDefinition* GetWeaponDefinition() const;

	/** Projectile the shot spawns: the weapon's own, or the ability's fallback. */
	TSubclassOf<AMALProjectile> GetProjectileClass() const;

	/** Highest tier whose ChargeTime has been met. Falls back to an uncharged shot. */
	FMALBowChargeTier SelectTier(float HeldSeconds) const;

	/** Server only: spawn the arrow along the camera aim. Returns false when nothing was fired. */
	bool SpawnArrow(const FMALBowChargeTier& Tier);

	/** Everything the projectile needs to resolve its hits, already scaled by tier and weapon. */
	FMALWeaponHitRequest BuildHitRequest(const FMALBowChargeTier& Tier);

	/** Jump the running montage to FireSection, if both exist. */
	void AdvanceMontageToFire();

	/** End the ability after RecoverTime; used when no fire animation carries it. */
	void ScheduleRecoverEnd();

	/** End the ability once PostFireEndDelay has elapsed, so recovery is not the animation. */
	void SchedulePostFireEnd();

	/** Timer callback: close the ability, which is what removes Status.Aiming and frees the input. */
	void HandlePostFireEnd();

	/** Montage task of the current draw, kept so the fire jump knows there is one. */
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	/** World seconds the input went down. */
	float ChargeStartTime = 0.0f;

	/** True once this activation has released a shot. */
	bool bFired = false;

	/** Counts down to the end of the ability when the montage does not. */
	FTimerHandle RecoverTimer;

	/** Counts down PostFireEndDelay after the release. */
	FTimerHandle PostFireEndTimer;
};
