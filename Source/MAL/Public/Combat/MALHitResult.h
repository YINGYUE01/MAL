// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MALHitResult.generated.h"

/**
 * FMALHitResult
 * 
 * Data passed from weapon collision overlap to the damage resolution pipeline.
 * Contains enough context to construct a GameplayEffect application.
 */
USTRUCT(BlueprintType)
struct FMALHitResult
{
	GENERATED_BODY()

	/** The actor that was hit */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Combat")
	TObjectPtr<AActor> HitActor;

	/** The bone/socket on the weapon that detected the hit */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Combat")
	FName WeaponSocketName;

	/** Combo step index when the hit occurred */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Combat")
	int32 ComboStep = 0;

	/** World-space hit location (weapon bone position at overlap) */
	UPROPERTY(BlueprintReadOnly, Category = "MAL|Combat")
	FVector HitLocation = FVector::ZeroVector;
};
