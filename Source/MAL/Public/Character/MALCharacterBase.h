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
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UMALAbilitySystemComponent;
class UMALAttributeSet;
class UMALClassDefinition;
class UMALWeaponComponent;
class UMALComboComponent;
class UAnimInstance;
class USpringArmComponent;
class UCameraComponent;
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

	/** Mesh a UMALWeaponDefinition is drawn with. Hidden and empty until something is equipped. */
	UFUNCTION(BlueprintPure, Category = "MAL|Combat")
	UStaticMeshComponent* GetWeaponMesh() const { return WeaponMesh; }

	/**
	 * Weapon visual for definitions that ship a rigged mesh.
	 *
	 * Two components rather than one because a component renders either static or skeletal
	 * meshes, never both: the simple blades in this project are static meshes while the rigged
	 * weapon packs ship skeletal ones. The equipped definition fills the one it matches and
	 * blanks the other.
	 */
	UFUNCTION(BlueprintPure, Category = "MAL|Combat")
	USkeletalMeshComponent* GetWeaponSkeletalMesh() const { return WeaponSkeletalMesh; }

	/**
	 * Show or hide the blade baked into the character mesh.
	 *
	 * The Paragon Kwang mesh carries its own weapon as the M05_Weapon material section, skinned
	 * rigidly to the weapon_r bone. A weapon that brings its own mesh has to hide that section or
	 * the character ends up holding two blades, and an unequip has to put it back.
	 *
	 * A slot the mesh does not have is ignored rather than warned about: only characters whose
	 * mesh carries a blade have anything to hide, and the same weapon definition is meant to work
	 * on all of them.
	 */
	UFUNCTION(BlueprintCallable, Category = "MAL|Combat|Weapon")
	void SetBuiltInWeaponHidden(bool bHide, FName MaterialSlotName);

	/** Class definition that determines which abilities this character has. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Class")
	TObjectPtr<UMALClassDefinition> ClassDefinition;

	/**
	 * ClassDefinition, falling back to the class default when the instance carries none.
	 *
	 * The property is EditDefaultsOnly, so the class default is the only place it can be authored.
	 * An actor placed before the property existed can still carry a serialized empty override that
	 * reads as None, and the instance value then wins over the blueprint. Reading through the
	 * archetype keeps such an actor working; an enemy that loses its abilities this way only walks
	 * up to its target and never swings.
	 */
	const UMALClassDefinition* GetClassDefinition() const;

	/**
	 * Server only: drop every granted spec and grant the class kit plus every unlocked skill.
	 *
	 * The skill tree is bought while a pawn may already be past its one-shot grant, and a save
	 * can land after possession for the same reason. Rebuilding from scratch makes both cases
	 * order-independent and is what keeps the spec count from doubling: GrantAbilities alone
	 * refuses to run twice, so a refresh has to clear the flag as well as the specs.
	 */
	void RefreshGrantedAbilities();

	/**
	 * Server only: rebuild the abilities the equipped weapon brings.
	 *
	 * One entry point shared by equip, unequip, skill purchase and respawn, and safe to call
	 * any number of times: it withdraws the weapon layer first and grants it back from whatever
	 * is equipped now. The layer lives under its own source key, so a skill purchase that
	 * rebuilds the character's kit cannot take a weapon's abilities with it.
	 *
	 * Does nothing when no ability system is ready yet; GrantAbilities calls back in once one is.
	 */
	void RefreshWeaponGrantedAbilities();

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

	/**
	 * Collision profile applied to the mesh on death, so a body that starts simulating physics can
	 * actually collide with the world.
	 *
	 * Character meshes are authored on the CharacterMesh profile, which is query-only: a ragdoll
	 * built from it has no physics collision and drops straight through the floor. Ragdoll is the
	 * engine profile with the PhysicsBody object type, which the physics scene resolves. None
	 * leaves whatever profile the mesh already has.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Death")
	FName CorpseMeshCollisionProfile = TEXT("Ragdoll");

	/**
	 * Whether the corpse capsule keeps blocking the world.
	 *
	 * Off by default. The capsule sits exactly where the character died and, left on its character
	 * profile, becomes an invisible obstacle: the player walks into a body that is no longer there.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Death")
	bool bKeepCapsuleCollisionOnDeath = false;

	// --- Progression ---

	/**
	 * Experience awarded to whoever lands the killing blow, authored on the class default.
	 *
	 * Zero is the right default for the player's own pawn: only enemies publish a reward, and
	 * progression only ever reads this off the victim.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Progression", meta = (ClampMin = "0"))
	int32 ExperienceReward = 0;

	/**
	 * The reward a killer is actually paid for this character.
	 *
	 * Read this instead of ExperienceReward. Instances placed before the property existed carry a
	 * serialized zero of their own, and an instance value outranks the class default even when it
	 * was never authored by hand. Treat that zero as "nothing written here" and fall back to the
	 * class default, so a reward authored on the blueprint reaches every instance.
	 */
	int32 GetExperienceReward() const;

	// --- Animation ---

	/**
	 * Locomotion layer linked when nothing is equipped, or when the equipped weapon names none.
	 *
	 * A layer node in the host animation blueprint resolves to nothing without a linked class,
	 * which reads as a reference pose. This default keeps a character that starts unarmed at a
	 * valid pose instead of a T-pose.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Animation")
	TSoftClassPtr<UAnimInstance> DefaultLocomotionLayer;

	/**
	 * Every machine: relink the ground locomotion layer to whatever the equipped weapon names.
	 *
	 * Driven by UMALWeaponComponent::OnEquippedWeaponChanged, which fires on the authority and on
	 * every client, so the layer swap follows the replicated weapon instead of being decided once
	 * on the server.
	 */
	void RefreshLocomotionLayer();

	// --- Aiming ---

	/**
	 * Enter or leave the aiming stance: face the camera, strafe instead of turning, walk slower
	 * and pull the camera in.
	 *
	 * Presentation only - no tag, no ability. The ranged ability owns MAL.Status.Aiming through
	 * its activation tags, so the stance and the tag share one lifetime by construction. Safe
	 * to call repeatedly; only the first call in each direction does work. Restores exactly the
	 * rotation mode, walk speed and camera pose that were in place before the first aim.
	 */
	UFUNCTION(BlueprintCallable, Category = "MAL|Combat|Aim")
	void SetAiming(bool bNewAiming);

	UFUNCTION(BlueprintPure, Category = "MAL|Combat|Aim")
	bool IsAiming() const { return bIsAiming; }

	/**
	 * World transform a projectile leaves from: the equipped weapon's spawn socket, or the
	 * character as a fallback when the weapon has no mesh or no such socket.
	 *
	 * @param PreferredSocket socket to try first; None reads the equipped definition.
	 */
	UFUNCTION(BlueprintPure, Category = "MAL|Combat|Aim")
	FTransform GetWeaponMuzzleTransform(FName PreferredSocket = NAME_None) const;

	/** Walk speed multiplier applied while aiming. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Combat|Aim", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float AimingWalkSpeedScale = 0.45f;

	/**
	 * Spring arm length while aiming. Zero (the default) pulls the boom in to a fixed fraction
	 * of whatever rest length the character authored, so the zoom stays proportional on a pawn
	 * whose camera was never touched by hand. A positive value overrides that ratio.
	 * Ignored when the character has no spring arm.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Combat|Aim", meta = (ClampMin = "0.0"))
	float AimingArmLength = 0.0f;

	/** Camera field of view while aiming. Ignored when the character has no camera component. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Combat|Aim", meta = (ClampMin = "5.0"))
	float AimingCameraFOV = 80.0f;

	/** Seconds the camera takes to reach the aiming pose. Zero snaps. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Combat|Aim", meta = (ClampMin = "0.0"))
	float AimingCameraBlendTime = 0.15f;

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

	/** Apply one of the default attribute effects, with Level as the spec level. */
	void ApplyAttributeGE(TSubclassOf<UGameplayEffect> GEClass, float Level);

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
	 * Visual for a weapon swapped in through UMALWeaponComponent.
	 *
	 * One component reused for every weapon rather than one per weapon: swapping assigns a new
	 * static mesh and re-attaches to the definition's socket, which keeps the component set of
	 * every character stable no matter how many weapons the game ships.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MAL|Combat")
	TObjectPtr<UStaticMeshComponent> WeaponMesh;

	/**
	 * Second weapon visual, for definitions that ship a skeletal mesh.
	 * Carries the same socket and offset as WeaponMesh; only one of the two is ever populated.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MAL|Combat")
	TObjectPtr<USkeletalMeshComponent> WeaponSkeletalMesh;

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
	/** Link RequestedLocomotionLayer, retrying on later ticks while the mesh cannot take it. */
	void ApplyLocomotionLayer();

	/** Start the camera blend toward the aiming or the rest pose. */
	void StartAimingCameraBlend(bool bEnter);

	/** One step of the camera blend; clears its own timer on arrival. */
	void StepAimingCameraBlend();

	/** Place the camera at Alpha (0 = pose before aiming, 1 = aiming pose). */
	void ApplyAimingCameraBlend(float Alpha);

	/** Layer class last linked, so re-equipping the same weapon does not rebuild it. */
	UPROPERTY(Transient)
	TSubclassOf<UAnimInstance> AppliedLocomotionLayer;

	/** Layer class the current equip asks for; kept while the link waits for a usable mesh. */
	UPROPERTY(Transient)
	TSubclassOf<UAnimInstance> RequestedLocomotionLayer;

	/**
	 * Ticks already spent retrying the current request. A swap requested before the mesh has an
	 * animation instance, or during a hit-stop, waits for the first tick that can take it; the
	 * bound only exists so a layer that can never link gives up instead of retrying forever.
	 */
	int32 LocomotionLinkAttempts = 0;

	/** About half a second of ticks: longer than a hit-stop, short enough to fail loudly. */
	static constexpr int32 MaxLocomotionLinkAttempts = 30;

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

	/** True between SetAiming(true) and SetAiming(false). */
	bool bIsAiming = false;

	/** Rotation mode in place before the first aim, restored on exit. */
	bool bSavedOrientRotationToMovement = false;
	bool bSavedUseControllerRotationYaw = false;

	/** Walk speed in place before the first aim, restored on exit. */
	float SavedAimWalkSpeed = 0.0f;

	/** Spring arm length and camera FOV authored in the blueprint, restored on exit. */
	float AuthoredArmLength = 0.0f;
	float AuthoredCameraFOV = 90.0f;
	bool bCameraPoseCaptured = false;

	/** Camera blend state; the timer runs only while a blend is in flight. */
	FTimerHandle AimingCameraBlendTimer;
	TWeakObjectPtr<USpringArmComponent> AimingSpringArm;
	TWeakObjectPtr<UCameraComponent> AimingCamera;
	float BlendStartArmLength = 0.0f;
	float BlendEndArmLength = 0.0f;
	float BlendStartFOV = 0.0f;
	float BlendEndFOV = 0.0f;
	float BlendElapsed = 0.0f;

	/** Attach to the health attribute on whichever ASC this character currently uses. */
	void BindHealthChange();

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> BoundAbilitySystemComponent;

	FDelegateHandle HealthChangedDelegateHandle;
};
