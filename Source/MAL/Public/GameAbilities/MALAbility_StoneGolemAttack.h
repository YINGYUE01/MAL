// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameAbilities/MALAbility_Attack.h"
#include "MALAbility_StoneGolemAttack.generated.h"

/**
 * Stone Golem attack ability.
 *
 * The hit window comes from the shared UMALAbility_Attack: MAL Weapon Collision notifies on the
 * montage open and close it, so the window always matches the frames where the hammer actually
 * moves. A timed window was tried first and had to guess at the animation, which made the swing
 * hit before the hammer came down and stop hitting before the impact.
 *
 * The only thing this subclass changes is how the window closes: see DisableWeaponCollision.
 */
UCLASS()
class MAL_API UMALAbility_StoneGolemAttack : public UMALAbility_Attack
{
	GENERATED_BODY()

public:
	UMALAbility_StoneGolemAttack();

	//~ Begin IMALComboAttackInterface
	virtual void DisableWeaponCollision() override;
	//~ End IMALComboAttackInterface
};
