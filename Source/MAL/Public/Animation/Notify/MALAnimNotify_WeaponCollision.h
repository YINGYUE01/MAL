// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "MALAnimNotify_WeaponCollision.generated.h"

/**
 * UMALAnimNotify_WeaponCollision
 * 
 * Toggles weapon collision on/off during montage playback.
 * Place at the start of a hit window (bEnable=true) and at the end (bEnable=false).
 * The notify routes through IMALComboAttackInterface to the active ability.
 */
UCLASS(Blueprintable, meta = (DisplayName = "MAL Weapon Collision"))
class MAL_API UMALAnimNotify_WeaponCollision : public UAnimNotify
{
	GENERATED_BODY()

public:
	/** Whether to enable or disable the weapon collision */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MAL|Notify")
	bool bEnable = true;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
