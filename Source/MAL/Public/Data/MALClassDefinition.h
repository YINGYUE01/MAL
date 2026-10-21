// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MALClassDefinition.generated.h"

class UMALAbilitySet;

/**
 * UMALClassDefinition
 * 
 * Defines a character class/profession (e.g. Warrior, Mage).
 * Contains a list of ability sets to grant to the character.
 */
UCLASS(BlueprintType)
class MAL_API UMALClassDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Ability sets granted to characters using this class definition */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Class Definition")
	TArray<TObjectPtr<UMALAbilitySet>> GrantedAbilitySets;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
