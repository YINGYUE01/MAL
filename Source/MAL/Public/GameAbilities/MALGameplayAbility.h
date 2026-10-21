// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "InputActionValue.h"
#include "MALGameplayAbility.generated.h"
class UCurveTable;

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

	/** Stay active while input is held, end on Released (e.g. Defend) */
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

	/** Curve table for level scaling (damage, cooldown, etc.) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ability|Level")
	TObjectPtr<UCurveTable> LevelScalingTable;

	/** Cooldown gameplay effect class */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ability|Cooldown")
	TSubclassOf<UGameplayEffect> CooldownGE;

	/** Cost gameplay effect class (placeholder, not yet used) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ability|Cost")
	TSubclassOf<UGameplayEffect> CostGE;

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
};
