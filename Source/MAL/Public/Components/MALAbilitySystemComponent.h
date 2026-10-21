// Copyright Epic Games, Inc., All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"
#include "InputActionValue.h"
#include "MALAbilitySystemComponent.generated.h"

class UMALAbilitySet;

UCLASS()
class MAL_API UMALAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UMALAbilitySystemComponent();

	virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) override;

	/**
	 * Grant all abilities from an AbilitySet, storing the InputTag in DynamicAbilityTags.
	 * Returns the granted spec handles so the caller can remove them later.
	 * Server only.
	 */
	TArray<FGameplayAbilitySpecHandle> GrantAbilitySet(const UMALAbilitySet* AbilitySet);

	/** Remove every ability granted through GrantAbilitySet. Server only. */
	void ClearGrantedAbilities();

	/** Upgrade an ability by its InputTag (Server only) */
	bool UpgradeAbilityByInputTag(const FGameplayTag& InputTag);

	/** Buffer an input press event by tag (called from PC HandleInputActionEvent) */
	void AbilityInputTagPressed(const FGameplayTag& InputTag, const FInputActionValue& Value);

	/** Buffer an input release event by tag (called from PC HandleInputActionEvent) */
	void AbilityInputTagReleased(const FGameplayTag& InputTag, const FInputActionValue& Value);

	/** Last input value seen for a tag (zeroed FInputActionValue when none was recorded). */
	FInputActionValue GetLastInputValue(const FGameplayTag& InputTag) const;

	/** Process all buffered input (called from PC PostProcessInput) */
	void ProcessAbilityInput(float DeltaTime, bool bGamePaused);

	/** Clear all buffered input */
	void ClearAbilityInput();

protected:
	/** Buffered handles for input pressed this frame */
	TArray<FGameplayAbilitySpecHandle> InputPressedSpecHandles;

	/** Buffered handles for input released this frame */
	TArray<FGameplayAbilitySpecHandle> InputReleasedSpecHandles;

	/** Buffered handles for input held (active between press and release) */
	TArray<FGameplayAbilitySpecHandle> InputHeldSpecHandles;

	/** Last value per input tag, used by UMALGameplayAbility::InputPressed/Released. */
	TMap<FGameplayTag, FInputActionValue> LastInputValues;

	/** Handles granted through GrantAbilitySet. */
	TArray<FGameplayAbilitySpecHandle> GrantedAbilityHandles;

	/** Override to replicate direct input for abilities with bReplicateInputDirectly. */
	virtual void AbilitySpecInputPressed(FGameplayAbilitySpec& Spec) override;

	/** Override to replicate direct input for abilities with bReplicateInputDirectly. */
	virtual void AbilitySpecInputReleased(FGameplayAbilitySpec& Spec) override;

	/** Find a spec by its InputTag in DynamicAbilityTags */
	FGameplayAbilitySpec* FindAbilitySpecByInputTag(const FGameplayTag& InputTag);
};
