// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "MALAnimNotifyState_SpellHover.generated.h"

/**
 * Marks the hover interval on a spell montage. The ability holds MOVE_Flying for this window;
 * the following landing notify starts the descent.
 */
UCLASS(Blueprintable, meta = (DisplayName = "MAL Spell Hover"))
class MAL_API UMALAnimNotifyState_SpellHover : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName_Implementation() const override;

	virtual void NotifyBegin(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		float TotalDuration,
		const FAnimNotifyEventReference& EventReference) override;
};
