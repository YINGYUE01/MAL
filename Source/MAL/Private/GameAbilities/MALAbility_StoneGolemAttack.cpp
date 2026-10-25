// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameAbilities/MALAbility_StoneGolemAttack.h"

#include "Combat/MALWeaponComponent.h"

UMALAbility_StoneGolemAttack::UMALAbility_StoneGolemAttack()
{
	// The montage owns the hit window timing; there is nothing to configure here.
}

void UMALAbility_StoneGolemAttack::DisableWeaponCollision()
{
	if (!IsActive())
	{
		return;
	}

	// Close the hit window only. The shared implementation also arms the combo's recovery
	// release, which ends the ability a fraction of a second later and cuts the montage before
	// its recovery tail plays. An AI swing has no follow-up input to cancel into, so the montage
	// should simply play out and end on completion.
	if (UMALWeaponComponent* WeaponComponent = GetWeaponComponent())
	{
		WeaponComponent->EndHitWindow();
	}
}
