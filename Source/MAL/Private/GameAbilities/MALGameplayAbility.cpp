// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameAbilities/MALGameplayAbility.h"
#include "Components/MALAbilitySystemComponent.h"
#include "Engine/CurveTable.h"
#include "Curves/RealCurve.h"

UMALGameplayAbility::UMALGameplayAbility()
{
	// Default settings for MAL abilities
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

float UMALGameplayAbility::GetLevelScaledValue(FName RowName, float DefaultValue) const
{
	if (LevelScalingTable && !RowName.IsNone())
	{
		if (const FRealCurve* Curve = LevelScalingTable->FindCurve(
			RowName, TEXT("UMALGameplayAbility::GetLevelScaledValue"), false))
		{
			return Curve->Eval(static_cast<float>(GetAbilityLevel()));
		}
	}

	return DefaultValue;
}

void UMALGameplayAbility::OnInputPressed(const FInputActionValue& Value)
{
	// FInputActionValue::Get<FVector>() works for Bool/Axis1D/Axis2D/Axis3D and preserves
	// the axes that are available, so Blueprint can inspect the real input value.
	OnAbilityInputPressed.Broadcast(Value.Get<FVector>());
}

void UMALGameplayAbility::OnInputReleased(const FInputActionValue& Value)
{
	OnAbilityInputReleased.Broadcast(Value.Get<FVector>());
}

void UMALGameplayAbility::InputPressed(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	FInputActionValue Value;
	if (const UMALAbilitySystemComponent* MALASC =
		Cast<UMALAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo()))
	{
		Value = MALASC->GetLastInputValue(InputActivatedTag);
	}

	OnInputPressed(Value);
}

void UMALGameplayAbility::InputReleased(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	FInputActionValue Value;
	if (const UMALAbilitySystemComponent* MALASC =
		Cast<UMALAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo()))
	{
		Value = MALASC->GetLastInputValue(InputActivatedTag);
	}

	OnInputReleased(Value);
}
