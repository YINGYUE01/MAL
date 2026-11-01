// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/MALProjectile.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MALProjectile)

namespace
{
	/** Sweep radius of the arrow. Small on purpose: the visual is a thin shaft. */
	constexpr float ArrowCollisionRadius = 4.0f;

	/** Cruise speed used when neither the class default nor the ability asked for one. */
	constexpr float DefaultArrowSpeed = 6000.0f;
}

AMALProjectile::AMALProjectile()
{
	PrimaryActorTick.bCanEverTick = false;

	// The arrow flies on every machine but only damages on the server, so the actor and its
	// movement replicate while the hit test stays authority-only.
	bReplicates = true;
	SetReplicateMovement(true);

	// Spawned exactly at the muzzle, which can graze a wall for a single frame.
	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	CollisionComponent->InitSphereRadius(ArrowCollisionRadius);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionComponent->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionComponent->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	CollisionComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	CollisionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	CollisionComponent->SetGenerateOverlapEvents(true);
	// OnComponentHit only fires for swept movement when the component is told to report hits;
	// without this the arrow would pass through everything and rely on its lifetime to die.
	CollisionComponent->SetNotifyRigidBodyCollision(true);
	SetRootComponent(CollisionComponent);

	ArrowMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Arrow"));
	ArrowMesh->SetupAttachment(CollisionComponent);
	ArrowMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ArrowMesh->SetCastShadow(false);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionComponent;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->ProjectileGravityScale = 0.0f;
	ProjectileMovement->InitialSpeed = DefaultArrowSpeed;
	ProjectileMovement->MaxSpeed = DefaultArrowSpeed;
	// Launch direction is set in world space by SetLaunchSpeed; leaving the component in
	// local-space mode would make a zero initial velocity stay zero no matter what is assigned.
	ProjectileMovement->bInitialVelocityInLocalSpace = false;
	ProjectileMovement->bAutoActivate = true;
	ProjectileMovement->SetIsReplicated(true);
}

void AMALProjectile::InitializeProjectile(const FMALWeaponHitRequest& InDamageRequest, int32 InPenetrations)
{
	DamageRequest = InDamageRequest;
	RemainingPenetrations = FMath::Max(0, InPenetrations);

	// Clear whatever the template carried: the target and the point it was hit belong to the
	// hit this arrow is about to resolve, not to the shot that spawned it.
	DamageRequest.Hit = FMALHitResult();
}

void AMALProjectile::SetLaunchSpeed(float InSpeed)
{
	LaunchTowardDirection(GetActorForwardVector(), InSpeed);
}

void AMALProjectile::LaunchTowardDirection(const FVector& InDirection, float InSpeed)
{
	if (!ProjectileMovement || !CollisionComponent)
	{
		return;
	}

	// A blueprint that left InitialSpeed at zero, or an ability that passed zero, used to leave
	// the arrow standing still: the movement component had no velocity and nothing else ever
	// set one. Fall back until a real number is available.
	float Speed = InSpeed;
	if (Speed <= 0.0f)
	{
		Speed = ProjectileMovement->InitialSpeed > 0.0f ? ProjectileMovement->InitialSpeed : DefaultArrowSpeed;
	}

	FVector Direction = InDirection.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		Direction = GetActorForwardVector().GetSafeNormal();
	}
	if (Direction.IsNearlyZero())
	{
		Direction = FVector::ForwardVector;
	}

	// Point the body along the shot as well: the visual mesh is attached to it, so a velocity
	// that disagrees with the actor rotation draws an arrow flying sideways.
	SetActorRotation(Direction.Rotation());

	// Re-arm the movement component: a disabled or unregistered projectile (spawned after a
	// pool reset, or with Auto Activate cleared in the blueprint) ticks nothing and sits there.
	ProjectileMovement->SetUpdatedComponent(CollisionComponent);
	ProjectileMovement->bSimulationEnabled = true;
	ProjectileMovement->Activate(true);

	// Both values, because InitialSpeed is only read while the movement component starts up and
	// the velocity is what actually moves the arrow from here on.
	ProjectileMovement->InitialSpeed = Speed;
	ProjectileMovement->MaxSpeed = Speed;
	ProjectileMovement->bInitialVelocityInLocalSpace = false;
	ProjectileMovement->Velocity = Direction * Speed;
	ProjectileMovement->UpdateComponentVelocity();

	UE_LOG(LogTemp, Verbose, TEXT("%s: launched at %.0f cm/s toward %s."),
		*GetName(), Speed, *Direction.ToCompactString());
}

