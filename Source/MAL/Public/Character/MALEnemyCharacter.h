// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/MALCharacterBase.h"
#include "GameplayTagContainer.h"
#include "Templates/SubclassOf.h"
#include "MALEnemyCharacter.generated.h"

class UMALWorldWidgetComponent;
class UGameplayAbility;

/**
 * AMALEnemyCharacter
 *
 * Enemy character. Unlike the player, it has no PlayerState-owned ASC; the ASC and
 * AttributeSet are default subobjects on this character. AMALCharacterBase caches them
 * and initializes actor info with (this, this).
 *
 * Appearance, the weapon hit volume and every AI decision live outside this class: the AI
 * layer was removed and will be replaced, so nothing here assumes a brain, a target or a
 * combat range.
 */
UCLASS()
class MAL_API AMALEnemyCharacter : public AMALCharacterBase
{
	GENERATED_BODY()

public:
	AMALEnemyCharacter();

	/** World-space health bar shown above the enemy. Set its WidgetClass on the blueprint. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MAL|UI")
	TObjectPtr<UMALWorldWidgetComponent> HealthBarWidget;

	/**
	 * Abilities every instance of this enemy starts with.
	 *
	 * An enemy has no weapon-driven kit and no class definition, so this list is its whole
	 * ability source. Granted on the server once the ability system is initialized; safe to
	 * edit per blueprint.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Abilities")
	TArray<TSubclassOf<UGameplayAbility>> InitialAbilities;

	/**
	 * Activate an already-granted ability by its asset tag.
	 *
	 * This is the trigger surface for everything that is not player input: a scripted encounter,
	 * a new AI layer, or a blueprint timeline. Server side only in practice, because abilities
	 * themselves are server-authoritative.
	 */
	UFUNCTION(BlueprintCallable, Category = "MAL|Abilities")
	bool TryActivateAbilityByTag(FGameplayTag AbilityTag);

protected:
	/** Server only: base clears the legacy layer and rebuilds the weapon, then grants InitialAbilities. */
	virtual void GrantAbilities() override;
};