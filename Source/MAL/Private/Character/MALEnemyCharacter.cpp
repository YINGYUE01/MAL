// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/MALEnemyCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/MALAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/MALAttributeSet.h"
#include "AI/MALEnemyAIController.h"
#include "AIController.h"
#include "GameFramework/CharacterMovementComponent.h"
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
