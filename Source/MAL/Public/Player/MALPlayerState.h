// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "MALPlayerState.generated.h"

class UMALAbilitySystemComponent;
class UMALAttributeSet;

UCLASS()
class MAL_API AMALPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AMALPlayerState();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UMALAttributeSet* GetHealthSet() const { return HealthSet; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AbilitySystem")
	TObjectPtr<UMALAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AbilitySystem")
	TObjectPtr<UMALAttributeSet> HealthSet;
};
