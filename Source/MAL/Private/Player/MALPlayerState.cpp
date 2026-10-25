// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/MALPlayerState.h"
#include "Components/MALAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/MALAttributeSet.h"
#include "GameplayEffectTypes.h"

AMALPlayerState::AMALPlayerState()
{
	AbilitySystemComponent = CreateDefaultSubobject<UMALAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);

	// The ASC lives on the PlayerState, and GAS replicates the active montage
	// (FGameplayAbilityRepAnimMontage) through this actor. APlayerState defaults to
	// 1 Hz; simulated proxies would then receive montage start/position only once per
	// second, and OnRep_ReplicatedAnimMontage hard-corrects positions beyond 0.1s,
	// which looks delayed and choppy on remote clients.
	// Match the character/action-game update rate (Lyra does the same on ALyraPlayerState).
	SetNetUpdateFrequency(100.0f);
	SetMinNetUpdateFrequency(30.0f);

	// Mixed: GameplayEffects replicate to the owner (the HUD and prediction need them) while
	// simulated proxies only get the minimum.
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	// The attribute set is a default subobject of the PlayerState. ASC initialization adopts
	// every UAttributeSet it finds on the same owner; networking is handled by the set itself
	// through ReplicatedUsing + GetLifetimeReplicatedProps.
	HealthSet = CreateDefaultSubobject<UMALAttributeSet>(TEXT("HealthSet"));
}

UAbilitySystemComponent* AMALPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AMALPlayerState::BeginPlay()
{
	Super::BeginPlay();
	// InitAbilityActorInfo is called from AMALCharacterBase::PossessedBy (server) and
	// AMALCharacterBase::OnRep_PlayerState / OnRep_Controller (client).
}
