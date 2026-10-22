// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/MALGameplayEffectContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MALGameplayEffectContext)

bool FMALGameplayEffectContext::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	// Let the base context replicate its own data first (instigator, causer, ability,
	// source object, actors, hit result, world origin), then append MAL-only bits.
	FGameplayEffectContext::NetSerialize(Ar, Map, bOutSuccess);

	uint8 MALRepBits = 0;
	if (Ar.IsSaving())
	{
		if (bIsCriticalHit)
		{
			MALRepBits |= 1 << 0;
		}
		if (bIsMagicDamage)
		{
			MALRepBits |= 1 << 1;
		}
	}

	Ar.SerializeBits(&MALRepBits, 2);

	if (Ar.IsLoading())
	{
		bIsCriticalHit = (MALRepBits & (1 << 0)) != 0;
		bIsMagicDamage = (MALRepBits & (1 << 1)) != 0;
	}

	return true;
}

FString FMALGameplayEffectContext::ToString() const
{
	FString Result = FGameplayEffectContext::ToString();
	Result += FString::Printf(TEXT(", Critical:%s, Magic:%s"),
		bIsCriticalHit ? TEXT("true") : TEXT("false"),
		bIsMagicDamage ? TEXT("true") : TEXT("false"));
	return Result;
}
