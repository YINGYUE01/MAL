// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "MALAbilityTask_PlayMontageAndWait.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMALPlayMontageAndWaitDelegate);

/**
 * UMALAbilityTask_PlayMontageAndWait
 *
 * Custom ability task that plays a montage and waits for completion.
 * Supports an optional SlotName parameter for upper/lower body animation splitting
 * (e.g. attack on upper body while locomotion continues on lower body).
 *
 * Slot routing is determined by the Montage asset's SlotAnimTracks configuration
 * in the editor. Set the montage's slot track to "UpperBody" for upper/lower body
 * animation splitting. The SlotName parameter here is for documentation/debug only.
 *
 * Usage in Blueprint: Create this task, bind OnCompleted/OnBlendOut/OnInterrupted/OnCancelled,
 * then call WaitForEnd.
 */
UCLASS()
class MAL_API UMALAbilityTask_PlayMontageAndWait : public UAbilityTask
{
	GENERATED_BODY()

public:
	UMALAbilityTask_PlayMontageAndWait();

	/** Create the task and configure parameters */
	UFUNCTION(BlueprintCallable, Category = "MAL|Ability|Tasks",
		meta = (DisplayName = "MAL Play Montage And Wait",
			HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UMALAbilityTask_PlayMontageAndWait* CreatePlayMontageAndWaitProxy(
		UGameplayAbility* OwningAbility,
		FName TaskInstanceName,
		UAnimMontage* MontageToPlay,
		float Rate = 1.0f,
		FName StartSection = NAME_None,
		bool bStopWhenAbilityEnds = true,
		float AnimRootMotionTranslationScale = 1.0f,
		float StartTimeSeconds = 0.0f,
		FName SlotName = NAME_None);

	/** Montage finished playing normally (reached end) */
	UPROPERTY(BlueprintAssignable)
	FMALPlayMontageAndWaitDelegate OnCompleted;

	/** Montage started blending out */
	UPROPERTY(BlueprintAssignable)
	FMALPlayMontageAndWaitDelegate OnBlendOut;

	/** Montage was interrupted by another montage */
	UPROPERTY(BlueprintAssignable)
	FMALPlayMontageAndWaitDelegate OnInterrupted;

	/** Ability was cancelled */
	UPROPERTY(BlueprintAssignable)
	FMALPlayMontageAndWaitDelegate OnCancelled;

	/** Whether the montage has finished blending out successfully */
	UFUNCTION(BlueprintCallable, Category = "MAL|Ability|Tasks")
	bool WasMontagePlayedSuccessfully() const;

	/** The montage that is currently playing */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Ability|Tasks")
	TObjectPtr<UAnimMontage> Montage;

	/** The slot name this montage is configured to play on */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Ability|Tasks")
	FName PlaySlotName;

protected:
	virtual void Activate() override;
	virtual void OnDestroy(bool AbilityEnded) override;

	/** Checks if the ability is playing a montage and stops that montage, returns true if stopped */
	bool StopPlayingMontage();

	UFUNCTION()
	void OnMontageBlendedIn(UAnimMontage* InMontage);

	UFUNCTION()
	void OnMontageBlendingOut(UAnimMontage* InMontage, bool bInterrupted);

	UFUNCTION()
	void OnGameplayAbilityCancelled();

	UFUNCTION()
	void OnMontageEnded(UAnimMontage* InMontage, bool bInterrupted);

	virtual FString GetDebugString() const override;

private:
	FOnMontageBlendedInEnded BlendedInDelegate;
	FOnMontageBlendingOutStarted BlendingOutDelegate;
	FOnMontageEnded MontageEndedDelegate;
	FDelegateHandle InterruptedHandle;

	UPROPERTY()
	float Rate;

	UPROPERTY()
	FName StartSection;

	UPROPERTY()
	float StartTimeSeconds;

	UPROPERTY()
	float AnimRootMotionTranslationScale;

	UPROPERTY()
	bool bStopWhenAbilityEnds;

	bool bMontageFinished;
	bool bAllowInterruptAfterBlendOut;
};
