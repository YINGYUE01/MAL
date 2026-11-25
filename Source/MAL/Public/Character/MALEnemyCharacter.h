// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/MALCharacterBase.h"
#include "GameplayTagContainer.h"
#include "Templates/SubclassOf.h"
#include "MALEnemyCharacter.generated.h"

class UMALWorldWidgetComponent;
class UMALBossAttackSelectorComponent;
class UMALBossSkillKit;
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
	 * Boss skill kit: the single source of the Boss's skills.
	 *
	 * Assigning it makes this enemy a Boss: every kit entry is granted on the server and the
	 * selector scores the selectable ones. Leave it empty on regular enemies - they grant
	 * InitialAbilities instead and the selector stays inert.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Boss")
	TObjectPtr<UMALBossSkillKit> BossSkillKit;

	/** Name shown on the top-screen Boss bar; falls back to "Boss" when unset. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Boss|Bar")
	FText BossDisplayName;

	/**
	 * Radius around a player inside which this Boss shows a top-screen bar, in centimetres.
	 *
	 * Visibility is computed per player on the server and pushed through Client RPCs; clients
	 * never infer it from replicated state. Zero disables the bar.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Boss|Bar", meta = (ClampMin = "0.0"))
	float BossBarActivationRadius = 1500.0f;

	/** Server-side Boss attack selector; only active when BossSkillKit is assigned. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MAL|Boss")
	TObjectPtr<UMALBossAttackSelectorComponent> BossAttackSelector;

	/**
	 * What kind of enemy this is; authored on the blueprint.
	 *
	 * The tag is a class default, so every machine reads the same value without replication.
	 * Empty means ordinary: only Boss-tagged enemies get a top-screen bar today.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|AI|Type")
	FGameplayTag EnemyTypeTag;

	/** Name shown on the top-screen Boss bar; falls back to "Boss" when unset. */
	UFUNCTION(BlueprintPure, Category = "MAL|Boss")
	FText GetBossDisplayName() const;

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

	/**
	 * Activate a granted ability by class.
	 *
	 * Preferred by the Boss selector: multiple Boss attacks can share a tag, but a class is a
	 * single spec, so this activates exactly one attack instead of every tag sibling.
	 */
	UFUNCTION(BlueprintCallable, Category = "MAL|Abilities")
	bool TryActivateAbilityByClass(TSubclassOf<UGameplayAbility> AbilityClass);

protected:
	virtual void BeginPlay() override;

	/** Server only: base clears the legacy layer and rebuilds the weapon, then grants InitialAbilities. */
	virtual void GrantAbilities() override;
};