// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameAbilities/MALAbility_StoneGolemAttack.h"

#include "Tags/MALGameplayTags.h"

UMALAbility_StoneGolemAttack::UMALAbility_StoneGolemAttack()
{
	const UMALGameplayTags* Tags = UMALGameplayTags::Get();

	// AI-only identity. Status.Attacking is what makes the behaviour tree wait for the swing to
	// finish; the ability is activated by tag, never through input.
	ConfigureAbility(Tags->Ability_Enemy_Attack, FGameplayTag(), Tags->Status_Attacking);

	// An AI swing plays out in full: the behaviour tree waits for the ability to end before it
	// decides what to do next, so an early release would cut the animation short.
	PostHitReleaseDelay = -1.f;
}
