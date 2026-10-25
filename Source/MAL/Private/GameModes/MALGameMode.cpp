// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameModes/MALGameMode.h"
#include "Character/MALCharacter.h"
#include "Player/MALPlayerController.h"
#include "Player/MALPlayerState.h"

AMALGameMode::AMALGameMode()
{
	// C++ defaults so a new map/GameMode cannot silently lose the GAS setup that lives on
	// the custom PlayerState. BP_GameMode may override these.
	DefaultPawnClass = AMALCharacter::StaticClass();
	PlayerControllerClass = AMALPlayerController::StaticClass();
	PlayerStateClass = AMALPlayerState::StaticClass();
}
