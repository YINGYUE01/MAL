// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "MALCharacterBase.generated.h"

class UGameplayEffect;
class UMALAbilitySystemComponent;
class UMALAttributeSet;
class UMALClassDefinition;

/**
 * AMALCharacterBase
 *
 * Shared base for player characters and enemies.
 *
 * ASC/AttributeSet ownership:
 *  - Player characters: the real ASC/AttributeSet live on AMALPlayerState. This base only
 *    caches pointers to them and initializes the actor info with (PlayerState, this).
 *  - Enemies: the real ASC/AttributeSet are default subobjects on the enemy character
 *    (see AMALEnemyCharacter). This base initializes the actor info with (this, this).
 *
 * InitializeAbilitySystem() resolves whichever case applies, so subclasses do not have to
 * duplicate the GAS setup.
 */
UCLASS(Abstract)
class MAL_API AMALCharacterBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AMALCharacterBase();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	/** Cached MAL ASC (PlayerState-owned for players, character-owned for enemies). */
	UFUNCTION(BlueprintPure, Category = "MAL|AbilitySystem")
	UMALAbilitySystemComponent* GetMALAbilitySystemComponent() const;

	/** Cached MAL AttributeSet (PlayerState-owned for players, character-owned for enemies). */
	UFUNCTION(BlueprintPure, Category = "MAL|AbilitySystem")
	UMALAttributeSet* GetMALAttributeSet() const;

	/** Class definition that determines which abilities this character has. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Class")
	TObjectPtr<UMALClassDefinition> ClassDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MAL|Attribute")
	TSubclassOf<UGameplayEffect> DefaultPrimaryAttribute;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MAL|Attribute")
	TSubclassOf<UGameplayEffect> DefaultSecondaryAttribute;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MAL|Attribute")
	TSubclassOf<UGameplayEffect> DefaultStandaloneAttribute;

protected:
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;

	/** Client-side init: the PlayerState (and with it the ASC) replicates after possession. */
	virtual void OnRep_PlayerState() override;

	/** Re-init when the controlling controller changes (respawn/re-possession). */
	virtual void OnRep_Controller() override;

	/**
	 * Resolve the ASC/AttributeSet pointers and initialize actor info.
	 * Safe to call multiple times (BeginPlay, PossessedBy, OnRep_*).
	 */
	virtual void InitializeAbilitySystem();

	/** Server only: apply default attribute GEs once. */
	virtual void ApplyDefaultAttributes();

	/** Server only: clear previously granted specs and grant ClassDefinition's ability sets. */
	virtual void GrantAbilities();

	/** Default attribute application guard. Player characters override this to use PlayerState. */
	virtual bool AreDefaultAttributesApplied() const;
	virtual void MarkDefaultAttributesApplied();

	/** Cached ASC. For players this points at PlayerState's component; for enemies it is a subobject. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MAL|AbilitySystem")
	TObjectPtr<UMALAbilitySystemComponent> AbilitySystemComponent;

	/** Cached AttributeSet. For players this points at PlayerState's set; for enemies it is a subobject. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MAL|AbilitySystem")
	TObjectPtr<UMALAttributeSet> AttributeSet;

	/** Enemy/default guard; player characters use AMALPlayerState's flag instead. */
	bool bDefaultAttributesApplied = false;

	/** Prevents clearing/regranting on every possession of the same character. */
	bool bAbilitiesGranted = false;
};
