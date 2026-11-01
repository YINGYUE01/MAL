// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/MALEnemyCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/MALAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/MALAttributeSet.h"
#include "AI/MALEnemyAIController.h"
#include "AIController.h"
#include "Data/MALEnemyDefinition.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Tags/MALGameplayTags.h"
#include "UI/Widgets/MALWorldWidgetComponent.h"

AMALEnemyCharacter::AMALEnemyCharacter()
{
	bReplicates = true;

	// Enemies share one team so a swing that sweeps past a neighbour in the formation does not
	// damage it. Blueprints can still override the id for neutral or allied variants.
	TeamId = FGenericTeamId(MALTeam::Enemy);

	// Enemies have no PlayerState, so they own their GAS state directly.
	AbilitySystemComponent = CreateDefaultSubobject<UMALAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UMALAttributeSet>(TEXT("AttributeSet"));

	// Nameplate health bar. Attached to the capsule rather than the mesh so animations do not
	// make the bar jitter.
	HealthBarWidget = CreateDefaultSubobject<UMALWorldWidgetComponent>(TEXT("HealthBarWidget"));
	HealthBarWidget->SetupAttachment(GetCapsuleComponent());
	HealthBarWidget->SetRelativeLocation(FVector(0.0f, 0.0f, 110.0f));

	// Default AI possession for placed/spawned enemies; a BP subclass can override these.
	AIControllerClass = AMALEnemyAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// Nothing drives the controller rotation for an AI pawn, so the body has to turn toward its
	// own velocity; otherwise a patrolling enemy slides sideways while facing where it spawned.
	bUseControllerRotationYaw = false;

	// Enemies are placed in tight formations and chase the same target, so let the movement
	// component push them apart instead of letting capsules overlap into one silhouette.
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->bUseRVOAvoidance = true;
		Movement->SetAvoidanceGroup(1);
		Movement->SetGroupsToAvoid(1);
		Movement->AvoidanceConsiderationRadius = 220.0f;

		Movement->bOrientRotationToMovement = true;
		Movement->RotationRate = FRotator(0.0f, 320.0f, 0.0f);
	}

	// A cleared wave should not keep dead pawns replicating for the rest of the match.
	CorpseLifeSpan = 8.0f;

	// Character defaults to 100 Hz, but keep a floor so montage replication does not
	// fall back to the low adaptive rate while the enemy is idle.
	SetNetUpdateFrequency(100.0f);
	SetMinNetUpdateFrequency(30.0f);
}

void AMALEnemyCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Before BeginPlay: the mesh, capsule and hit volume have to be in place before anything
	// queries them (AI perception reads the capsule, animations read the mesh).
	ApplyDefinition();
	EnsureHitBox();

	UE_LOG(LogTemp, Log, TEXT("AMALEnemyCharacter %s: PostInitializeComponents done (auto hit box %d, definition %s)."),
		*GetName(), bAutoCreateHitBox ? 1 : 0, *GetNameSafe(Definition));
}

void AMALEnemyCharacter::ApplyDefinition()
{
	if (!Definition)
	{
		return;
	}

	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		if (USkeletalMesh* LoadedMesh = Definition->SkeletalMesh.LoadSynchronous())
		{
			MeshComponent->SetSkeletalMeshAsset(LoadedMesh);
		}

		if (UClass* LoadedAnimClass = Definition->AnimInstanceClass.LoadSynchronous())
		{
			MeshComponent->SetAnimInstanceClass(LoadedAnimClass);
		}

		MeshComponent->SetRelativeScale3D(Definition->MeshScale);
	}

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCapsuleSize(Definition->CapsuleRadius, Definition->CapsuleHalfHeight);
	}

	if (Definition->WalkSpeed > 0.0f)
	{
		if (UCharacterMovementComponent* Movement = GetCharacterMovement())
		{
			Movement->MaxWalkSpeed = Definition->WalkSpeed;
		}
	}
}

