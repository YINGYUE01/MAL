// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameAbilities/MALGameplayAbility.h"
#include "MALAbility_Dodge.generated.h"

/**
 * UMALAbility_Dodge
 *
 * A short burst of movement with the damage window closed.
 *
 * The impulse is the ability: no montage is required, so it works on a pawn whose dodge
 * animation has not been authored yet. When a montage is set it plays on top, but the ability
 * still ends on DodgeDuration -- the dodge length is a gameplay value, and letting a long
 * montage decide it would turn a dodge into a lockout.
 *
 * Status.Invulnerable is owned for the whole activation, which is what makes the dodge answer
 * an attack: the damage executor discards hits on a target holding that tag. It also
 * cancels attacks, so the dodge can be used as a way out of an over-committed swing.
 */
UCLASS()
class MAL_API UMALAbility_Dodge : public UMALGameplayAbility
{
	GENERATED_BODY()

public:
	UMALAbility_Dodge();

protected:
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

	/** Distance covered by the whole dodge, in centimetres. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Dodge", meta = (ClampMin = "0.0"))
	float DodgeDistance = 500.0f;

	/** Seconds the dodge lasts and, because the tag is owned for the activation, the i-frame window. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Dodge", meta = (ClampMin = "0.05"))
	float DodgeDuration = 0.25f;

	/** Extra height the impulse carries, so a dodge over a step does not clip into it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Dodge")
	bool bLiftWhileDodging = true;

private:
	/** Ends the ability when the dodge window closes, whatever the animation is doing. */
	void HandleDodgeEnd();

	FTimerHandle DodgeEndTimer;
};
