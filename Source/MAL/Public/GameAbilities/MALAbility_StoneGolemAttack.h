// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameAbilities/MALAbility_MeleeAttack.h"
#include "MALAbility_StoneGolemAttack.generated.h"

/**
 * Stone Golem attack ability.
 *
 * A single AI swing. The hit window comes from the shared melee pipeline: MAL Weapon Collision
 * notifies on the montage open and close it, so the window always matches the frames where the
 * hammer actually moves. A timed window was tried first and had to guess at the animation,
 * which made the swing hit before the hammer came down and stop hitting before the impact.
 *
 * It used to derive from UMALAbility_Attack and override DisableWeaponCollision purely to stop
 * the combo's recovery release from cutting the montage short. Combo chaining now lives below
 * this class instead of above it, so the swing plays out to the end without any special case.
 */
UCLASS()
class MAL_API UMALAbility_StoneGolemAttack : public UMALAbility_MeleeAttack
{
	GENERATED_BODY()

public:
	UMALAbility_StoneGolemAttack();
};
