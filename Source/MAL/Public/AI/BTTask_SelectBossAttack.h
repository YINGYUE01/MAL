// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BTTask_SelectBossAttack.generated.h"

/**
 * Asks the Boss selector for an attack and writes the chosen class to the blackboard.
 *
 * Selection is deliberately separate from activation: a selector miss should let the tree fall
 * through to repositioning, not consume an attack slot or start a cooldown.
 */
UCLASS()
class MAL_API UBTTask_SelectBossAttack : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_SelectBossAttack();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;

protected:
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetActorKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector SelectedAttackClassKey;

	/** Writes the selected entry's PreferredRange so the tree can move to its ideal distance. */
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector PreferredRangeKey;
};
