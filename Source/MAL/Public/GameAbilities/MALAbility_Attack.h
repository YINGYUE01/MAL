// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameAbilities/MALAbility_MeleeAttack.h"
#include "MALAbility_Attack.generated.h"

class UMALComboComponent;

/**
 * UMALAbility_Attack
 *
 * Multi-hit combo on top of UMALAbility_MeleeAttack: each attack press during an open combo
 * window jumps the montage to the next section. Damage, knockback, lunge and the hit window
 * all come from the melee base; this class only owns chaining.
 *
 * Combo state lives on UMALComboComponent (on the character, not the ability), so the ability
 * stays instanced-and-reused while the state machine survives across swings.
 */
UCLASS()
class MAL_API UMALAbility_Attack : public UMALAbility_MeleeAttack
{
	GENERATED_BODY()

public:
	UMALAbility_Attack();

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
	virtual void OpenComboWindow() override;
	virtual void CloseComboWindow() override;
	virtual void EnableWeaponCollision() override;
	virtual void DisableWeaponCollision() override;
	//~ End IMALComboAttackInterface

protected:
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

	/** Entry section: the first configured combo section. */
	virtual FName GetMontageStartSection() const override;

	/** Hit window is stamped with the section currently playing, not a cached step. */
	virtual int32 GetHitComboStep() const override;

	/** Input hook: attack pressed while this ability is already active (routes a combo press). */
	virtual void OnInputPressed(const FInputActionValue& Value) override;

	/** Resolve the character's combo component (buffered input, step advancement, recovery). */
	UMALComboComponent* GetComboComponent() const;

	/** ComboComponent callback: jump the montage to the new section. */
	void OnComboAdvanced(int32 Step, FName Section);

	/** Current step derived from the montage's active section (authoritative on both sides). */
	int32 GetCurrentComboStepFromMontage() const;

	/** ComboComponent callback: end the ability so movement and the next attack are free again. */
	void ReleaseRecovery();
};
