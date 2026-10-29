// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameAbilities/MALAbility_StoneGolemAttack.h"

#include "Tags/MALGameplayTags.h"

UMALAbility_StoneGolemAttack::UMALAbility_StoneGolemAttack()
{
	const UMALGameplayTags* Tags = UMALGameplayTags::Get();

	// Same identity as the player's attack: the behaviour tree activates it by Ability.Attack and
	// checks Status.Attacking before starting another swing. The input tag is never used by AI,
	// but keeping it identical means one granted ability can be swapped for the other.
	ConfigureAbility(Tags->Ability_Attack, Tags->Input_LMB, Tags->Status_Attacking);

	// An AI swing plays out in full: the behaviour tree waits for the ability to end before it
	// decides what to do next, so an early release would cut the animation short.
	PostHitReleaseDelay = -1.f;
}
