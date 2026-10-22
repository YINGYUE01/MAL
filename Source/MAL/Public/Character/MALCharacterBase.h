// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayEffectTypes.h"
#include "MALCharacterBase.generated.h"

class UAbilitySystemComponent;
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

	/** True on the server and on every client once health reached zero. */
	UFUNCTION(BlueprintPure, Category = "MAL|Death")
	bool IsDead() const { return bIsDead; }

	/**
	 * Server side: enter the death state.
	 *
	 * Guarded by bIsDead, so a killing blow that lands several hits in one frame still runs the
	 * shutdown once. Ignored on clients; they get the state through replication.
	 */
	UFUNCTION(BlueprintCallable, Category = "MAL|Death")
	void HandleDeath(AActor* Killer = nullptr);

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

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * Replicated so a client runs the same presentation path as the authority. The loose
	 * Status.Dead tag and the ability cancellation stay server-only.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_IsDead, BlueprintReadOnly, Category = "MAL|Death")
	bool bIsDead = false;

	UFUNCTION()
	void OnRep_IsDead();

	/**
	 * Shared shutdown: stops movement, AI logic and input, then fires the blueprint hook.
	 *
	 * The killer is only known on the authority. Clients receive nullptr because the
	 * GameplayEffectSpec that carried it never leaves the server.
	 */
	virtual void ApplyDeathState(AActor* Killer);

	/**
	 * Blueprint hook for death montages, ragdoll, loot and despawn timers.
	 * Runs once on the server and once on each client.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "MAL|Death", meta = (DisplayName = "On Death"))
	void OnDeath(AActor* Killer);

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

private:
	/** Enter the death state when health crosses zero. Server only. */
	void HandleHealthChanged(const FOnAttributeChangeData& Data);

	/** Attach to the health attribute on whichever ASC this character currently uses. */
	void BindHealthChange();

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> BoundAbilitySystemComponent;

	FDelegateHandle HealthChangedDelegateHandle;
};
