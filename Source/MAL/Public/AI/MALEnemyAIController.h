// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "MALEnemyAIController.generated.h"

/**
 * AMALEnemyAIController
 *
 * Empty shell. The previous perception / behavior tree / patrol implementation was removed
 * and the replacement AI layer is not written yet.
 *
 * The class is kept so existing enemy Blueprints still resolve their controller class; it
 * currently gives the possessed pawn no input, no target and no behavior.
 */
UCLASS()
class MAL_API AMALEnemyAIController : public AAIController
{
	GENERATED_BODY()

public:
	AMALEnemyAIController(const FObjectInitializer& ObjectInitializer);
};