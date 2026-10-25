// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "MALCooldownGameplayEffect.generated.h"

/**
 * UMALCooldownGameplayEffect
 *
 * A duration effect that carries the cooldown tag contract.
 *
 * UGameplayAbility::GetCooldownTimeRemainingAndDuration matches the ability's cooldown tags
 * against the effects running on the owner, so a cooldown effect that grants no tags reports zero
 * remaining time and the HUD never sees the sweep. Declaring the tags as a property here keeps
 * that requirement beside the duration instead of relying on every cooldown asset to remember a
 * target-tags component.
 */
UCLASS()
class MAL_API UMALCooldownGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UMALCooldownGameplayEffect();

	/** Granted to the owner for the whole duration; an ability's cooldown tags match against these. */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|Cooldown")
	FGameplayTagContainer CooldownTags;

#if WITH_EDITOR
	virtual void PostCDOCompiled(const FPostCDOCompiledContext& Context) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	virtual void PostLoad() override;

private:
	/** Mirror CooldownTags into the target-tags component, which is what the effect grants from. */
	void SyncGrantedTags();
};
