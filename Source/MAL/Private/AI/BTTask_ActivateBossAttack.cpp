// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/BTTask_ActivateBossAttack.h"

#include "AI/MALBossAttackSelectorComponent.h"
#include "AIController.h"
#include "GameFramework/Pawn.h"

UBTTask_ActivateBossAttack::UBTTask_ActivateBossAttack()
{
	NodeName = TEXT("Activate Boss Attack");
	bNotifyTick = true;
	bCreateNodeInstance = true;
}

EBTNodeResult::Type UBTTask_ActivateBossAttack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	APawn* Pawn = AIController ? AIController->GetPawn() : nullptr;
	UMALBossAttackSelectorComponent* Selector = Pawn ? Pawn->FindComponentByClass<UMALBossAttackSelectorComponent>() : nullptr;
	if (!Selector)
	{
		UE_LOG(LogTemp, Warning, TEXT("BTTask_ActivateBossAttack: %s has no UMALBossAttackSelectorComponent"),
			*GetNameSafe(Pawn));
		return EBTNodeResult::Failed;
	}

	if (!Selector->ActivateLastSelectedAttack())
	{
		return EBTNodeResult::Failed;
	}

	ElapsedTime = 0.0f;
	return EBTNodeResult::InProgress;
}

void UBTTask_ActivateBossAttack::TickTask(
	UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	APawn* Pawn = AIController ? AIController->GetPawn() : nullptr;
	UMALBossAttackSelectorComponent* Selector = Pawn ? Pawn->FindComponentByClass<UMALBossAttackSelectorComponent>() : nullptr;

	if (!Selector)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	if (!Selector->IsAttackInProgress())
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	ElapsedTime += DeltaSeconds;
	if (ElapsedTime >= MaxWaitTime)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("BTTask_ActivateBossAttack: %s exceeded %.1fs waiting for the ability to end; releasing the selector"),
			*GetNameSafe(Pawn), MaxWaitTime);

		// The ability watchdog should have ended it already; clearing the selector prevents the
		// Boss from being permanently stuck in "attack in progress".
		Selector->NotifyAttackEnded();
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
}

EBTNodeResult::Type UBTTask_ActivateBossAttack::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	APawn* Pawn = AIController ? AIController->GetPawn() : nullptr;
	if (UMALBossAttackSelectorComponent* Selector = Pawn ? Pawn->FindComponentByClass<UMALBossAttackSelectorComponent>() : nullptr)
	{
		if (Selector->IsAttackInProgress())
		{
			Selector->NotifyAttackEnded();
		}
	}

	return EBTNodeResult::Aborted;
}
