// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayDebuggerCategory.h"

/**
 * Gameplay Debugger page for this project's enemies.
 *
 * The engine pages inspect the behavior tree and the perception system from the inside. What they
 * cannot show is the project's half of that contract: which blackboard keys are actually written,
 * whether a patrol leg still owns the path, and how the sight cone is tuned on this enemy.
 */
class FMALGameplayDebuggerCategory : public FGameplayDebuggerCategory
{
public:
	FMALGameplayDebuggerCategory();

	static TSharedRef<FGameplayDebuggerCategory> MakeInstance();

	virtual void CollectData(APlayerController* OwnerPC, AActor* DebugActor) override;
};
