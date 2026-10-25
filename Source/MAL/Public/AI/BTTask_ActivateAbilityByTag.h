// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "GameplayTagContainer.h"
#include "BTTask_ActivateAbilityByTag.generated.h"

class AAIController;
class UAbilitySystemComponent;
class UBehaviorTreeComponent;
struct FAbilityEndedData;

/**
 * Activates a gameplay ability by tag when the blackboard target is inside attack range
 * and visible.
 *
 * The task stays InProgress until the activated ability ends, so the surrounding Sequence
 * can put a Wait node after it to create a clean attack cadence. It fails when there is no
 * target, the target is out of range/dead, or the ability is already active, which lets the
 * Selector fall through to its polling wait.
 */
UCLASS()
class MAL_API UBTTask_ActivateAbilityByTag : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_ActivateAbilityByTag();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;

	/** Ability to activate, matched against the ability's asset tags. */
	UPROPERTY(EditAnywhere, Category = "MAL|AI")
	FGameplayTag AbilityTag;

	/**
	 * Fallback range. The task prefers AMALEnemyAIController::GetAttackRange() so the chase and
	 * the swing cannot drift apart; this value only applies to other AI controllers.
	 */
	UPROPERTY(EditAnywhere, Category = "MAL|AI", meta = (ClampMin = "0.0"))
	float AttackRange = 300.0f;

	/** Rotate the pawn/control rotation toward the target before activating. */
	UPROPERTY(EditAnywhere, Category = "MAL|AI")
	bool bFaceTarget = true;

protected:
	void HandleAbilityEnded(const FAbilityEndedData& Data);
	void UnbindAbilityEnded();
	void ClearFocus();

	float ResolveAttackRange(const AAIController* AIController) const;

	TWeakObjectPtr<UAbilitySystemComponent> BoundAbilitySystem;
	TWeakObjectPtr<AAIController> BoundController;
	TWeakObjectPtr<UBehaviorTreeComponent> BoundOwnerComp;
	FDelegateHandle AbilityEndedHandle;
};
