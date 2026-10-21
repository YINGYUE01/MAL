// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/MALPlayerState.h"
#include "Components/MALAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/MALAttributeSet.h"

AMALPlayerState::AMALPlayerState()
{
	AbilitySystemComponent = CreateDefaultSubobject<UMALAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);

	// 属性集作为 PlayerState 的默认子对象创建，
	// ASC 初始化时会自动扫描并纳管同一 Owner 上的所有 UAttributeSet。
	// 属性的网络复制由 ASC 统一处理，无需单独 SetIsReplicated。
	HealthSet = CreateDefaultSubobject<UMALAttributeSet>(TEXT("HealthSet"));
}

UAbilitySystemComponent* AMALPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AMALPlayerState::BeginPlay()
{
	Super::BeginPlay();
	// InitAbilityActorInfo is called in AMALCharacter::PossessedBy
	// to ensure Avatar is correctly set before ability granting.
}
