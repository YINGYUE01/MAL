// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "GameplayTagContainer.h"
#include "Perception/AIPerceptionTypes.h"
#include "MALEnemyAIController.generated.h"

class UAISenseConfig_Sight;
class UBehaviorTree;
class UBlackboardData;
class AMALEnemyCharacter;
struct FMALDamageResolvedEvent;

/**
 * Blackboard keys shared by the native controller, the AI tasks and the asset generator.
 * An authored BehaviorTree must use exactly these names.
 */
namespace MALAIBlackboardKeys
{
	MAL_API extern const FName TargetActor;
	MAL_API extern const FName LastKnownLocation;
	MAL_API extern const FName HomeLocation;
	MAL_API extern const FName InAttackRange;
	MAL_API extern const FName PatrolLocation;
}

/**
 * Server-side brain for AMALEnemyCharacter.
 *
 * Owns sight perception, resolved-damage reaction, the Blackboard and a small Behavior Tree:
 *   Selector
 *     Sequence (attack): ActivateAbilityByTag -> Wait(cooldown + deviation)
 *     Sequence (patrol): PickPatrolPoint -> PatrolMoveTo -> Wait(pause)
 *     Wait(0.25)          <- re-arms the selector
 *
 * Patrol movement belongs to the tree: PickPatrolPoint only writes a destination to the
 * blackboard and PatrolMoveTo walks there. It fails while a target exists, so the two branches
 * never trade path requests. The chase is still driven from Tick because it has to react faster
 * than a tree re-evaluation and carries a straight-line fallback for maps without navigation.
 *
 * The tree is built in code so the AI works without a BehaviorTree asset. A designer-authored
 * asset assigned on the Blueprint (or at BehaviorTreeAssetPath) wins over the runtime tree.
 */
UCLASS()
class MAL_API AMALEnemyAIController : public AAIController
{
	GENERATED_BODY()

public:
	AMALEnemyAIController(const FObjectInitializer& ObjectInitializer);

	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Current blackboard target, or null when the enemy has no target. */
	UFUNCTION(BlueprintPure, Category = "MAL|AI")
	AActor* GetCombatTarget() const;

	/** True while the possessed pawn holds Status.Attacking. */
	UFUNCTION(BlueprintPure, Category = "MAL|AI")
	bool IsAttacking() const;

	/**
	 * Range at which this enemy commits to a swing, read from the possessed pawn.
	 *
	 * Every enemy shares one controller class, so the value has to live on the pawn: a value
	 * here would be one range for the whole roster.
	 */
	UFUNCTION(BlueprintPure, Category = "MAL|AI")
	float GetAttackRange() const;

	/**
	 * Destination source for UBTTask_PickPatrolPoint: a random point around where the pawn was
	 * possessed. Returns false when there is no pawn or the patrol radius is zero.
	 */
	bool PickPatrolPoint(FVector& OutPoint) const;

	/**
	 * Set by UBTTask_PatrolMoveTo while a patrol leg owns the path. Tick uses it to take the path
	 * back when a target appears; without this a chase would wait for the whole leg to finish.
	 */
	void SetPatrolMoveActive(bool bActive) { bPatrolMoveActive = bActive; }

	/**
	 * Read-only accessors for the Gameplay Debugger page. They expose tuning and state that live
	 * only on this class and are otherwise invisible until an enemy silently stops noticing
	 * the player.
	 */
	bool IsPatrolMoveActive() const { return bPatrolMoveActive; }
	bool IsUsingPathfindingChase() const { return bUsePathfindingChase; }
	float GetPatrolRadius() const;
	float GetChaseAcceptableRadius() const;
	float GetSightRadius() const;
	float GetLoseSightRadius() const;
	float GetPeripheralVisionAngleDegrees() const;

	/** Possessed enemy, or null when this brain is not driving an AMALEnemyCharacter. */
	AMALEnemyCharacter* GetEnemyCharacter() const;

	/** Tuning the asset generator bakes into BT_StoneGolem, read from the class defaults. */
	struct FMALAIDefaults
	{
		float AttackRange = 300.0f;
		float AttackCooldown = 1.5f;
		float AttackCooldownDeviation = 0.4f;
		float PatrolAcceptableRadius = 60.0f;
		float PatrolPauseMin = 1.0f;
		float PatrolPauseMax = 3.0f;
	};

	static FMALAIDefaults GetAssetDefaults();

protected:
	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	void ConfigurePerception();

	/** Subscribe to resolved damage so a hit from behind still pulls the enemy into the fight. */
	void BindCombatEvents();
	void HandleDamageResolved(const FMALDamageResolvedEvent& Event);

	void StartEnemyBehavior();
	UBlackboardData* BuildRuntimeBlackboard();
	UBehaviorTree* BuildRuntimeBehaviorTree();

	/** Path-following chase; drops to direct movement when the map has no navigation data. */
	void UpdateChase(AActor* Target);

	/** Face the target with the control rotation so the lunge and the body agree. */
	void FaceTarget(APawn* MyPawn, AActor* Target);

	bool RequestMoveToActor(AActor* Goal);

	/** True when NewTarget should take over from CurrentTarget. */
	bool ShouldSwitchTarget(AActor* CurrentTarget, AActor* NewTarget) const;

	bool IsWithinAttackRange(const APawn* MyPawn, const AActor* Target) const;

	/** Optional authored tree. When null the controller loads BehaviorTreeAssetPath. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|AI")
	TObjectPtr<UBehaviorTree> BehaviorTreeAsset;

	/** Used when no asset was assigned in a Blueprint; keeps native-placed enemies working. */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|AI")
	FString BehaviorTreeAssetPath = TEXT("/Game/Blueprint/AI/BT_StoneGolem.BT_StoneGolem");

	UPROPERTY(Transient)
	TObjectPtr<UBehaviorTree> RuntimeBehaviorTree;

	UPROPERTY(Transient)
	TObjectPtr<UBlackboardData> RuntimeBlackboard;

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	/** Cleared permanently the first time a move request reports no navigation data. */
	bool bUsePathfindingChase = true;

	/** Where the pawn was possessed; the center of the patrol area. */
	FVector HomeLocation = FVector::ZeroVector;

	/** True while a tree patrol move is running; see Tick. */
	bool bPatrolMoveActive = false;

	/** Seconds between perception status log lines; diagnostic aid for a sight that never fires. */
	float LastPerceptionLogTime = 0.0f;

	bool bCombatEventsBound = false;
};
