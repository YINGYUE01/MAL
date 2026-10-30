// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/MALDamageExecutor.h"
#include "MALProjectile.generated.h"

class UProjectileMovementComponent;
class USphereComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * AMALProjectile
 *
 * One arrow in flight.
 *
 * Damage is resolved on the server when the sweep reports a blocking hit, through the same
 * UMALDamageExecutor entry point melee uses: friendly fire, corpses and immunity tags are
 * filtered there, so a projectile does not need its own copy of those rules. The request it
 * carries is built by the ability that fired it, which is where damage, cue and scaling live.
 *
 * Clients only see it fly: the actor replicates its movement, the hit test does not run there.
 *
 * Penetration is a count of extra targets. Zero stops on the first thing it damages, one passes
 * through that target and stops on the next. A hit that dealt no damage (a wall, a friend, a
 * corpse) always stops the arrow, otherwise it would sail through the level.
 */
UCLASS()
class MAL_API AMALProjectile : public AActor
{
	GENERATED_BODY()

public:
	AMALProjectile();

	/**
	 * Server only: hand over the payload built by the firing ability.
	 *
	 * @param InDamageRequest  everything the executor needs except the target and impact point,
	 *                         which are filled per hit.
	 * @param InPenetrations   extra targets the arrow may pass through after the first.
	 */
	void InitializeProjectile(const FMALWeaponHitRequest& InDamageRequest, int32 InPenetrations);

	/** Launch along the actor's forward axis at InSpeed. Zero or less keeps the class default. */
	void SetLaunchSpeed(float InSpeed);

	/** Mesh drawn for this arrow, plus its offset from the root (facing and scale). */
	void SetArrowVisual(UStaticMesh* InMesh, const FTransform& InOffset);

protected:
	virtual void BeginPlay() override;

	/** Sweep result. Bound on the authority only. */
	UFUNCTION()
	void HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	/** Sphere that sweeps the world. Small: an arrow hits what it visually touches. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MAL|Projectile")
	TObjectPtr<USphereComponent> CollisionComponent;

	/** Visual only, no collision. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MAL|Projectile")
	TObjectPtr<UStaticMeshComponent> ArrowMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MAL|Projectile")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	/** Seconds a stray arrow lives before it despawns. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Projectile", meta = (ClampMin = "0.1"))
	float LifeSeconds = 4.0f;

	/**
	 * Payload to resolve. Held as a reflected member so the actor and the ability it names stay
	 * alive while the arrow flies; an arrow outliving its shooter is not a case worth modelling.
	 */
	UPROPERTY()
	FMALWeaponHitRequest DamageRequest;

	/** Extra targets left to pass through. */
	int32 RemainingPenetrations = 0;

	/** Actors already hit by this arrow, so a pass-through does not damage the same body twice. */
	UPROPERTY(Transient)
	TSet<TObjectPtr<AActor>> HitActors;
};
