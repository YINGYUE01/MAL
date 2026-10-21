// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "InputAction.h"
#include "MALInputConfig.generated.h"

/**
 * FMALInputActionMapping
 * 
 * Maps a GameplayTag to an InputAction for tag-driven input handling.
 */
USTRUCT(BlueprintType)
struct FMALInputActionMapping
{
	GENERATED_BODY()

	/** Tag identifying this input action (e.g. Input.Move, Input.Attack) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FGameplayTag InputTag;

	/** The InputAction asset to trigger */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UInputAction* Action;
};

/**
 * UMALInputConfig
 * 
 * Data asset that holds an array of InputTag-to-InputAction mappings.
 * Used by the input component to bind actions with tag-based identification.
 */
UCLASS(BlueprintType)
class MAL_API UMALInputConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Array of InputTag <-> InputAction mappings */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TArray<FMALInputActionMapping> AbilityInputActions;

	/** Find an InputAction by its associated GameplayTag */
	UFUNCTION(BlueprintCallable, Category = "Input")
	UInputAction* FindActionByTag(const FGameplayTag& InputTag) const;

	/** Find the GameplayTag associated with an InputAction */
	UFUNCTION(BlueprintCallable, Category = "Input")
	FGameplayTag FindTagByAction(const UInputAction* Action) const;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
