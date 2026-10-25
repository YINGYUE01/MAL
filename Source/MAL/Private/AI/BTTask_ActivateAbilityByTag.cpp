// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/BTTask_ActivateAbilityByTag.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AI/MALEnemyAIController.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/MALCharacterBase.h"
#include "GameFramework/Pawn.h"
#include "Tags/MALGameplayTags.h"

UBTTask_ActivateAbilityByTag::UBTTask_ActivateAbilityByTag()
{
	NodeName = TEXT("Activate Ability By Tag");
	bNotifyTaskFinished = true;

	// The task owns a delegate binding and per-agent state, so every tree instance needs its
	// own node object instead of sharing the template.
	ForceInstancing(true);

	AbilityTag = UMALGameplayTags::Get()->Ability_Attack;
}

EBTNodeResult::Type UBTTask_ActivateAbilityByTag::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	APawn* Pawn = AIController ? AIController->GetPawn() : nullptr;
	if (!Pawn)
	{
		return EBTNodeResult::Failed;
	}

	AMALCharacterBase* Character = Cast<AMALCharacterBase>(Pawn);
	if (!Character || Character->IsDead())
	{
		return EBTNodeResult::Failed;
	}

	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard)
	{
		return EBTNodeResult::Failed;
	}

	AActor* Target = Cast<AActor>(Blackboard->GetValueAsObject(MALAIBlackboardKeys::TargetActor));
	if (!Target)
	{
		return EBTNodeResult::Failed;
	}

	if (const AMALCharacterBase* TargetCharacter = Cast<AMALCharacterBase>(Target))
	{
		if (TargetCharacter->IsDead())
		{
			return EBTNodeResult::Failed;
		}
	}

	const float Distance = FVector::Dist2D(Pawn->GetActorLocation(), Target->GetActorLocation());
	if (Distance > ResolveAttackRange(AIController))
	{
		return EBTNodeResult::Failed;
	}

	// Do not start a second swing through a wall or before the current one has ended.
	if (!AIController->LineOfSightTo(Target))
	{
		return EBTNodeResult::Failed;
	}

	UAbilitySystemComponent* AbilitySystem = Character->GetAbilitySystemComponent();
	if (!AbilitySystem || AbilitySystem->HasMatchingGameplayTag(UMALGameplayTags::Get()->Status_Attacking))
	{
		return EBTNodeResult::Failed;
	}

	if (!AbilityTag.IsValid())
	{
		return EBTNodeResult::Failed;
	}

	// Bound before activation: the failure paths below clear the focus through these handles,
	// and leaving them unset would leave the focus set below dangling.
	BoundController = AIController;
	BoundOwnerComp = &OwnerComp;

	AIController->StopMovement();

	if (bFaceTarget)
	{
		const FVector ToTarget = Target->GetActorLocation() - Pawn->GetActorLocation();
		const FRotator Facing = FRotator(0.0f, ToTarget.Rotation().Yaw, 0.0f);

		// UMALAbility_Attack::ApplyLunge reads the controller's control rotation, so both the
		// control yaw and the pawn facing have to point at the target before activation.
		AIController->SetControlRotation(Facing);
		Pawn->SetActorRotation(Facing);
		AIController->SetFocus(Target, EAIFocusPriority::Gameplay);
	}

	FGameplayTagContainer Tags;
	Tags.AddTag(AbilityTag);

	if (!AbilitySystem->TryActivateAbilitiesByTag(Tags, true))
	{
		ClearFocus();
		return EBTNodeResult::Failed;
	}

	// Bind after activation: an ability can end during TryActivateAbilitiesByTag when its
	// montage is missing, and binding first would call FinishLatentTask before the task is
	// actually in progress.
	BoundAbilitySystem = AbilitySystem;
	AbilityEndedHandle = AbilitySystem->OnAbilityEnded.AddUObject(this, &UBTTask_ActivateAbilityByTag::HandleAbilityEnded);

	UE_LOG(LogTemp, Warning, TEXT("BTTask_ActivateAbilityByTag: %s activated %s on %s (distance %.1f)"),
		*GetName(), *AbilityTag.ToString(), *Pawn->GetName(), Distance);

	// If the ability already ended in the same call, there is nothing to wait for. Let the
	// surrounding sequence continue and run its cooldown wait.
	if (!AbilitySystem->HasMatchingGameplayTag(UMALGameplayTags::Get()->Status_Attacking))
	{
		UnbindAbilityEnded();
		ClearFocus();
		return EBTNodeResult::Succeeded;
	}

	return EBTNodeResult::InProgress;
}

void UBTTask_ActivateAbilityByTag::OnTaskFinished(
	UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);

	UnbindAbilityEnded();
	ClearFocus();
	BoundController.Reset();
	BoundOwnerComp.Reset();
}

float UBTTask_ActivateAbilityByTag::ResolveAttackRange(const AAIController* AIController) const
{
	// One source of truth: a designer editing the AI controller's range should not have to
	// remember that the tree carries a second copy of the same number.
	if (const AMALEnemyAIController* MALController = Cast<AMALEnemyAIController>(AIController))
	{
		return MALController->GetAttackRange();
	}

	return AttackRange;
}

void UBTTask_ActivateAbilityByTag::HandleAbilityEnded(const FAbilityEndedData& Data)
{
	if (!Data.AbilityThatEnded || !Data.AbilityThatEnded->GetAssetTags().HasTag(AbilityTag))
	{
		return;
	}

	UnbindAbilityEnded();
	ClearFocus();

	if (UBehaviorTreeComponent* OwnerComp = BoundOwnerComp.Get())
	{
		FinishLatentTask(*OwnerComp, EBTNodeResult::Succeeded);
	}
}

void UBTTask_ActivateAbilityByTag::UnbindAbilityEnded()
{
	if (UAbilitySystemComponent* AbilitySystem = BoundAbilitySystem.Get())
	{
		if (AbilityEndedHandle.IsValid())
		{
			AbilitySystem->OnAbilityEnded.Remove(AbilityEndedHandle);
		}
	}

	AbilityEndedHandle.Reset();
	BoundAbilitySystem.Reset();
}

void UBTTask_ActivateAbilityByTag::ClearFocus()
{
	if (AAIController* AIController = BoundController.Get())
	{
		AIController->ClearFocus(EAIFocusPriority::Gameplay);
	}
}
