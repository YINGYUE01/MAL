// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MALComboAttackInterface.generated.h"

class USkeletalMeshComponent;

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UMALComboAttackInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * IMALComboAttackInterface
 *
 * Implemented by montage-driven melee abilities so AnimNotifies (combo window / lunge /
 * weapon collision) can call back into the ability that is currently driving the montage,
 * without the notifies coupling to a concrete ability class.
 *
 * Only the chaining methods are combo-specific. A single-swing ability implements the
 * interface for the weapon/lunge hooks and leaves the window methods empty, which is why
 * they have defaults rather than being pure virtual.
 */
class MAL_API IMALComboAttackInterface
{
	GENERATED_BODY()

public:
	/** Open the window during which the next combo hit can be chained. */
	virtual void OpenComboWindow() {}

	/** Close the chaining window. */
	virtual void CloseComboWindow() {}

	/** Trigger this step's forward lunge (called from the montage's lunge frame). */
	virtual void TriggerLunge() = 0;

	/** Enable weapon collision for hit detection */
	virtual void EnableWeaponCollision() {}

	/** Disable weapon collision */
	virtual void DisableWeaponCollision() {}

	/**
	 * Resolve the combo interface from the ability currently animating on the given
	 * skeletal mesh (owner -> IAbilitySystemInterface -> ASC -> GetAnimatingAbility).
	 * Returns nullptr when no combo ability is driving the montage.
	 */
	static IMALComboAttackInterface* GetFromMesh(USkeletalMeshComponent* MeshComp);
};
