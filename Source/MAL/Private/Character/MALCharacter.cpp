// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/MALCharacter.h"
#include "Player/MALPlayerState.h"

AMALCharacter::AMALCharacter()
{
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
