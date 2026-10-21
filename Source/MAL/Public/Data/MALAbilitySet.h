// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "MALAbilitySet.generated.h"

class UGameplayAbility;

/**
 * FMALAbilitySetEntry
 * 
 * A single entry in an ability set, defining an ability class and its starting level.
 */
USTRUCT(BlueprintType)
struct FMALAbilitySetEntry
{
	GENERATED_BODY()

	/** The ability class to grant */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<UGameplayAbility> AbilityClass;

	/** Starting level when granted */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	int32 StartingLevel = 1;

	/** Optional: Override the InputActivatedTag from the Ability CDO. Leave empty to use the CDO default. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FGameplayTag OverrideInputTag;
};

/**
 * UMALAbilitySet
 * 
 * A reusable set of abilities (e.g. "Warrior Basic Abilities").
 * Granted to a character via ClassDefinition.
 */
UCLASS(BlueprintType)
class MAL_API UMALAbilitySet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Abilities contained in this set */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Ability Set")
	TArray<FMALAbilitySetEntry> Abilities;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
