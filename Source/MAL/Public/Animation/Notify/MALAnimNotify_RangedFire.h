// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "MALAnimNotify_RangedFire.generated.h"

/**
 * UMALAnimNotify_RangedFire
 *
 * Release frame of a ranged cast: fires the projectile of whichever ranged ability is animating
 * this montage. Place it on the frame the weapon actually releases (bow string snaps forward),
 * which is what makes the shot line up with the animation instead of with a timer.
 *
 * Mirrors UMALAnimNotify_WeaponCollision: the ability is resolved from the animating mesh, and
 * the notify itself does nothing on clients (the ability guards on authority).
 */
UCLASS(Blueprintable, meta = (DisplayName = "MAL Ranged Fire"))
class MAL_API UMALAnimNotify_RangedFire : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName_Implementation() const override;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
