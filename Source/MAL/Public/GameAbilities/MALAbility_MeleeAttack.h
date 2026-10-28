// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameAbilities/MALGameplayAbility.h"
#include "Interfaces/MALComboAttackInterface.h"
#include "Combat/MALHitResult.h"
#include "Templates/SubclassOf.h"
#include "GameplayTagContainer.h"
#include "Engine/EngineTypes.h"
#include "MALAbility_MeleeAttack.generated.h"

class UAbilityTask_ApplyRootMotionMoveToForce;
class UCameraShakeBase;
class UGameplayEffect;
class UAnimMontage;
class UMALWeaponComponent;
struct FMALWeaponHitRequest;

/**
 * UMALAbility_MeleeAttack
 *
 * A swing: plays a montage and resolves hits through the character's weapon component. The
 * montage's MAL Weapon Collision notify opens and closes the hit window, and every overlap
 * inside it is handed to UMALDamageExecutor.
 *
 * Everything a melee skill needs to be tunable lives here (damage, lunge, knockback, hit
 * feedback), so a new swing ability is a subclass with a montage and nothing else. Combo
 * chaining is deliberately NOT part of this class: an AI swing wants the montage to play out
 * to the end, which is the opposite of what a combo's recovery release does. See
 * UMALAbility_Attack for chaining.
 */
UCLASS()
class MAL_API UMALAbility_MeleeAttack : public UMALGameplayAbility, public IMALComboAttackInterface
{
	GENERATED_BODY()

public:
	UMALAbility_MeleeAttack();

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

	//~ Begin IMALComboAttackInterface
	virtual void TriggerLunge() override;
	virtual void EnableWeaponCollision() override;
	virtual void DisableWeaponCollision() override;
	//~ End IMALComboAttackInterface

	/**
	 * Prefers the equipped weapon's montage over the ability's own.
	 *
	 * A greatsword and a dagger cannot share a swing animation, but they can share the ability:
	 * the weapon supplies the montage, the ability keeps the hit window, damage and cue logic.
	 */
	virtual UAnimMontage* GetAbilityMontage() const override;

protected:
	// --- Weapon window ---

	/** Resolve the character's weapon component (owns the hit window and the overlap binding). */
	UMALWeaponComponent* GetWeaponComponent() const;

	/** Name of the weapon collision component on the character (handed to UMALWeaponComponent). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ability|Melee|Weapon")
	FName WeaponCollisionSocketName = TEXT("weapon_collision");

	/**
	 * True once this activation opened a hit window. Reset on activation and checked on end to
	 * warn when the montage carries no opening notify: that swing silently cannot hit anything,
	 * which is easy to mistake for a targeting or range problem.
	 */
	bool bHitWindowOpened = false;

	/** Combo step stamped on the hit window. Flat melee abilities stay at 0. */
	virtual int32 GetHitComboStep() const { return 0; }

	// --- Recovery ---

	/**
	 * Seconds between the hit window closing and the ability releasing movement.
	 *
	 * The tail of a swing montage is recovery the player is not going to watch. Holding
	 * Status.Attacking through it locks Move/Jump (see AMALPlayerController) and keeps
	 * ActivationBlockedTags up, so the next input feels unresponsive. Ending here lets the
	 * montage blend out while the character is already free again.
	 *
	 * Negative disables this release. Named apart from UMALAbility_Attack's
	 * RecoveryReleaseDelay on purpose: the combo class owns its own window-aware release, and
	 * two same-named properties would shadow. UMALAbility_StoneGolemAttack opts out because an
	 * AI swing is meant to play out in full.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ability|Melee|Recovery", meta = (ClampMin = "-1.0", ClampMax = "1.5"))
	float PostHitReleaseDelay = 0.3f;

	/** Start the release countdown; called when the hit window closes. */
	void SchedulePostHitRelease();

	/** Drop a pending release: a new hit window means the swing is still going. */
	void CancelPostHitRelease();

	/** Timer callback: end the ability so movement and the next input are free again. */
	void HandlePostHitRelease();

	FTimerHandle PostHitReleaseTimer;

