// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_PickPatrolPoint.generated.h"

/**
 * Writes a random patrol point around the enemy's spawn location into the blackboard.
 *
 * Movement deliberately lives in the tree: this task only supplies the destination, and a
 * MoveTo node after it does the walking. It fails when there is a combat target, so the
 * selector falls through to the attack branch instead of fighting the chase for path requests.
 */
UCLASS()
class MAL_API UBTTask_PickPatrolPoint : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_PickPatrolPoint();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
