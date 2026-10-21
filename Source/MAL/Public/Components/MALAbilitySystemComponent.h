// Copyright Epic Games, Inc., All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"
#include "MALAbilitySystemComponent.generated.h"

class UMALAbilitySet;

UCLASS()
class MAL_API UMALAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UMALAbilitySystemComponent();

	virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) override;

	/** Grant all abilities from an AbilitySet, storing InputTag in DynamicAbilityTags */
	void GrantAbilitySet(const UMALAbilitySet* AbilitySet);

	/** Upgrade an ability by its InputTag (Server only) */
	bool UpgradeAbilityByInputTag(const FGameplayTag& InputTag);

	/** Buffer an input press event by tag (called from PC HandleInputActionEvent) */
	void AbilityInputTagPressed(const FGameplayTag& InputTag);

	/** Buffer an input release event by tag (called from PC HandleInputActionEvent) */
	void AbilityInputTagReleased(const FGameplayTag& InputTag);

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

	/** Override to invoke ReplicatedEvent for WaitInputPress */
	virtual void AbilitySpecInputPressed(FGameplayAbilitySpec& Spec) override;

	/** Override to invoke ReplicatedEvent for WaitInputRelease */
	virtual void AbilitySpecInputReleased(FGameplayAbilitySpec& Spec) override;

	/** Called when ability specs are replicated from server */
	virtual void OnRep_ActivateAbilities() override;

	/** Find a spec by its InputTag in DynamicAbilityTags */
	FGameplayAbilitySpec* FindAbilitySpecByInputTag(const FGameplayTag& InputTag);
};