UBoxComponent* AMALEnemyCharacter::EnsureHitBox()
{
	// A Definition wins when it defines a volume; otherwise the character's own defaults keep a
	// plain enemy able to hit before any art decision has been made.
	const bool bWanted = Definition ? Definition->bCreateHitBox : bAutoCreateHitBox;
	UE_LOG(LogTemp, Log, TEXT("AMALEnemyCharacter %s: EnsureHitBox wanted=%d."), *GetName(), bWanted ? 1 : 0);
	if (!bWanted)
	{
		return nullptr;
	}

	const FName ComponentName = Definition ? Definition->HitBoxComponentName : DefaultHitBoxComponentName;
	const FName SocketName = Definition ? Definition->HitBoxAttachSocket : DefaultHitBoxAttachSocket;
	const FVector RequestedExtent = Definition ? Definition->HitBoxExtent : DefaultHitBoxExtent;
	const FVector Extent(FMath::Max(RequestedExtent.X, MinHitBoxExtent.X),
		FMath::Max(RequestedExtent.Y, MinHitBoxExtent.Y),
		FMath::Max(RequestedExtent.Z, MinHitBoxExtent.Z));
	const FVector Offset = Definition ? Definition->HitBoxRelativeLocation : DefaultHitBoxRelativeLocation;

	if (ComponentName.IsNone())
	{
		return nullptr;
	}

	USkeletalMeshComponent* MeshComponent = GetMesh();
	if (!MeshComponent)
	{
		return nullptr;
	}

	// A socket that does not exist on this mesh would put the volume at the mesh root, which
	// sits at the feet; the offset is what keeps it where the weapon actually is.
	const bool bHasSocket = !SocketName.IsNone() && MeshComponent->DoesSocketExist(SocketName);
	const FName AttachSocket = bHasSocket ? SocketName : NAME_None;
	const FVector EffectiveOffset = bHasSocket ? Offset : Offset + MissingSocketHitBoxOffset;
	if (!bHasSocket)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("AMALEnemyCharacter %s: socket '%s' missing on the mesh; hit box moved to %s."),
			*GetName(), *SocketName.ToString(), *EffectiveOffset.ToCompactString());
	}

	TArray<UActorComponent*> Components;
	GetComponents(Components);
	for (UActorComponent* Component : Components)
	{
		if (!Component || Component->GetFName() != ComponentName)
		{
			continue;
		}

		UBoxComponent* Existing = Cast<UBoxComponent>(Component);
		if (!Existing)
		{
			continue;
		}

		// Re-anchor: a box authored in a blueprint was placed for that blueprint's mesh, which
		// is not necessarily the one this enemy ends up wearing.
		Existing->AttachToComponent(MeshComponent,
			FAttachmentTransformRules::SnapToTargetNotIncludingScale, AttachSocket);
		Existing->SetBoxExtent(Extent);
		Existing->SetRelativeLocation(EffectiveOffset);
		return Existing;
	}

	UBoxComponent* HitBox = NewObject<UBoxComponent>(this, ComponentName);
	if (!HitBox)
	{
		UE_LOG(LogTemp, Warning, TEXT("AMALEnemyCharacter %s: could not create hit box '%s' (name already taken?)."),
			*GetName(), *ComponentName.ToString());
		return nullptr;
	}

	HitBox->SetupAttachment(MeshComponent, AttachSocket);
	HitBox->SetBoxExtent(Extent);
	HitBox->SetRelativeLocation(EffectiveOffset);

	// Off until the swing's notify enables it; the overlap binding is added per window.
	HitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HitBox->SetCollisionObjectType(ECC_WorldDynamic);
	HitBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	HitBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	HitBox->SetGenerateOverlapEvents(true);
	HitBox->SetHiddenInGame(true);
	HitBox->RegisterComponent();

	UE_LOG(LogTemp, Log, TEXT("AMALEnemyCharacter %s: created hit box '%s' (socket %s, extent %s)."),
		*GetName(), *ComponentName.ToString(), *AttachSocket.ToString(), *Extent.ToCompactString());

	return HitBox;
}

void AMALEnemyCharacter::GrantAbilities()
{
	Super::GrantAbilities();

	if (!HasAuthority() || !Definition || !Definition->AttackAbilityClass)
	{
		return;
	}

	UMALAbilitySystemComponent* MALASC = GetMALAbilitySystemComponent();
	if (!MALASC)
	{
		return;
	}

	// Tagged with the generic attack tag: that is what the behaviour tree activates by, and
	// what the swing's own owned tags are compared against while it runs.
	MALASC->GrantAbility(Definition->AttackAbilityClass, UMALGameplayTags::Get()->Ability_Attack, 1, NAME_None);
}

void AMALEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	// AutoPossessAI normally runs during PostInitializeComponents. PIE can copy a placed
	// enemy before that path has produced a controller, and a stale serialized instance can
	// leave a plain AAIController behind. Replace anything that is not our brain, then spawn
	// explicitly so the enemy can never silently stand still.
	if (HasAuthority() && AutoPossessAI != EAutoPossessAI::Disabled)
	{
		if (AController* Existing = GetController())
		{
			if (!Cast<AMALEnemyAIController>(Existing))
			{
				Existing->UnPossess();
				Existing->Destroy();
			}
		}

		if (!GetController())
		{
			// Respect a Blueprint AI Controller class; only fall back to the native class when
			// the pawn was left with the generic engine controller.
			if (!AIControllerClass || AIControllerClass == AAIController::StaticClass())
			{
				AIControllerClass = AMALEnemyAIController::StaticClass();
			}

			SpawnDefaultController();

			UE_LOG(LogTemp, Warning, TEXT("AMALEnemyCharacter::BeginPlay %s: spawned controller %s"),
				*GetName(), *GetNameSafe(GetController()));
		}
	}
}
