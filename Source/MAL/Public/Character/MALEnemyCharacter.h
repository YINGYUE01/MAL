// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/MALCharacterBase.h"
#include "GameplayTagContainer.h"
#include "MALEnemyCharacter.generated.h"

class UMALWorldWidgetComponent;

/**
 * AMALEnemyCharacter
 *
 * Enemy character. Unlike the player, it has no PlayerState-owned ASC; the ASC and
 * AttributeSet are default subobjects on this character. AMALCharacterBase caches them
 * and initializes actor info with (this, this).
 *
 * Appearance and the weapon hit volume are authored on the blueprint: this class stays free
 * of content paths and of per-enemy configuration.
 */
UCLASS()
class MAL_API AMALEnemyCharacter : public AMALCharacterBase
{
	GENERATED_BODY()

public:
	AMALEnemyCharacter();

	virtual void BeginPlay() override;

	/** World-space health bar shown above the enemy. Set its WidgetClass on the blueprint. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MAL|UI")
	TObjectPtr<UMALWorldWidgetComponent> HealthBarWidget;

	// --- AI tuning ---
	//
	// These live on the pawn rather than the controller because every enemy shares one AI
	// controller class: a value on the controller would be one value for all of them.

	/** Range at which this enemy commits to a swing. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|AI|Combat", meta = (ClampMin = "0.0"))
	float AttackRange = 300.0f;

	/** Seconds the tree waits after a swing before the next attempt. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|AI|Combat", meta = (ClampMin = "0.0"))
	float AttackCooldown = 1.5f;

	/** Random seconds added to AttackCooldown so a pack does not swing in lockstep. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|AI|Combat", meta = (ClampMin = "0.0"))
	float AttackCooldownDeviation = 0.4f;

	/** Ability tag the behavior tree activates for this enemy's swing. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|AI|Combat")
	FGameplayTag AttackAbilityTag;

	/** Stop distance handed to the path following component while chasing. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|AI|Combat", meta = (ClampMin = "0.0"))
	float ChaseAcceptableRadius = 180.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|AI|Perception", meta = (ClampMin = "0.0"))
	float SightRadius = 2000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|AI|Perception", meta = (ClampMin = "0.0"))
	float LoseSightRadius = 2500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|AI|Perception", meta = (ClampMin = "0.0", ClampMax = "360.0"))
	float PeripheralVisionAngleDegrees = 90.0f;

	/** How far from where the pawn was possessed a patrol point may land. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|AI|Patrol", meta = (ClampMin = "0.0"))
	float PatrolRadius = 900.0f;

	/** Stop distance for a single patrol leg. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|AI|Patrol", meta = (ClampMin = "0.0"))
	float PatrolAcceptableRadius = 60.0f;

	/** Shortest pause between two patrol legs. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|AI|Patrol", meta = (ClampMin = "0.0"))
	float PatrolPauseMin = 1.0f;

	/** Longest pause between two patrol legs. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|AI|Patrol", meta = (ClampMin = "0.0"))
	float PatrolPauseMax = 3.0f;

	UFUNCTION(BlueprintPure, Category = "MAL|AI")
	FGameplayTag GetAttackAbilityTag() const { return AttackAbilityTag; }
};
