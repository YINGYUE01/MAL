// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "MALAnimNotifyState_SpellLift.generated.h"

/**
 * Marks the lift interval on a spell montage.
 * Begin starts the Root Motion lift; End releases the task and enters hover.
 */
UCLASS(Blueprintable, meta = (DisplayName = "MAL Spell Lift"))
class MAL_API UMALAnimNotifyState_SpellLift : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName_Implementation() const override;

	virtual void NotifyBegin(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		float TotalDuration,
		const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
