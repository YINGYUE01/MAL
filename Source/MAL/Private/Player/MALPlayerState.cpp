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

	// Mixed: GameplayEffects 对 owner 复制（HUD/预测需要），对模拟代理只复制最小信息。
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	// 属性集作为 PlayerState 的默认子对象创建，
	// ASC 初始化时会自动扫描并纳管同一 Owner 上的所有 UAttributeSet。
	// 属性的网络复制由属性集自身的 ReplicatedUsing + GetLifetimeReplicatedProps 处理。
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
