// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/Execution/MALDamageExecutionCalculation.h"

#include "AbilitySystem/Attributes/MALAttributeSet.h"
#include "AbilitySystem/MALGameplayEffectContext.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "Tags/MALGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MALDamageExecutionCalculation)

namespace
{
	/** Attribute capture definitions shared by every instance (CDO included). */
	struct FMALDamageStatics
	{
		DECLARE_ATTRIBUTE_CAPTUREDEF(PhysicalAttack);
		DECLARE_ATTRIBUTE_CAPTUREDEF(MagicAttack);
		DECLARE_ATTRIBUTE_CAPTUREDEF(CritChance);
		DECLARE_ATTRIBUTE_CAPTUREDEF(CritDamage);
		DECLARE_ATTRIBUTE_CAPTUREDEF(PhysicalDefense);
		DECLARE_ATTRIBUTE_CAPTUREDEF(MagicDefense);

		FMALDamageStatics()
		{
			// Attack/crit stats live on the instigator, defenses on the target.
			DEFINE_ATTRIBUTE_CAPTUREDEF(UMALAttributeSet, PhysicalAttack, Source, false);
			DEFINE_ATTRIBUTE_CAPTUREDEF(UMALAttributeSet, MagicAttack, Source, false);
			DEFINE_ATTRIBUTE_CAPTUREDEF(UMALAttributeSet, CritChance, Source, false);
			DEFINE_ATTRIBUTE_CAPTUREDEF(UMALAttributeSet, CritDamage, Source, false);
			DEFINE_ATTRIBUTE_CAPTUREDEF(UMALAttributeSet, PhysicalDefense, Target, false);
			DEFINE_ATTRIBUTE_CAPTUREDEF(UMALAttributeSet, MagicDefense, Target, false);
		}
	};

	const FMALDamageStatics& DamageStatics()
	{
		static FMALDamageStatics Statics;
		return Statics;
	}
}

UMALDamageExecutionCalculation::UMALDamageExecutionCalculation()
{
	// Declare every captured attribute up front so the GE asset can preview them and
	// the aggregator captures them before Execute runs.
	RelevantAttributesToCapture.Add(DamageStatics().PhysicalAttackDef);
	RelevantAttributesToCapture.Add(DamageStatics().MagicAttackDef);
	RelevantAttributesToCapture.Add(DamageStatics().CritChanceDef);
	RelevantAttributesToCapture.Add(DamageStatics().CritDamageDef);
	RelevantAttributesToCapture.Add(DamageStatics().PhysicalDefenseDef);
	RelevantAttributesToCapture.Add(DamageStatics().MagicDefenseDef);
}

bool UMALDamageExecutionCalculation::IsMagicDamageSpec(const FGameplayEffectSpec& Spec)
{
	const UMALGameplayTags* Tags = UMALGameplayTags::Get();

	// Tags added by the ability at spec creation (AddDynamicAssetTag) win...
	if (Spec.GetDynamicAssetTags().HasTag(Tags->Damage_Type_Magic))
	{
		return true;
	}

	// ...otherwise fall back to the tags baked into the GE asset.
	if (const FGameplayTagContainer* SourceTags = Spec.CapturedSourceTags.GetAggregatedTags())
	{
		return SourceTags->HasTag(Tags->Damage_Type_Magic);
	}

	return false;
}

