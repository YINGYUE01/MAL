// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/MALCharacterBase.h"
#include "MALEnemyCharacter.generated.h"

class UMALWorldWidgetComponent;

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

	virtual void BeginPlay() override;

	/** World-space health bar shown above the enemy. Set its WidgetClass on the blueprint. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MAL|UI")
	TObjectPtr<UMALWorldWidgetComponent> HealthBarWidget;
};
