// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MALCombatEvents.generated.h"

/**
 * FMALDamageResolvedEvent
 *
 * One resolved damage application, published on the authority by UMALAttributeSet after the
 * meta damage has been folded into Health.
 *
 * Presentation consumers (damage numbers today, hit markers and combat logs later) subscribe
 * to UMALCombatEventsSubsystem instead of AttributeSet reaching into the HUD.
 */
USTRUCT(BlueprintType)
struct FMALDamageResolvedEvent
{
	GENERATED_BODY()

	/**
	 * Actor that owns the damaged AttributeSet: the PlayerState for player characters, the pawn
	 * itself for enemies. Compare it against the set's owning actor, not against the avatar.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Combat")
	TObjectPtr<AActor> Victim = nullptr;

	/** Actor credited with the damage (the effect instigator). */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Combat")
	TObjectPtr<AActor> Instigator = nullptr;

	/** Impact point, or a point above the victim when the hit carried none. */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Combat")
	FVector WorldLocation = FVector::ZeroVector;

	/** Post-mitigation damage actually removed from Health. */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Combat")
	float Amount = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Combat")
	bool bCritical = false;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Combat")
	bool bMagic = false;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|Combat")
	bool bKillingBlow = false;
};
