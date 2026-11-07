// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_ActivateBossAttack.generated.h"

/**
 * Activates the entry selected by UBTTask_SelectBossAttack and stays InProgress until that
 * ability ends. The selector owns cooldowns and cadence, so this node must not return before
 * the ability is actually over.
 */
UCLASS()
class MAL_API UBTTask_ActivateBossAttack : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_ActivateBossAttack();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

protected:
	/** Safety net for an ability that somehow never reports its end. */
	UPROPERTY(EditAnywhere, Category = "Boss")
	float MaxWaitTime = 10.0f;

private:
	float ElapsedTime = 0.0f;
};
