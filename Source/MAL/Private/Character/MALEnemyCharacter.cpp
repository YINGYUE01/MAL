// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/MALEnemyCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/MALAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/MALAttributeSet.h"
#include "AIController.h"
#include "UI/Widgets/MALWorldWidgetComponent.h"

AMALEnemyCharacter::AMALEnemyCharacter()
{
	bReplicates = true;

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
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// Character defaults to 100 Hz, but keep a floor so montage replication does not
	// fall back to the low adaptive rate while the enemy is idle.
	SetNetUpdateFrequency(100.0f);
	SetMinNetUpdateFrequency(30.0f);
}
