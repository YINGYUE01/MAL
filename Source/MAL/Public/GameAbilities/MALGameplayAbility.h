// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "InputActionValue.h"
#include "MALGameplayAbility.generated.h"
class UAbilityTask_PlayMontageAndWait;
class UAnimMontage;
class UCurveTable;
class UTexture2D;

/** Delegate for Blueprint input events */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMALAbilityInputDelegate, const FVector&, InputValue);

/**
 * Activation policy for abilities
 */
UENUM(BlueprintType)
enum class EMALAbilityActivationPolicy : uint8
{
	/** Activate on input Started event (e.g. Attack, Dash) */
	OnInputTriggered	UMETA(DisplayName = "On Input Triggered"),

	/**
	 * Active only while input is held, ended on Released (e.g. Defend). The ASC cancels the
	 * ability when the key comes up unless the ability already ended itself in InputReleased.
	 */
	WhileInputActive	UMETA(DisplayName = "While Input Active")
};

/**
 * UMALGameplayAbility
 * 
 * Base class for all MAL gameplay abilities.
 * Provides input tag binding, activation policy, and level scaling support.
 */
UCLASS()
class MAL_API UMALGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UMALGameplayAbility();

	/** Which input tag activates this ability (e.g. Input.Attack) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ability")
	FGameplayTag InputActivatedTag;

	/** How this ability responds to input */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ability")
	EMALAbilityActivationPolicy ActivationPolicy = EMALAbilityActivationPolicy::OnInputTriggered;

	/** Maximum level this ability can reach */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ability|Level")
	int32 MaxLevel = 1;

	/** Curve table for level scaling (damage, cooldown, etc.). Row names are looked up by GetLevelScaledValue. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ability|Level")
	TObjectPtr<UCurveTable> LevelScalingTable;

	/**
	 * Name shown on the ability bar. Lives on the ability rather than on a separate UI data asset
	 * so a slot only has to resolve its spec to describe itself.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ability|UI")
	FText DisplayName;

	/** Icon shown on the ability bar; left empty, the slot falls back to a plain frame. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ability|UI")
	TObjectPtr<UTexture2D> Icon;

	/**
	 * Evaluate a row from LevelScalingTable at the ability's current level.
	 * Returns DefaultValue when the table/row is missing.
	 */
	UFUNCTION(BlueprintPure, Category = "MAL|Ability|Level")
	float GetLevelScaledValue(FName RowName, float DefaultValue = 0.0f) const;

	// --- Shared setup ---

	/**
	 * One call for the three values every MAL ability repeats: the asset tag the ASC uses to
	 * identify it, the input that activates it, and the status tag it owns while running.
	 *
	 * Call it from the subclass constructor. The status tag also becomes an activation blocker
	 * when bBlocksSameStatusActivation is true, which is how two attacks refuse to overlap
	 * without each ability naming the tag again.
	 */
	void ConfigureAbility(
		FGameplayTag InAbilityTag,
		FGameplayTag InInputTag,
		FGameplayTag InStatusTag,
		bool bBlocksSameStatusActivation = true);

	// --- Montage ---

	/**
	 * Montage played while this ability is active.
	 *
	 * Empty is a valid configuration: the ability then does its work and ends without
	 * animating. Combo-style abilities override GetMontageStartSection to pick an entry
	 * section instead of touching this property.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ability|Animation")
	TObjectPtr<UAnimMontage> AbilityMontage;

	/**
	 * Montage PlayAbilityMontage actually plays.
	 *
	 * Defaults to AbilityMontage. Melee abilities override it so the equipped weapon can supply
	 * its own swings, which is what lets one combo ability drive a sword and a greatsword that
	 * animate differently.
	 */
	virtual UAnimMontage* GetAbilityMontage() const { return AbilityMontage; }

	/**
	 * Play AbilityMontage and route its lifecycle into OnAbilityMontageEnded.
	 *
	 * The engine task already handles replication, blend-out and cancellation; owning the
	 * three delegate bindings here is what stops every subclass from repeating them.
	 *
	 * @param StartSection  section to enter at; NAME_None starts at the beginning.
	 * @return the running task, or null when no montage is set or the task could not be
	 *         created (the caller decides whether that is fatal).
	 */
	UAbilityTask_PlayMontageAndWait* PlayAbilityMontage(FName StartSection = NAME_None);

	/** Section PlayAbilityMontage enters at. Combo abilities return their first combo section. */
	virtual FName GetMontageStartSection() const { return NAME_None; }

	/**
	 * The montage finished, was interrupted, or was cancelled.
	 *
	 * Default is "the montage is the ability": end on the spot, flagged as cancelled only when
	 * something actually cut it short. Subclasses whose effects outlive the animation override
	 * this instead of binding the task delegates themselves.
	 */
	virtual void OnAbilityMontageEnded(bool bInterrupted);

	// --- Costs ---

	/**
	 * CommitAbility, and end the ability when it fails.
	 *
	 * Committing is the first thing every activation does, so the failure path (end, flagged
	 * cancelled) is shared rather than copied.
	 *
	 * @return true when the cost and cooldown were paid and activation should continue.
	 */
	bool CommitAbilityOrEnd(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo);

	// --- Input forwarding interface ---

	/** Called when input is pressed (for charge mechanics, etc.) */
	virtual void OnInputPressed(const FInputActionValue& Value);

	/** Called when input is released */
	virtual void OnInputReleased(const FInputActionValue& Value);

	/** Broadcast when input is pressed during this ability (bind in Blueprint) */
	UPROPERTY(BlueprintAssignable, Category = "MAL|Ability|Input")
	FMALAbilityInputDelegate OnAbilityInputPressed;

	/** Broadcast when input is released during this ability (bind in Blueprint) */
	UPROPERTY(BlueprintAssignable, Category = "MAL|Ability|Input")
	FMALAbilityInputDelegate OnAbilityInputReleased;

	// --- Engine standard input interface ---

	/** Called by ASC when AbilitySpecInputPressed is triggered */
	virtual void InputPressed(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override;

	/** Called by ASC when AbilitySpecInputReleased is triggered */
	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override;

protected:
	// PlayAbilityMontage routes the task's three end states here. They stay separate from
	// OnAbilityMontageEnded because AddDynamic needs parameterless UFUNCTIONs.

	UFUNCTION()
	void HandleMontageCompleted();

	UFUNCTION()
	void HandleMontageInterrupted();

	UFUNCTION()
	void HandleMontageCancelled();
};
