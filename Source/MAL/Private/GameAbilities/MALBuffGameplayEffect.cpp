// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameAbilities/MALBuffGameplayEffect.h"

#include "AbilitySystem/Attributes/MALAttributeSet.h"
#include "GameplayEffect.h"
#include "Tags/MALGameplayTags.h"

UMALBuffGameplayEffect::UMALBuffGameplayEffect()
{
	// A buff is temporary by definition: it applies, counts down and then removes itself.
	// The 15s default is only a fallback for abilities that never override the spec duration.
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(15.f));

	// The magnitude comes from the ability, not the asset. Two abilities can share this GE
	// and still grant very different amounts.
	FSetByCallerFloat SetByCallerMagnitude;
	SetByCallerMagnitude.DataName = UMALGameplayTags::Get()->Data_DamageBonus.GetTagName();
	SetByCallerMagnitude.DataTag = UMALGameplayTags::Get()->Data_DamageBonus;

	FGameplayModifierInfo DamageBonusModifier;
	DamageBonusModifier.Attribute = UMALAttributeSet::GetDamageBonusAttribute();
	DamageBonusModifier.ModifierOp = EGameplayModOp::Additive;
	DamageBonusModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCallerMagnitude);
	Modifiers.Add(DamageBonusModifier);

	// Driven by the GE's own cue list so the aura appears the moment the effect lands and is
	// torn down the moment the effect stops, whatever the reason.
	GameplayCues.Add(FGameplayEffectCue(UMALGameplayTags::Get()->GameplayCue_Buff_DamageUp, 0.f, 0.f));

	// Casting again while the buff is up refreshes the timer instead of stacking a second
	// percentage point on top. One source (the caster's ASC) can only ever hold one stack.
	// StackingType is deprecated in favour of SetStackingType(), which only exists in the
	// editor; the CDO has to write the field directly so packaged builds keep the setting.
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	StackingType = EGameplayEffectStackingType::AggregateBySource;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	StackLimitCount = 1;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
	StackPeriodResetPolicy = EGameplayEffectStackingPeriodPolicy::ResetOnSuccessfulApplication;

	UE_LOG(LogTemp, Verbose, TEXT("UMALBuffGameplayEffect: modifiers=%d cues=%d"), Modifiers.Num(), GameplayCues.Num());
}
