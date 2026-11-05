// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameAbilities/MALGameplayAbility.h"
#include "MALAbility_HitReact.generated.h"

class UAnimMontage;

/**
 * UMALAbility_HitReact
 *
 * Shared, event-driven hit reaction. Triggered by MAL.Event.HitReact, which the server sends
 * from AMALCharacterBase::HandleDamageResolved after a non-lethal hit was resolved.
 *
 * One class serves players and enemies: the montage is read from the damaged character
 * (HitReactMontage), so a blueprint only has to point at its own animation. The reaction owns
 * the body for the duration of the montage: it cancels attacks/casts, and Status.HitReacting
 * blocks a second reaction from restarting it.
 *
 * Full-body by design: the montage is expected on the character's DefaultSlot.
 */
UCLASS()
class MAL_API UMALAbility_HitReact : public UMALGameplayAbility
{
	GENERATED_BODY()

public:
	UMALAbility_HitReact();

	/** Seconds after an activation during which further hits do not restart the reaction. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ability|HitReact", meta = (ClampMin = "0.0"))
	float ReactCooldown = 0.35f;

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	/** Montage comes from the damaged character, keeping one ability class for everyone. */
	virtual UAnimMontage* GetAbilityMontage() const override;

private:
	/** World time of the last accepted reaction, used for the internal re-trigger cooldown. */
	double LastReactTime = -1.0e9;
};
