// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/MALHitResult.h"
#include "MALWeaponComponent.generated.h"

class UPrimitiveComponent;

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
	 */
	void StartHitWindow(int32 ComboStep, FName CollisionComponentName);

	/** Close the window, disable the volume and clear the per-swing hit list. */
	void EndHitWindow();

	UFUNCTION(BlueprintPure, Category = "MAL|Combat")
	bool IsHitWindowOpen() const { return bHitWindowOpen; }

	/** Fired on the authority for each target hit during the current window. */
	FMALWeaponHitSignature OnWeaponHit;

protected:
	//~ Begin UActorComponent
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent

	UFUNCTION()
	void HandleOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

private:
	/** Find the primitive with the given name on the owning actor. */
	UPrimitiveComponent* FindCollisionComponent(FName ComponentName) const;

	/** Unbind and turn collision off. Safe to call repeatedly. */
	void ReleaseCollision();

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
