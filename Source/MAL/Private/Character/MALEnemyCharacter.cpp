// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/MALEnemyCharacter.h"

#include "Components/CapsuleComponent.h"
#include "Components/MALAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/MALAttributeSet.h"
#include "AI/MALEnemyAIController.h"
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

	// Possession is left to the engine and to Blueprint overrides; the controller class is an
	// empty shell until the replacement AI layer exists. Nothing here decides behaviour.
	AIControllerClass = AMALEnemyAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// No AI rotation driver is assumed, so the body turns toward its own velocity; otherwise a
	// moving enemy slides sideways while facing where it spawned.
	bUseControllerRotationYaw = false;

	// Enemies are placed in tight formations, so let the movement component push them apart
	// instead of letting capsules overlap into one silhouette.
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

void AMALEnemyCharacter::GrantAbilities()
{
	Super::GrantAbilities();

	// The base clears the character layer and refreshes the weapon; an enemy's own kit is this
	// blueprint-authored list. GrantAbilityIfMissing keeps a re-possession from duplicating specs.
	for (const TSubclassOf<UGameplayAbility>& AbilityClass : InitialAbilities)
	{
		GrantAbilityIfMissing(AbilityClass);
	}
}

bool AMALEnemyCharacter::TryActivateAbilityByTag(FGameplayTag AbilityTag)
{
	if (!AbilityTag.IsValid())
	{
		return false;
	}

	UMALAbilitySystemComponent* MALASC = GetMALAbilitySystemComponent();
	if (!MALASC)
	{
		return false;
	}

	FGameplayTagContainer AbilityTags;
	AbilityTags.AddTag(AbilityTag);

	return MALASC->TryActivateAbilitiesByTag(AbilityTags, /*bAllowRemoteActivation*/ true);
}