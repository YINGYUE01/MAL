// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "MALAnimNotify_ComboWindow.generated.h"

/**
 * UMALAnimNotify_ComboWindow
 *
 * Notify state that opens the combo chaining window on Begin and closes it on End.
 * Place one per combo section, spanning the frames during which the next hit may be
 * chained. Talks to the ability driving the montage via IMALComboAttackInterface.
 */
UCLASS(meta = (DisplayName = "MAL Combo Window"))
class MAL_API UMALAnimNotify_ComboWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