	// --- Damage ---

	/** Damage effect class to apply on weapon hit; null falls back to UMALDamageGameplayEffect. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ability|Melee|Damage")
	TSubclassOf<UGameplayEffect> DamageGEClass;

	/** Base damage magnitude applied to the damage GE */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ability|Melee|Damage")
	float BaseDamage = 10.0f;

	/** Extra damage multiplier per combo step (0.15 => 15% per step). 0 for abilities without steps. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ability|Melee|Damage")
	float ComboDamageBonusPerStep = 0.15f;

	/** WeaponComponent hit callback: hand the resolved hit to the damage executor. */
	void OnWeaponHit(const FMALHitResult& HitResult);

	/**
	 * Fill the damage request for one confirmed hit.
	 *
	 * The hook exists so a subclass can add its own fields (per-step scaling, target data, a
	 * different damage source) without copying the whole OnWeaponHit.
	 */
	virtual void BuildHitRequest(FMALWeaponHitRequest& OutRequest, const FMALHitResult& HitResult);

	// --- Knockback ---

	/**
	 * Whether a confirmed hit pushes the target away from the attacker.
	 *
	 * Applied by UMALDamageExecutor on the authority, so the distance means the same thing in
	 * single player and on a listen server. A target killed by the blow is not pushed: death
	 * disables its movement component, so the push would never play out.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ability|Melee|Knockback")
	bool bCanKnockback = false;

	/** How far the target is pushed, in world units, along the attacker -> target line. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ability|Melee|Knockback", meta = (ClampMin = "0.0", EditCondition = "bCanKnockback"))
	float KnockbackDistance = 250.0f;

	/**
	 * Seconds the push takes. The distance is fixed, so this only decides how violent it reads:
	 * short is an impact, long is a shove.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ability|Melee|Knockback", meta = (ClampMin = "0.05", EditCondition = "bCanKnockback"))
	float KnockbackDuration = 0.25f;

	// --- Hit feedback (hit-stop + camera shake) ---

	/** How long the attacker's local animation/movement freezes on a confirmed hit. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ability|Melee|HitFeedback", meta = (ClampMin = "0.0", ClampMax = "0.2"))
	float HitStopDuration = 0.06f;

	/** Camera shake started on the attacker's owning client. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ability|Melee|HitFeedback")
	TSubclassOf<UCameraShakeBase> HitCameraShakeClass;

	/**
	 * Cue fired at the impact point the moment a weapon hit is confirmed.
	 *
	 * This is the hook for hit audio. OnWeaponHit only runs on the server, so a sound played
	 * there would never reach a client; the cue system is what carries it across. Point this at
	 * a GameplayCueNotify_Burst Blueprint that owns the sound (and any impact VFX).
	 *
	 * Leave it empty to disable impact feedback.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ability|Melee|HitFeedback")
	FGameplayTag HitImpactCueTag;

	// --- Lunge ---

	/** Forward lunge distance per hit (fixed distance along the lunge direction) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ability|Melee|Lunge")
	float LungeDistance = 150.0f;

	/** Duration of the lunge movement */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ability|Melee|Lunge")
	float LungeDuration = 0.3f;

	/** Apply forward lunge via GAS root motion (MoveToForce) */
	void ApplyLunge();

	/** Called when the lunge root motion finishes (timed out or reached destination) */
	UFUNCTION()
	void OnLungeFinished();

	/** The currently running lunge task; ended before a new lunge starts. */
	UPROPERTY()
	TObjectPtr<UAbilityTask_ApplyRootMotionMoveToForce> ActiveLungeTask;

	// --- Deprecated ---

	/**
	 * Montage used to live on UMALAbility_Attack, the class every melee ability used to derive
	 * from. Kept on the shared base rather than on the combo class alone: assets saved before
	 * the split still carry this property, and a swing that no longer derives from the combo
	 * would otherwise load with no montage at all. PostLoad copies it into AbilityMontage.
	 */
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Set AbilityMontage instead."))
	TObjectPtr<UAnimMontage> AttackMontage;

	virtual void PostLoad() override;
};