void AMALProjectile::SetArrowVisual(UStaticMesh* InMesh, const FTransform& InOffset)
{
	if (!ArrowMesh)
	{
		return;
	}

	ArrowMesh->SetStaticMesh(InMesh);
	ArrowMesh->SetRelativeTransform(InOffset);
}

void AMALProjectile::BeginPlay()
{
	Super::BeginPlay();

	SetLifeSpan(LifeSeconds);

	// The muzzle sits close enough to the shooter's capsule that the first sweep can graze it,
	// and an arrow that dies on its own bow never reaches anyone.
	if (AActor* Shooter = DamageRequest.Instigator)
	{
		CollisionComponent->IgnoreActorWhenMoving(Shooter, true);
	}

	if (!HasAuthority())
	{
		return;
	}

	// Same guarantee on the authority even when nothing called SetLaunchSpeed: an arrow with no
	// velocity is indistinguishable from one that never spawned.
	if (ProjectileMovement && ProjectileMovement->Velocity.IsNearlyZero())
	{
		SetLaunchSpeed(ProjectileMovement->InitialSpeed);
	}

	CollisionComponent->OnComponentHit.AddDynamic(this, &AMALProjectile::HandleHit);

	// Characters answer the projectile with Overlap rather than Block (the character capsule is
	// set up so melee volumes can report hits), so the blocking-hit callback above never runs
	// against them. Both paths feed the same resolver.
	CollisionComponent->OnComponentBeginOverlap.AddDynamic(this, &AMALProjectile::HandleBeginOverlap);
}

void AMALProjectile::HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!HasAuthority())
	{
		return;
	}

	// A blocking hit is either a body or level geometry. No body: nothing to damage, the arrow
	// simply stops against the world.
	const FVector ImpactPoint = Hit.bBlockingHit ? FVector(Hit.ImpactPoint) : GetActorLocation();

	if (!OtherActor || !DamageRequest.Instigator || OtherActor == DamageRequest.Instigator || OtherActor == GetOwner())
	{
		Destroy();
		return;
	}

	if (ResolveHit(OtherActor, ImpactPoint))
	{
		Destroy();
	}
}

void AMALProjectile::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority())
	{
		return;
	}

	const FVector ImpactPoint = bFromSweep
		? FVector(SweepResult.ImpactPoint)
		: (OtherActor ? OtherActor->GetActorLocation() : GetActorLocation());

	if (ResolveHit(OtherActor, ImpactPoint))
	{
		Destroy();
	}
}

bool AMALProjectile::ResolveHit(AActor* OtherActor, const FVector& ImpactPoint)
{
	AActor* Avatar = DamageRequest.Instigator;
	if (!Avatar || !OtherActor || OtherActor == Avatar || OtherActor == GetOwner())
	{
		return false;
	}

	// A pierced body is reported again on the way out; one hit per actor per arrow.
	if (HitActors.Contains(OtherActor))
	{
		return false;
	}
	HitActors.Add(OtherActor);

	// Verbose, and before the damage path: a wall carries no attribute set and the executor says
	// nothing about it, so without this line an arrow that simply hit the level looks like one
	// that never collided at all.
	UE_LOG(LogTemp, Verbose, TEXT("%s hit %s at %s."),
		*GetName(), *GetNameSafe(OtherActor), *ImpactPoint.ToCompactString());

	FMALWeaponHitRequest Request = DamageRequest;
	Request.Hit.HitActor = OtherActor;
	Request.Hit.HitLocation = FVector_NetQuantize(ImpactPoint);

	const float Damage = UMALDamageExecutor::ExecuteWeaponHit(Request);

	// Only a body that took damage is passed through. A wall, a corpse or a friendly absorbs the
	// arrow even at full penetration, which is what stops a shot from crossing the whole level.
	if (Damage <= 0.0f || RemainingPenetrations <= 0)
	{
		return true;
	}

	--RemainingPenetrations;
	CollisionComponent->IgnoreActorWhenMoving(OtherActor, true);
	return false;
}
