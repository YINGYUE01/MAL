// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/MALCharacter.h"
#include "Player/MALPlayerState.h"

AMALCharacter::AMALCharacter()
{
	// Without an explicit team the friendly-fire filter cannot tell a teammate from an enemy, so
	// every player-controlled pawn starts on the player side. Blueprints can still override it.
	TeamId = FGenericTeamId(MALTeam::Player);
}

bool AMALCharacter::AreDefaultAttributesApplied() const
{
	if (const AMALPlayerState* MALPS = GetPlayerState<AMALPlayerState>())
	{
		return MALPS->AreDefaultAttributesApplied();
	}

	return Super::AreDefaultAttributesApplied();
}

void AMALCharacter::MarkDefaultAttributesApplied()
{
	if (AMALPlayerState* MALPS = GetPlayerState<AMALPlayerState>())
	{
		MALPS->MarkDefaultAttributesApplied();
		return;
	}

	Super::MarkDefaultAttributesApplied();
}
