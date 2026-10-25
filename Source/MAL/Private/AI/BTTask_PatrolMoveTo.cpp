// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/BTTask_PatrolMoveTo.h"

#include "AI/MALEnemyAIController.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "BrainComponent.h"
#include "Navigation/PathFollowingComponent.h"

UBTTask_PatrolMoveTo::UBTTask_PatrolMoveTo()
{
	NodeName = TEXT("Patrol Move");

	// Latent task: completion arrives as a move message, and the controller only learns the path
	// was released through OnTaskFinished, which the tree skips unless this flag is set. Without
	// it the patrol flag below stays set forever, and Tick keeps stopping the chase.
	bNotifyTick = false;
	bNotifyTaskFinished = true;
}

EBTNodeResult::Type UBTTask_PatrolMoveTo::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!AIController || !Blackboard)
	{
		return EBTNodeResult::Failed;
	}

	// A missing key is a setup error, not a reason to walk to the world origin in silence.
	if (Blackboard->GetKeyID(MALAIBlackboardKeys::PatrolLocation) == FBlackboard::InvalidKey)
	{
		UE_LOG(LogTemp, Error,
			TEXT("UBTTask_PatrolMoveTo: blackboard has no 'PatrolLocation' vector key; add it to the Blackboard asset used by this tree"));
		return EBTNodeResult::Failed;
	}

	FVector Destination;
	Blackboard->GetLocationFromEntry(MALAIBlackboardKeys::PatrolLocation, Destination);

	FAIMoveRequest MoveRequest(Destination);
	MoveRequest.SetAcceptanceRadius(AcceptableRadius);
	MoveRequest.SetUsePathfinding(true);
	MoveRequest.SetAllowPartialPath(true);
	MoveRequest.SetCanStrafe(false);
	MoveRequest.SetReachTestIncludesAgentRadius(true);

	FPathFollowingRequestResult Result = AIController->MoveTo(MoveRequest);

	if (Result.Code == EPathFollowingRequestResult::Failed)
	{
		// No navigation data: walk straight at the point instead of reporting a failure the
		// patrol branch would only repeat one pause later.
		FAIMoveRequest DirectRequest(Destination);
		DirectRequest.SetAcceptanceRadius(AcceptableRadius);
		DirectRequest.SetUsePathfinding(false);
		DirectRequest.SetProjectGoalLocation(false);
		DirectRequest.SetCanStrafe(false);
		DirectRequest.SetReachTestIncludesAgentRadius(true);

		Result = AIController->MoveTo(DirectRequest);
	}

	if (Result.Code == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		// Standing on the point counts as arrival; failing here would burn a pause for nothing.
		return EBTNodeResult::Succeeded;
	}

	if (Result.Code != EPathFollowingRequestResult::RequestSuccessful)
	{
		return EBTNodeResult::Failed;
	}

	// Tell the controller the path is ours so its Tick can take it back the moment a target shows up.
	AMALEnemyAIController* MALController = Cast<AMALEnemyAIController>(AIController);
	if (!MALController)
	{
		AIController->StopMovement();
		return EBTNodeResult::Failed;
	}

	MALController->SetPatrolMoveActive(true);

	FPatrolMoveTaskMemory* Memory = CastInstanceNodeMemory<FPatrolMoveTaskMemory>(NodeMemory);
	Memory->MoveRequestID = Result.MoveId;

	// Both the pathfinding and the direct request report completion through the same message,
	// so one wait covers either case.
	WaitForMessage(OwnerComp, UBrainComponent::AIMessage_MoveFinished, Result.MoveId);

	return EBTNodeResult::InProgress;
}

EBTNodeResult::Type UBTTask_PatrolMoveTo::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (AIController && AIController->GetMoveStatus() != EPathFollowingStatus::Idle)
	{
		AIController->StopMovement();
	}

	return EBTNodeResult::Aborted;
}

void UBTTask_PatrolMoveTo::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
	FPatrolMoveTaskMemory* Memory = CastInstanceNodeMemory<FPatrolMoveTaskMemory>(NodeMemory);
	Memory->MoveRequestID = FAIRequestID::InvalidRequest;

	if (AMALEnemyAIController* MALController = Cast<AMALEnemyAIController>(OwnerComp.GetAIOwner()))
	{
		MALController->SetPatrolMoveActive(false);
	}

	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
}
