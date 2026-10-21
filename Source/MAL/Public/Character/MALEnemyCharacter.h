// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/MALCharacterBase.h"
#include "MALEnemyCharacter.generated.h"

/**
 * AMALEnemyCharacter
 *
 * Enemy character. Unlike the player, it has no PlayerState-owned ASC; the ASC and
 * AttributeSet are default subobjects on this character. AMALCharacterBase caches them
 * and initializes actor info with (this, this).
 */
UCLASS()
class MAL_API AMALEnemyCharacter : public AMALCharacterBase
{
	GENERATED_BODY()

public:
	AMALEnemyCharacter();
};