void UMALDamageExecutionCalculation::Execute_Implementation(
	const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

	// Useful when diagnosing a damage GE that silently produces no output: if this line is
	// missing, the GE's Executions array does not reference this calculation class.
	{
		const UAbilitySystemComponent* SourceASC = ExecutionParams.GetSourceAbilitySystemComponent();
		const UAbilitySystemComponent* TargetASC = ExecutionParams.GetTargetAbilitySystemComponent();
		UE_LOG(LogTemp, Verbose, TEXT("MALDamageExecution: Execute for %s | source=%s(%d sets) target=%s(%d sets)"),
			*GetNameSafe(Spec.Def),
			*GetNameSafe(SourceASC ? SourceASC->GetAvatarActor() : nullptr),
			SourceASC ? SourceASC->GetSpawnedAttributes().Num() : -1,
			*GetNameSafe(TargetASC ? TargetASC->GetAvatarActor() : nullptr),
			TargetASC ? TargetASC->GetSpawnedAttributes().Num() : -1);
	}

	FAggregatorEvaluateParameters EvaluateParameters;
	EvaluateParameters.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	EvaluateParameters.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	// ---- 1. Pick the damage channel ------------------------------------------------
	const bool bMagicDamage = IsMagicDamageSpec(Spec);
	const FGameplayEffectAttributeCaptureDefinition& AttackDef =
		bMagicDamage ? DamageStatics().MagicAttackDef : DamageStatics().PhysicalAttackDef;
	const FGameplayEffectAttributeCaptureDefinition& DefenseDef =
		bMagicDamage ? DamageStatics().MagicDefenseDef : DamageStatics().PhysicalDefenseDef;

	float Attack = 0.f;
	float Defense = 0.f;
	float CritChance = 0.f;
	float CritDamage = 0.f;

	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(AttackDef, EvaluateParameters, Attack);
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DefenseDef, EvaluateParameters, Defense);
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().CritChanceDef, EvaluateParameters, CritChance);
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().CritDamageDef, EvaluateParameters, CritDamage);

	// ---- 2. Base damage comes from the ability via SetByCaller ---------------------
	const float BaseDamage = Spec.GetSetByCallerMagnitude(UMALGameplayTags::Get()->Data_Damage, /*WarnIfNotFound*/ false, 0.f);
	if (BaseDamage <= 0.f)
	{
		UE_LOG(LogTemp, Verbose,
			TEXT("MALDamageExecution: %s has no SetByCaller(%s) magnitude, skipping."),
			*GetNameSafe(Spec.Def), *UMALGameplayTags::Get()->Data_Damage.ToString());
		return;
	}

	// ---- 3. Attack scaling and defense mitigation ---------------------------------
	Attack = FMath::Max(0.f, Attack);
	Defense = FMath::Max(0.f, Defense);

	const float AttackScale = 1.f + Attack / FMath::Max(AttackScalingDivisor, KINDA_SMALL_NUMBER);
	const float DefenseScale = DefenseScalingConstant / (DefenseScalingConstant + Defense);

	// ---- 4. Critical roll (server authoritative) ----------------------------------
	bool bCriticalHit = false;
	if (bAllowCriticalHits && CritChance > 0.f)
	{
		bCriticalHit = FMath::FRandRange(0.f, 100.f) < CritChance;
	}

	float FinalDamage = BaseDamage * AttackScale * DefenseScale;
	if (bCriticalHit)
	{
		FinalDamage *= FMath::Max(0.f, CritDamage) / 100.f;
	}
	FinalDamage = FMath::Max(0.f, FinalDamage);

	// ---- 5. Publish the result on the context for presentation ---------------------
	FGameplayEffectContextHandle Context = Spec.GetContext();
	if (FGameplayEffectContext* RawContext = Context.Get())
	{
		if (RawContext->GetScriptStruct() == FMALGameplayEffectContext::StaticStruct())
		{
			FMALGameplayEffectContext* MALContext = static_cast<FMALGameplayEffectContext*>(RawContext);
			MALContext->SetIsCriticalHit(bCriticalHit);
			MALContext->SetIsMagicDamage(bMagicDamage);
		}
	}

	UE_LOG(LogTemp, Verbose,
		TEXT("MALDamageExecution: %s -> %s | channel=%s base=%.1f atk=%.1f def=%.1f crit=%s final=%.1f"),
		*GetNameSafe(Spec.GetContext().GetInstigator()),
		*GetNameSafe(Spec.GetContext().GetEffectCauser()),
		bMagicDamage ? TEXT("Magic") : TEXT("Physical"),
		BaseDamage, Attack, Defense,
		bCriticalHit ? TEXT("yes") : TEXT("no"),
		FinalDamage);
	// Damage is a Meta attribute: PostGameplayEffectExecute transfers it into Health
	// and resets it to zero afterwards.
	OutExecutionOutput.AddOutputModifier(
		FGameplayModifierEvaluatedData(UMALAttributeSet::GetDamageAttribute(), EGameplayModOp::Additive, FinalDamage));
}
