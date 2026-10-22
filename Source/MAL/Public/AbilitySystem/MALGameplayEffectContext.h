// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "MALGameplayEffectContext.generated.h"

/**
 * FMALGameplayEffectContext
 *
 * Project-wide effect context, allocated by UMALAbilitySystemComponent::MakeEffectContext.
 *
 * Damage executions write their transient hit metadata here (crit flag, damage channel)
 * so presentation code can read the result without recomputing it:
 *
 *   const FMALGameplayEffectContext* MALContext =
 *       static_cast<const FMALGameplayEffectContext*>(Spec.GetContext().Get());
 *
 * Both flags are replicated through NetSerialize, so owning clients and simulated
 * proxies see the same values the server rolled.
 */
USTRUCT()
struct FMALGameplayEffectContext : public FGameplayEffectContext
{
	GENERATED_BODY()

public:
	FMALGameplayEffectContext() = default;

	/** True when the damage execution rolled a critical hit. */
	bool IsCriticalHit() const { return bIsCriticalHit; }
	void SetIsCriticalHit(bool bInIsCriticalHit) { bIsCriticalHit = bInIsCriticalHit; }

	/** True when the hit was resolved through the magic channel (MAL.Damage.Type.Magic). */
	bool IsMagicDamage() const { return bIsMagicDamage; }
	void SetIsMagicDamage(bool bInIsMagicDamage) { bIsMagicDamage = bInIsMagicDamage; }

	//~ Begin FGameplayEffectContext
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FMALGameplayEffectContext::StaticStruct();
	}

	virtual FGameplayEffectContext* Duplicate() const override
	{
		FMALGameplayEffectContext* NewContext = new FMALGameplayEffectContext();
		*NewContext = *this;

		if (GetHitResult())
		{
			// Does a deep copy of the hit result
			NewContext->AddHitResult(*GetHitResult(), true);
		}

		return NewContext;
	}

	virtual bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess) override;

	virtual FString ToString() const override;
	//~ End FGameplayEffectContext

protected:
	/** Set by UMALDamageExecutionCalculation when the crit roll succeeded. */
	UPROPERTY()
	bool bIsCriticalHit = false;

	/** Set by UMALDamageExecutionCalculation when the magic channel was used. */
	UPROPERTY()
	bool bIsMagicDamage = false;
};

template<>
struct TStructOpsTypeTraits<FMALGameplayEffectContext> : public TStructOpsTypeTraitsBase2<FMALGameplayEffectContext>
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true
	};
};
