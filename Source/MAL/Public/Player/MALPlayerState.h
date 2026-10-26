// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "MALPlayerState.generated.h"

class UMALAbilitySystemComponent;
class UMALAttributeSet;
class UMALProgressionAttributeSet;
class UMALProgressionComponent;

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

	/**
	 * Level and experience, in a set of its own rather than on UMALAttributeSet.
	 *
	 * Enemies carry that set too and have no progression to replicate, and keeping the two apart
	 * leaves the vitals set's replication list and clamp switch without a players-only branch.
	 */
	UFUNCTION(BlueprintPure, Category = "MAL|Progression")
	UMALProgressionAttributeSet* GetProgressionAttributeSet() const { return ProgressionSet; }

	/**
	 * Awards experience, owns the level-up decision, and spends the attribute points it pays out.
	 * Server-only logic.
	 */
	UFUNCTION(BlueprintPure, Category = "MAL|Progression")
	UMALProgressionComponent* GetProgressionComponent() const { return ProgressionComponent; }

	/** Current character level, rounded. 1 while the progression set or the ASC is absent. */
	int32 GetProgressionLevel() const;

	/** True once the server applied the default attribute GEs (prevents duplicate Infinite GEs on re-possession). */
	bool AreDefaultAttributesApplied() const { return bDefaultAttributesApplied; }
	void MarkDefaultAttributesApplied() { bDefaultAttributesApplied = true; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AbilitySystem")
	TObjectPtr<UMALAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AbilitySystem")
	TObjectPtr<UMALAttributeSet> HealthSet;

	/**
	 * Level and experience. Lives here for the same reason the vitals set does: this is the actor
	 * the ASC belongs to, and the ASC adopts every attribute set it finds on its owner.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AbilitySystem")
	TObjectPtr<UMALProgressionAttributeSet> ProgressionSet;

	/**
	 * Progression belongs to the player, not to the pawn: a respawn hands over a new character
	 * while the level and the banked experience carry on.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MAL|Progression")
	TObjectPtr<UMALProgressionComponent> ProgressionComponent;

	/** Server-only guard; the PlayerState persists across respawns. */
	bool bDefaultAttributesApplied = false;
};
