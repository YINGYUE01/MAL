// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayEffectTypes.h"
#include "GenericTeamAgentInterface.h"
#include "MALCharacterBase.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
class UMALAbilitySystemComponent;
class UMALAttributeSet;
class UMALClassDefinition;
class UMALWeaponComponent;
class UMALComboComponent;
struct FMALDamageResolvedEvent;

/**
 * Team ids the shipped characters are created with. Blueprints can override TeamId per class, so
 * these are only the defaults a scene can rely on out of the box.
 */
namespace MALTeam
{
	constexpr uint8 Player = 1;
	constexpr uint8 Enemy = 2;
}

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
class MAL_API AMALCharacterBase : public ACharacter, public IAbilitySystemInterface, public IGenericTeamAgentInterface
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

	//~ Begin IGenericTeamAgentInterface
	virtual FGenericTeamId GetGenericTeamId() const override { return TeamId; }
	virtual void SetGenericTeamId(const FGenericTeamId& NewTeamId) override { TeamId = NewTeamId; }
	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;
	//~ End IGenericTeamAgentInterface

	/**
	 * Team this character fights for.
	 *
	 * Static per class: it is read from the archetype on every machine, which is why it is not
	 * replicated. FGenericTeamId::NoTeam (the default) means unassigned, and an unassigned
	 * character is neutral towards everyone rather than friendly to other unassigned ones.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Team")
	FGenericTeamId TeamId;

	/** Melee hit-window driver shared by player characters and enemies. */
	UFUNCTION(BlueprintPure, Category = "MAL|Combat")
	UMALWeaponComponent* GetWeaponComponent() const { return WeaponComponent; }

	/** Combo state machine shared by player characters and enemies. */
	UFUNCTION(BlueprintPure, Category = "MAL|Combat")
	UMALComboComponent* GetComboComponent() const { return ComboComponent; }

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

	/**
	 * Server side: seconds the corpse stays before it is destroyed. Zero keeps it forever.
	 * A cleared wave should not leave pawns behind that still replicate and tick.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Death", meta = (ClampMin = "0.0"))
	float CorpseLifeSpan = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MAL|Attribute")
	TSubclassOf<UGameplayEffect> DefaultPrimaryAttribute;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MAL|Attribute")
	TSubclassOf<UGameplayEffect> DefaultSecondaryAttribute;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MAL|Attribute")
	TSubclassOf<UGameplayEffect> DefaultStandaloneAttribute;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
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

	/**
	 * Hit-window driver for melee attacks.
	 * Created here so every character that can swing a weapon has one, regardless of whether
	 * the ability lives on a player-controlled or an AI-controlled pawn.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MAL|Combat")
	TObjectPtr<UMALWeaponComponent> WeaponComponent;

	/**
	 * Combo state machine for melee attacks (buffered input, step advancement, recovery).
	 * Kept off the ability so a new activation always starts from a clean state.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MAL|Combat")
	TObjectPtr<UMALComboComponent> ComboComponent;

	/** Enemy/default guard; player characters use AMALPlayerState's flag instead. */
	bool bDefaultAttributesApplied = false;

	/** Prevents clearing/regranting on every possession of the same character. */
	bool bAbilitiesGranted = false;

private:
	/** Enter the death state when health crosses zero. Server only. */
	void HandleHealthChanged(const FOnAttributeChangeData& Data);

	/**
	 * Authority only: kill this character with the instigator carried by the resolved hit.
	 *
	 * The attribute change delegate has no effect spec, so the killer can only be named from
	 * here. UMALAttributeSet publishes the event immediately after folding damage into Health,
	 * which is why HandleHealthChanged defers its own fallback by one tick.
	 */
	void HandleDamageResolved(const FMALDamageResolvedEvent& Event);

	/** Next-tick fallback for deaths that never went through the damage pipeline. */
	void HandlePendingDeath();

	/**
	 * True while HandlePendingDeath is queued. Health can cross zero once per damage tick, so
	 * without this guard a damage-over-time effect would queue a fresh timer on every change.
	 */
	bool bPendingDeathCheck = false;

	FTimerHandle PendingDeathTimer;

	/** Attach to the health attribute on whichever ASC this character currently uses. */
	void BindHealthChange();

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> BoundAbilitySystemComponent;

	FDelegateHandle HealthChangedDelegateHandle;
};
