// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameAbilities/MALGameplayAbility.h"

UMALGameplayAbility::UMALGameplayAbility()
{
	// Default settings for MAL abilities
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UMALGameplayAbility::OnInputPressed(const FInputActionValue& Value)
{
	OnAbilityInputPressed.Broadcast(FVector(Value.Get<FVector2D>(), 0.0f));
	// Override in subclasses
}

void UMALGameplayAbility::OnInputReleased(const FInputActionValue& Value)
{
	OnAbilityInputReleased.Broadcast(FVector(Value.Get<FVector2D>(), 0.0f));
	// Override in subclasses
}

void UMALGameplayAbility::InputPressed(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	// Forward to the custom interface for subclass convenience
	FInputActionValue DummyValue;
	OnInputPressed(DummyValue);
}

void UMALGameplayAbility::InputReleased(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	// Forward to the custom interface for subclass convenience
	FInputActionValue DummyValue;
	OnInputReleased(DummyValue);
}
