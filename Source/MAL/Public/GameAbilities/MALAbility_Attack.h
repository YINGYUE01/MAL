// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MALGameplayAbility.h"
#include "Interfaces/MALComboAttackInterface.h"
#include "Combat/MALHitResult.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "MALAbility_Attack.generated.h"

class UAnimMontage;
class UPrimitiveComponent;
class UAbilityTask_ApplyRootMotionMoveToForce;

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

	/** Forward lunge distance per hit (fixed distance along the lunge direction) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ability|Attack|Lunge")
	float LungeDistance = 150.0f;

	/** Duration of the lunge movement */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ability|Attack|Lunge")
	float LungeDuration = 0.3f;

	// --- Combo runtime state (not serialized) ---

	/** Current combo step index (0-based into ComboSections) */
	int32 ComboStep = 0;

	/** True while the montage's combo window notify state is active */
	bool bComboWindowOpen = false;

	/** True if attack was pressed before the window opened; consumed when it opens */
	bool bComboInputBuffered = false;

	/** World time of the buffered press, used to expire stale inputs. */
	double ComboInputBufferTime = 0.0;

	// --- Combo input handling ---

	/** Input hook: attack pressed while this ability is already active */
	virtual void OnInputPressed(const FInputActionValue& Value) override;

	/** Route a combo press: advance if the window is open, otherwise buffer it */
	void HandleComboInput();

	/** Jump to the next combo section (if any remain) */
	void AdvanceCombo();

	/** Consume buffered input when a combo window opens (only if still fresh) */
	void TryConsumeBufferedInput();

	/** Reset combo counters/flags (on activate and end) */
	void ResetComboState();

	/** Current step derived from the montage's active section (authoritative on both sides). */
	int32 GetCurrentComboStepFromMontage() const;

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

	/** The weapon collision component (cached during EnableWeaponCollision) */
	TObjectPtr<UPrimitiveComponent> WeaponCollisionComp;

	/** Actors already hit during the current collision window (prevents multi-hit per swing). */
	TSet<AActor*> HitActorsThisSwing;

	/** Damage effect class to apply on weapon hit; null falls back to UMALDamageGameplayEffect. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ability|Attack|Damage")
	TSubclassOf<class UGameplayEffect> DamageGEClass;

	/** Base damage magnitude applied to the damage GE */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ability|Attack|Damage")
	float BaseDamage = 10.0f;

	/** Extra damage multiplier per combo step (0.15 => 15% per step). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ability|Attack|Damage")
	float ComboDamageBonusPerStep = 0.15f;

	/** Name of the weapon collision component to find on the character */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ability|Attack|Damage")
	FName WeaponCollisionSocketName = TEXT("weapon_collision");

	// --- Hit event (GameplayEvent decoupling) ---

	/** Called when the montage fires MAL.Event.Hit via SendGameplayEventToActor */
	UFUNCTION()
	void OnHitEventReceived(FGameplayEventData Payload);

	/** Handle a weapon overlap hit: construct effect context and apply damage GE */
	void OnWeaponHit(const FMALHitResult& HitResult);

	/** Overlap callback: construct hit result and route to OnWeaponHit */
	UFUNCTION()
	void OnWeaponOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

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
