// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/MALCharacterBase.h"
#include "MALCharacter.generated.h"

/**
 * AMALCharacter
 *
 * Player character. Keeps the existing class name so BP_MALCharacter does not need a
 * redirect/reparent; all shared GAS setup now lives in AMALCharacterBase.
 *
 * The real ASC/AttributeSet still live on AMALPlayerState. AMALCharacterBase caches
 * pointers to them and initializes actor info with (PlayerState, this).
 */
UCLASS()
class MAL_API AMALCharacter : public AMALCharacterBase
{
	GENERATED_BODY()

public:
	AMALCharacter();

protected:
	/** Players store the "default attributes applied" flag on the persistent PlayerState. */
	virtual bool AreDefaultAttributesApplied() const override;
	virtual void MarkDefaultAttributesApplied() override;
};
