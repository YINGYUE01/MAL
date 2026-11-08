// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "MALAnimNotifyState_SpellLanding.generated.h"

/**
 * Marks the landing interval on a spell montage.
 * Begin starts the descent from the lift start location; End releases gravity and closes the
 * motion sequence.
 */
UCLASS(Blueprintable, meta = (DisplayName = "MAL Spell Landing"))
class MAL_API UMALAnimNotifyState_SpellLanding : public UAnimNotifyState
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
