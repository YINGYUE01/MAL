// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AITypes.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_PatrolMoveTo.generated.h"

/** Latent move request bookkeeping. */
struct FPatrolMoveTaskMemory
{
	FAIRequestID MoveRequestID;
};

/**
 * Walks to the PatrolLocation blackboard key.
 *
 * A plain MoveTo node reports Failed on a map without navigation data, which leaves the enemy
 * standing exactly where the patrol problem started. This node retries the same destination as
 * a direct move in that case, so patrolling works before a NavMesh exists.
 */
UCLASS()
class MAL_API UBTTask_PatrolMoveTo : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_PatrolMoveTo();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;

	virtual uint16 GetInstanceMemorySize() const override { return sizeof(FPatrolMoveTaskMemory); }

	/** Stop distance; the runtime tree copies this from the controller's PatrolAcceptableRadius. */
	UPROPERTY(Category = "Patrol", EditAnywhere, meta = (ClampMin = "0.0", UIMin = "0.0"))
	float AcceptableRadius = 60.0f;
};
