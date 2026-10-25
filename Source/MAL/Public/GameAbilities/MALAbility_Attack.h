// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MALGameplayAbility.h"
#include "Interfaces/MALComboAttackInterface.h"
#include "Combat/MALHitResult.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "MALAbility_Attack.generated.h"

class UAnimMontage;
class UAbilityTask_ApplyRootMotionMoveToForce;
class UCameraShakeBase;
class UMALWeaponComponent;
class UMALComboComponent;

/**
 * UMALAbility_Attack
 *
 * Melee attack ability that plays a full-body montage and supports a multi-hit combo.
 * Combo chaining is driven by montage sections: each attack press during an open combo
 * window jumps to the next section. Per-hit forward lunge and hit events are triggered
 * from AnimNotifies placed in the montage (IMALComboAttackInterface + MAL.Event.Hit).
 */
UCLASS()
class MAL_API UMALAbility_Attack : public UMALGameplayAbility, public IMALComboAttackInterface
{
	GENERATED_BODY()

public:
	UMALAbility_Attack();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	//~ Begin IMALComboAttackInterface
	virtual void OpenComboWindow() override;
	virtual void CloseComboWindow() override;
	virtual void TriggerLunge() override;
	virtual void EnableWeaponCollision() override;
	virtual void DisableWeaponCollision() override;
	//~ End IMALComboAttackInterface

protected:
	/** The montage to play for this attack (plays on the montage's DefaultSlot, full body) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ability|Attack")
	TObjectPtr<UAnimMontage> AttackMontage;

	/** Combo section names in order (e.g. Combo1..Combo4). Index 0 is the entry section. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ability|Attack|Combo")
	TArray<FName> ComboSections;

	/** How long (seconds) an early combo press stays buffered before it is discarded. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ability|Attack|Combo", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ComboInputBufferWindow = 0.25f;

	/**
	 * Seconds between the hit window closing and the ability releasing movement.
	 *
	 * The tail of an attack montage is recovery the player is not going to watch. Holding
	 * Status.Attacking through it locks Move/Jump (see AMALPlayerController) and keeps
	 * ActivationBlockedTags up, so the next input feels unresponsive. Ending here lets the
	 * montage blend out while the character is already free again.
	 *
	 * Must stay at or above the combo window length, otherwise chaining gets cut off before the
	 * player can act on it. Set to 0 to release on the frame the hit window closes.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ability|Attack|Recovery", meta = (ClampMin = "0.0", ClampMax = "1.5"))
	float RecoveryReleaseDelay = 0.25f;

	/** Forward lunge distance per hit (fixed distance along the lunge direction) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ability|Attack|Lunge")
	float LungeDistance = 150.0f;

	/** Duration of the lunge movement */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ability|Attack|Lunge")
	float LungeDuration = 0.3f;

	// --- Combo state (owned by UMALComboComponent) ---

	/** Input hook: attack pressed while this ability is already active (routes a combo press). */
	virtual void OnInputPressed(const FInputActionValue& Value) override;

	/** Resolve the character's combo component (buffered input, step advancement, recovery). */
	UMALComboComponent* GetComboComponent() const;

	/** ComboComponent callback: jump the montage to the new section. */
	void OnComboAdvanced(int32 Step, FName Section);

	/** Current step derived from the montage's active section (authoritative on both sides). */
	int32 GetCurrentComboStepFromMontage() const;

	// --- Recovery release ---

	/** ComboComponent callback: end the ability so movement and the next attack are free again. */
	void ReleaseRecovery();

	// --- Lunge ---

	/** Apply forward lunge via GAS root motion (MoveToForce) */
	void ApplyLunge();

	/** Called when the lunge root motion finishes (timed out or reached destination) */
	UFUNCTION()
	void OnLungeFinished();

	/** The currently running lunge task; ended before a new lunge starts. */
	UPROPERTY()
	TObjectPtr<UAbilityTask_ApplyRootMotionMoveToForce> ActiveLungeTask;

	// --- Weapon collision ---

	/** Resolve the character's weapon component (owns the hit window and the overlap binding). */
	UMALWeaponComponent* GetWeaponComponent() const;

	/**
	 * True once this activation opened a hit window. Reset on activation and checked on end to
	 * warn when the montage carries no opening notify: that swing silently cannot hit anything,
	 * which is easy to mistake for a targeting or range problem.
	 */
	bool bHitWindowOpened = false;

	/** Damage effect class to apply on weapon hit; null falls back to UMALDamageGameplayEffect. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ability|Attack|Damage")
	TSubclassOf<class UGameplayEffect> DamageGEClass;

	/** Base damage magnitude applied to the damage GE */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ability|Attack|Damage")
	float BaseDamage = 10.0f;

	/** Extra damage multiplier per combo step (0.15 => 15% per step). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ability|Attack|Damage")
	float ComboDamageBonusPerStep = 0.15f;

	/** Name of the weapon collision component on the character (handed to UMALWeaponComponent). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ability|Attack|Damage")
	FName WeaponCollisionSocketName = TEXT("weapon_collision");

	// --- Hit feedback (hit-stop + camera shake) ---

	/** How long the attacker's local animation/movement freezes on a confirmed hit. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ability|Attack|HitFeedback", meta = (ClampMin = "0.0", ClampMax = "0.2"))
	float HitStopDuration = 0.06f;

	/** Camera shake started on the attacker's owning client. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ability|Attack|HitFeedback")
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
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ability|Attack|HitFeedback")
	FGameplayTag HitImpactCueTag;

	/** WeaponComponent hit callback: hand the resolved hit to the damage executor. */
	void OnWeaponHit(const FMALHitResult& HitResult);

	// --- Montage callbacks ---

	UFUNCTION()
	void OnMontageCompleted();

	UFUNCTION()
	void OnMontageBlendOut();

	UFUNCTION()
	void OnMontageInterrupted();

	UFUNCTION()
	void OnMontageCancelled();
};
