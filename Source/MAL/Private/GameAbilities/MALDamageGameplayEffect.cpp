// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameAbilities/MALDamageGameplayEffect.h"
#include "AbilitySystem/Attributes/MALAttributeSet.h"
#include "Tags/MALGameplayTags.h"

UMALDamageGameplayEffect::UMALDamageGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
	Modifier.Attribute = UMALAttributeSet::GetDamageAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;

	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = UMALGameplayTags::Get()->Data_Damage;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);

	UE_LOG(LogTemp, Verbose, TEXT("UMALDamageGameplayEffect: modifiers=%d attributeValid=%s tag=%s"),
		Modifiers.Num(),
		UMALAttributeSet::GetDamageAttribute().IsValid() ? TEXT("true") : TEXT("false"),
		*SetByCaller.DataTag.ToString());
}
