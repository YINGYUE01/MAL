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

	/** Generic accessor used by AMALCharacterBase (players take the set from the PlayerState). */
	UMALAttributeSet* GetAttributeSet() const { return HealthSet; }

	/** True once the server applied the default attribute GEs (prevents duplicate Infinite GEs on re-possession). */
	bool AreDefaultAttributesApplied() const { return bDefaultAttributesApplied; }
	void MarkDefaultAttributesApplied() { bDefaultAttributesApplied = true; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AbilitySystem")
	TObjectPtr<UMALAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AbilitySystem")
	TObjectPtr<UMALAttributeSet> HealthSet;

	/** Server-only guard; the PlayerState persists across respawns. */
	bool bDefaultAttributesApplied = false;
};
