// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "MALAnimNotify_Lunge.generated.h"

/**
 * UMALAnimNotify_Lunge
 *
 * Instant notify placed at each combo section's forward-motion frame. Triggers the
 * owning ability's lunge via IMALComboAttackInterface::TriggerLunge().
 */
UCLASS(meta = (DisplayName = "MAL Lunge"))
class MAL_API UMALAnimNotify_Lunge : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
