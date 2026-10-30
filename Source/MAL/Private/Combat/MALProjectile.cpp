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
	if (!ProjectileMovement || InSpeed <= 0.0f)
	{
		return;
	}

	// Both values, because InitialSpeed is only read while the movement component starts up and
	// the velocity is what actually moves the arrow from here on.
	ProjectileMovement->InitialSpeed = InSpeed;
	ProjectileMovement->MaxSpeed = InSpeed;
	ProjectileMovement->Velocity = GetActorForwardVector() * InSpeed;
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

	CollisionComponent->OnComponentHit.AddDynamic(this, &AMALProjectile::HandleHit);
}

void AMALProjectile::HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!HasAuthority())
	{
		return;
	}

	AActor* Avatar = DamageRequest.Instigator;
	if (!Avatar || !OtherActor || OtherActor == Avatar || OtherActor == GetOwner())
	{
		return;
	}

	// A pierced body is reported again on the way out; one hit per actor per arrow.
	if (HitActors.Contains(OtherActor))
	{
		return;
	}
	HitActors.Add(OtherActor);

	// Verbose, and before the damage path: a wall carries no attribute set and the executor says
	// nothing about it, so without this line an arrow that simply hit the level looks like one
	// that never collided at all.
	UE_LOG(LogTemp, Verbose, TEXT("%s hit %s at %s."),
		*GetName(), *GetNameSafe(OtherActor), *Hit.ImpactPoint.ToCompactString());

	FMALWeaponHitRequest Request = DamageRequest;
	Request.Hit.HitActor = OtherActor;

	const FVector ImpactPoint = Hit.bBlockingHit ? FVector(Hit.ImpactPoint) : GetActorLocation();
	Request.Hit.HitLocation = FVector_NetQuantize(ImpactPoint);

	const float Damage = UMALDamageExecutor::ExecuteWeaponHit(Request);

	// Only a body that took damage is passed through. A wall, a corpse or a friendly absorbs the
	// arrow even at full penetration, which is what stops a shot from crossing the whole level.
	if (Damage <= 0.0f || RemainingPenetrations <= 0)
	{
		Destroy();
		return;
	}

	--RemainingPenetrations;
	CollisionComponent->IgnoreActorWhenMoving(OtherActor, true);
}
