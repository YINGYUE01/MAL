// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MALEnemyDefinition.generated.h"

class UAnimInstance;
class UGameplayAbility;
class USkeletalMesh;

/**
 * UMALEnemyDefinition
 *
 * Everything that makes one enemy visually and mechanically itself, in one asset: the mesh,
 * the animation blueprint it runs, its body size, how fast it walks, the swing it owns, and
 * the volume that swing uses to hit.
 *
 * The point is swap cost. Without this, changing an enemy's look means editing its blueprint
 * (mesh, anim class, scale, capsule, hit box) and its ability asset (montage), and the two can
 * drift apart silently. With it, a new enemy is a data asset plus a blueprint that only names
 * it -- and a reskin never touches the ability at all.
 *
 * Attacking is deliberately expressed as an ability class rather than a montage: the montage,
 * its sections and its collision notifies all belong to the ability asset, and duplicating
 * them here would give two sources of truth for the same animation.
 */
UCLASS(BlueprintType)
class MAL_API UMALEnemyDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// --- Appearance ---

	/** Mesh the enemy is drawn with. Soft, so loading the definition does not load the art. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Enemy|Appearance")
	TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

	/**
	 * Animation blueprint the mesh runs.
	 *
	 * Must be built for the skeleton above: a mismatched one reads as the reference pose rather
	 * than as an error, which is why the mesh and this field live next to each other.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Enemy|Appearance")
	TSoftClassPtr<UAnimInstance> AnimInstanceClass;

	/** Uniform scale applied to the mesh. Large for a brute, small for a fast one. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Enemy|Appearance")
	FVector MeshScale = FVector::OneVector;

	/** Capsule radius; also what the AI keeps away from the player. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Enemy|Appearance", meta = (ClampMin = "10.0"))
	float CapsuleRadius = 42.0f;

	/** Capsule half height. The mesh origin sits at the capsule centre. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Enemy|Appearance", meta = (ClampMin = "20.0"))
	float CapsuleHalfHeight = 96.0f;

	/** Walk speed. Zero keeps whatever the character class authored. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Enemy|Appearance", meta = (ClampMin = "0.0"))
	float WalkSpeed = 0.0f;

	// --- Combat ---

	/**
	 * Ability the behavior tree activates through MAL.Ability.Attack.
	 *
	 * The ability asset owns the montage, its sections and the collision notifies, so swapping
	 * the swing is swapping this class -- the tree and the definition stay untouched.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Enemy|Combat")
	TSubclassOf<UGameplayAbility> AttackAbilityClass;

	/**
	 * Create the hit volume named below when the enemy has none.
	 *
	 * UMALWeaponComponent finds the volume by name, so an enemy authored without one swings
	 * through its target and never damages it. Creating it from here keeps that contract in
	 * one place instead of in every enemy blueprint.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Enemy|Combat")
	bool bCreateHitBox = true;

	/** Name the hit window looks up; must match UMALWeaponComponent::HitBoxComponentName. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Enemy|Combat", meta = (EditCondition = "bCreateHitBox"))
	FName HitBoxComponentName = TEXT("weapon_collision");

	/** Bone/socket the volume hangs off. Missing on the mesh falls back to the mesh root. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Enemy|Combat", meta = (EditCondition = "bCreateHitBox"))
	FName HitBoxAttachSocket = TEXT("weapon_r");

	/** Half-size of the volume. A greatsword sweeps a wider box than a dagger. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Enemy|Combat", meta = (EditCondition = "bCreateHitBox"))
	FVector HitBoxExtent = FVector(70.0f, 70.0f, 70.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Enemy|Combat", meta = (EditCondition = "bCreateHitBox"))
	FVector HitBoxRelativeLocation = FVector::ZeroVector;
};
