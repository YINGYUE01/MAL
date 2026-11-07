// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/MALEnemyCharacter.h"

#include "Components/CapsuleComponent.h"
#include "Components/MALAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/MALAttributeSet.h"
#include "AI/MALBossAttackSelectorComponent.h"
#include "AI/MALEnemyAIController.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Data/MALBossAttackSet.h"
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

	// Inert until a Boss Blueprint assigns BossAttackSet; keeping it on the shared class avoids a
	// second Boss character class just to host one component.
	BossAttackSelector = CreateDefaultSubobject<UMALBossAttackSelectorComponent>(TEXT("BossAttackSelector"));
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

bool AMALEnemyCharacter::TryActivateAbilityByClass(TSubclassOf<UGameplayAbility> AbilityClass)
{
	if (!AbilityClass)
	{
		return false;
	}

	UMALAbilitySystemComponent* MALASC = GetMALAbilitySystemComponent();
	if (!MALASC)
	{
		return false;
	}

	return MALASC->TryActivateAbilityByClass(AbilityClass, /*bAllowRemoteActivation*/ true);
}

bool AMALEnemyCharacter::TryActivateAbilityByTag(FGameplayTag AbilityTag)
{
	// Bosses use the existing BT attack task as a trigger, but the selector decides which move
	// actually runs. Intercepting here means the Blueprint tree needs no new node: its fixed
	// Enemy.Attack2 call becomes "ask the Boss selector", while regular enemies keep the old
	// tag activation path.
	if (BossAttackSet && BossAttackSelector)
	{
		AActor* Target = nullptr;
		if (const AAIController* AI = Cast<AAIController>(GetController()))
		{
			if (AActor* FocusActor = AI->GetFocusActor())
			{
				Target = FocusActor;
			}
			else if (const UBlackboardComponent* Blackboard = AI->GetBlackboardComponent())
			{
				Target = Cast<AActor>(Blackboard->GetValueAsObject(TEXT("TargetActor")));
			}
		}

		FMALBossAttackEntry SelectedAttack;
		FString Debug;
		if (!BossAttackSelector->SelectAttack(Target, SelectedAttack, Debug))
		{
			UE_LOG(LogTemp, Verbose, TEXT("AMALEnemyCharacter %s: Boss select failed: %s"),
				*GetName(), *Debug);
			return false;
		}

		return BossAttackSelector->ActivateLastSelectedAttack();
	}
	return false;
	//
	// if (!AbilityTag.IsValid())
	// {
	// 	return false;
	// }
	//
	// UMALAbilitySystemComponent* MALASC = GetMALAbilitySystemComponent();
	// if (!MALASC)
	// {
	// 	return false;
	// }
	//
	// FGameplayTagContainer AbilityTags;
	// AbilityTags.AddTag(AbilityTag);
	//
	// return MALASC->TryActivateAbilitiesByTag(AbilityTags, /*bAllowRemoteActivation*/ true);
}