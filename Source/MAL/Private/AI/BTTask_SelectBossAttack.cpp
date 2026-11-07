// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/BTTask_SelectBossAttack.h"

#include "AI/MALBossAttackSelectorComponent.h"
#include "AIController.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Class.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Pawn.h"

UBTTask_SelectBossAttack::UBTTask_SelectBossAttack()
{
	NodeName = TEXT("Select Boss Attack");

	TargetActorKey.AddObjectFilter(
		this, GET_MEMBER_NAME_CHECKED(UBTTask_SelectBossAttack, TargetActorKey), AActor::StaticClass());
	TargetActorKey.SelectedKeyName = TEXT("TargetActor");

	SelectedAttackClassKey.AddClassFilter(
		this, GET_MEMBER_NAME_CHECKED(UBTTask_SelectBossAttack, SelectedAttackClassKey), UGameplayAbility::StaticClass());
	SelectedAttackClassKey.SelectedKeyName = TEXT("SelectedBossAttackClass");
}

EBTNodeResult::Type UBTTask_SelectBossAttack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	APawn* Pawn = AIController ? AIController->GetPawn() : nullptr;
	UMALBossAttackSelectorComponent* Selector = Pawn ? Pawn->FindComponentByClass<UMALBossAttackSelectorComponent>() : nullptr;
	if (!Selector)
	{
		UE_LOG(LogTemp, Warning, TEXT("BTTask_SelectBossAttack: %s has no UMALBossAttackSelectorComponent"),
			*GetNameSafe(Pawn));
		return EBTNodeResult::Failed;
	}

	AActor* Target = nullptr;
	if (UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent())
	{
		if (TargetActorKey.IsSet())
		{
			Target = Cast<AActor>(Blackboard->GetValueAsObject(TargetActorKey.SelectedKeyName));
		}
	}

	if (!Target && AIController)
	{
		Target = AIController->GetFocusActor();
	}

	FMALBossAttackEntry SelectedAttack;
	FString Debug;
	if (!Selector->SelectAttack(Target, SelectedAttack, Debug))
	{
		UE_LOG(LogTemp, Verbose, TEXT("BTTask_SelectBossAttack: %s"), *Debug);
		return EBTNodeResult::Failed;
	}

	if (UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent())
	{
		if (SelectedAttackClassKey.IsSet())
		{
			Blackboard->SetValue<UBlackboardKeyType_Class>(
				SelectedAttackClassKey.GetSelectedKeyID(), SelectedAttack.AbilityClass.Get());
		}
	}

	return EBTNodeResult::Succeeded;
}

FString UBTTask_SelectBossAttack::GetStaticDescription() const
{
	return TEXT("Select a Boss attack from the selector component");
}
