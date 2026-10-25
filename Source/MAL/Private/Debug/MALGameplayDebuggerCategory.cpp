// Copyright Epic Games, Inc. All Rights Reserved.

#include "Debug/MALGameplayDebuggerCategory.h"

#if WITH_GAMEPLAY_DEBUGGER

#include "AI/MALEnemyAIController.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/MALCharacterBase.h"
#include "GameFramework/Pawn.h"
#include "GenericTeamAgentInterface.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense.h"

FMALGameplayDebuggerCategory::FMALGameplayDebuggerCategory()
{
	bShowCategoryName = true;
	bShowOnlyWithDebugActor = true;

	// The page only needs to feel live; re-collecting and re-replicating every frame buys nothing
	// the eye can see and would skew the timings we are trying to inspect.
	CollectDataInterval = 0.1f;
}

TSharedRef<FGameplayDebuggerCategory> FMALGameplayDebuggerCategory::MakeInstance()
{
	return MakeShared<FMALGameplayDebuggerCategory>();
}

void FMALGameplayDebuggerCategory::CollectData(APlayerController* OwnerPC, AActor* DebugActor)
{
	APawn* EnemyPawn = Cast<APawn>(DebugActor);
	AMALEnemyAIController* EnemyController = EnemyPawn ? Cast<AMALEnemyAIController>(EnemyPawn->GetController()) : nullptr;
	if (!EnemyController)
	{
		AddTextLine(TEXT("{yellow}Selected actor is not an enemy: no AMALEnemyAIController"));
		return;
	}

	const AMALCharacterBase* EnemyCharacter = Cast<AMALCharacterBase>(EnemyPawn);

	AddTextLine(FString::Printf(TEXT("{white}%s {yellow}(%s)"), *EnemyPawn->GetName(), *EnemyController->GetName()));
	AddTextLine(FString::Printf(TEXT("State: alive=%d attacking=%d team=%d"),
		(EnemyCharacter && EnemyCharacter->IsDead()) ? 0 : 1,
		EnemyController->IsAttacking() ? 1 : 0,
		FGenericTeamId::GetTeamIdentifier(EnemyPawn).GetId()));

	if (const UBrainComponent* Brain = EnemyController->GetBrainComponent())
	{
		AddTextLine(FString::Printf(TEXT("Brain: running=%d"), Brain->IsRunning() ? 1 : 0));
	}

	UBlackboardComponent* Blackboard = EnemyController->GetBlackboardComponent();
	if (!Blackboard)
	{
		AddTextLine(TEXT("{red}Blackboard: missing, the tree reads nothing"));
		return;
	}

	AActor* Target = Cast<AActor>(Blackboard->GetValueAsObject(MALAIBlackboardKeys::TargetActor));
	const float DistanceToTarget = Target ? FVector::Dist(EnemyPawn->GetActorLocation(), Target->GetActorLocation()) : -1.0f;

	AddTextLine(FString::Printf(TEXT("Target: {yellow}%s {white}distance=%.0f inRange=%d"),
		*GetNameSafe(Target),
		DistanceToTarget,
		Blackboard->GetValueAsBool(MALAIBlackboardKeys::InAttackRange) ? 1 : 0));

	AddTextLine(FString::Printf(TEXT("Chase: pathfinding=%d stopRadius=%.0f"),
		EnemyController->IsUsingPathfindingChase() ? 1 : 0,
		EnemyController->GetChaseAcceptableRadius()));

	AddTextLine(FString::Printf(TEXT("Patrol: moving=%d radius=%.0f point=%s"),
		EnemyController->IsPatrolMoveActive() ? 1 : 0,
		EnemyController->GetPatrolRadius(),
		*Blackboard->GetValueAsVector(MALAIBlackboardKeys::PatrolLocation).ToCompactString()));

	AddTextLine(FString::Printf(TEXT("Home=%s LastKnown=%s"),
		*Blackboard->GetValueAsVector(MALAIBlackboardKeys::HomeLocation).ToCompactString(),
		*Blackboard->GetValueAsVector(MALAIBlackboardKeys::LastKnownLocation).ToCompactString()));

	UAIPerceptionComponent* Perception = EnemyController->GetPerceptionComponent();
	if (!Perception)
	{
		AddTextLine(TEXT("{red}Perception: component is null, sight never runs"));
		return;
	}

	AddTextLine(FString::Printf(TEXT("Sight: radius=%.0f lose=%.0f halfAngle=%.0f"),
		EnemyController->GetSightRadius(),
		EnemyController->GetLoseSightRadius(),
		EnemyController->GetPeripheralVisionAngleDegrees()));

	TArray<AActor*> PerceivedActors;
	Perception->GetCurrentlyPerceivedActors(nullptr, PerceivedActors);

	AddTextLine(FString::Printf(TEXT("Perception: active=%d listener=%d perceived=%d"),
		Perception->IsActive() ? 1 : 0,
		Perception->GetListenerId().IsValid() ? 1 : 0,
		PerceivedActors.Num()));

	for (const AActor* Perceived : PerceivedActors)
	{
		AddTextLine(FString::Printf(TEXT("  {green}%s"), *GetNameSafe(Perceived)));
	}
}

#endif // WITH_GAMEPLAY_DEBUGGER
