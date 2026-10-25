// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/BTTask_PickPatrolPoint.h"

#include "AI/MALEnemyAIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"

UBTTask_PickPatrolPoint::UBTTask_PickPatrolPoint()
{
	NodeName = TEXT("Pick Patrol Point");
}

EBTNodeResult::Type UBTTask_PickPatrolPoint::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	const AMALEnemyAIController* MALController = Cast<AMALEnemyAIController>(OwnerComp.GetAIOwner());
	if (!MALController)
	{
		return EBTNodeResult::Failed;
	}

	// A target outranks patrol. Failing here is what keeps the two branches from trading path
	// requests: the selector simply moves on to the attack branch.
	if (MALController->GetCombatTarget())
	{
		return EBTNodeResult::Failed;
	}

	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard)
	{
		return EBTNodeResult::Failed;
	}

	// A missing key is a setup error, not a reason to walk to the world origin in silence.
	if (Blackboard->GetKeyID(MALAIBlackboardKeys::PatrolLocation) == FBlackboard::InvalidKey)
	{
		UE_LOG(LogTemp, Error,
			TEXT("UBTTask_PickPatrolPoint: blackboard has no 'PatrolLocation' vector key; add it to the Blackboard asset used by this tree"));
		return EBTNodeResult::Failed;
	}

	FVector Point;
	if (!MALController->PickPatrolPoint(Point))
	{
		return EBTNodeResult::Failed;
	}

	Blackboard->SetValueAsVector(MALAIBlackboardKeys::PatrolLocation, Point);
	return EBTNodeResult::Succeeded;
}
