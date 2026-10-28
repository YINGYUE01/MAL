// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/MALHitResult.h"
#include "MALWeaponComponent.generated.h"

class UBoxComponent;
class UNiagaraComponent;
class UPrimitiveComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UMALWeaponDefinition;

/** Broadcast once per target per hit window (server only). */
DECLARE_MULTICAST_DELEGATE_OneParam(FMALWeaponHitSignature, const FMALHitResult&);

/**
 * UMALWeaponComponent
 *
 * Owns the melee hit window for a character: resolves the weapon collision primitive,
 * enables overlap for the duration of a swing and reports every target exactly once.
 *
 * Lives on AMALCharacterBase so player characters and enemies share one implementation.
 * The ability driving the montage only has to open and close the window; it never touches
 * collision state or the hit set itself.
 *
 * Server-authoritative: StartHitWindow is a no-op on clients, which keeps the overlap
 * binding (and therefore the whole hit test) off machines that could not resolve it.
 *
 * It also owns which weapon that window belongs to. A UMALWeaponDefinition carries the mesh, the
 * socket it hangs from, the size of the hit volume and the damage scale, so swapping weapons is
 * one call rather than a blueprint edit. The equipped definition is replicated - the server
 * decides, every machine draws the same weapon - while the hit window itself stays server-only.
 */
UCLASS(ClassGroup = (MAL), meta = (BlueprintSpawnableComponent))
class MAL_API UMALWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMALWeaponComponent();

	/**
	 * Open the hit window for one swing. Any window still open is closed first.
	 *
	 * @param ComboStep              Step index stamped on every resulting hit.
	 * @param CollisionComponentName Name of the primitive acting as the weapon hit volume.
	 *                               None uses HitBoxComponentName.
	 */
	void StartHitWindow(int32 ComboStep, FName CollisionComponentName = NAME_None);

	/** Close the window, disable the volume and clear the per-swing hit list. */
	void EndHitWindow();

	UFUNCTION(BlueprintPure, Category = "MAL|Combat")
	bool IsHitWindowOpen() const { return bHitWindowOpen; }

	/**
	 * Server only: swap to NewWeapon, or clear it with nullptr.
	 *
	 * Closes any hit window the old weapon opened, then applies the definition's mesh, socket, hit
	 * volume and damage scale. Clients are not told directly: they read the replicated property
	 * and apply the same state through OnRep_EquippedWeapon.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "MAL|Combat")
	void EquipWeapon(UMALWeaponDefinition* NewWeapon);

	/** Server only: drop the equipped weapon and restore the character's built-in one. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "MAL|Combat")
	void UnequipWeapon();

	UFUNCTION(BlueprintPure, Category = "MAL|Combat")
	UMALWeaponDefinition* GetEquippedWeapon() const { return EquippedWeapon; }

	/** Damage scale of the equipped weapon. 1 while unarmed. */
	UFUNCTION(BlueprintPure, Category = "MAL|Combat")
	float GetDamageMultiplier() const;

	/** Primitive name the hit window falls back to, and the box an equipped weapon resizes. */
	UFUNCTION(BlueprintPure, Category = "MAL|Combat")
	FName GetHitBoxComponentName() const { return HitBoxComponentName; }

	/** Fired on the authority for each target hit during the current window. */
	FMALWeaponHitSignature OnWeaponHit;

protected:
	//~ Begin UActorComponent
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~ End UActorComponent

	/** Every machine, including the authority: apply the definition the server selected. */
	UFUNCTION()
	void OnRep_EquippedWeapon();

	UFUNCTION()
	void HandleOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

private:
	/** Find the primitive with the given name on the owning actor. */
	UPrimitiveComponent* FindCollisionComponent(FName ComponentName) const;

	/** The hit volume box, resolved once and cached. Null when the character has none. */
	UBoxComponent* ResolveHitBox();

	/** Remember the box the character was authored with, so unequipping can put it back. */
	void CacheAuthoredHitBox();

	/** Meshes, built-in weapon visibility and trail. Runs on the server and on clients. */
	void ApplyVisuals();

	/** Size and centre the hit volume for the equipped weapon. */
	void ApplyHitBox();

	/** Unbind and turn collision off. Safe to call repeatedly. */
	void ReleaseCollision();

	/**
	 * Weapon the character starts with. Applied on the server in BeginPlay, which is also what
	 * seeds the replicated property for the clients that join later.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|Combat")
	TObjectPtr<UMALWeaponDefinition> StartingWeapon;

	/** Primitive that acts as the weapon hit volume. Must match the character's component name. */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|Combat")
	FName HitBoxComponentName = TEXT("weapon_collision");

	/**
	 * Weapon currently equipped. Replicated because the swap has to look the same everywhere;
	 * the definition itself is an asset, so the wire only carries a reference to it.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_EquippedWeapon)
	TObjectPtr<UMALWeaponDefinition> EquippedWeapon;

	/** Cached hit volume. Kept off CollisionComponent, which only exists during a window. */
	UPROPERTY(Transient)
	TObjectPtr<UBoxComponent> HitBox;

	/** Trail spawned for the equipped weapon, destroyed whenever the weapon changes. */
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> TrailComponent;

	/**
	 * Built-in weapon section currently hidden, so a swap can restore it before hiding the next
	 * one. Remembered here rather than read back from the definition, which is already the new one
	 * by the time the old state has to be undone.
	 */
	FName HiddenBuiltInSlot;

	/** Box size and centre the blueprint authored, restored when the weapon is unequipped. */
	FVector AuthoredHitBoxExtent = FVector::ZeroVector;
	FVector AuthoredHitBoxLocation = FVector::ZeroVector;

	bool bAuthoredHitBoxCached = false;

	/** Primitive currently acting as the hit volume. */
	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> CollisionComponent;

	/**
	 * Actors hit since the window opened.
	 * Weak on purpose: a target destroyed inside the hit window must not leave a dangling entry.
	 */
	TSet<TWeakObjectPtr<AActor>> HitActors;

	/** Reported back as FMALHitResult::WeaponSocketName. */
	FName ActiveComponentName;

	int32 CurrentComboStep = 0;

	bool bHitWindowOpen = false;
};
