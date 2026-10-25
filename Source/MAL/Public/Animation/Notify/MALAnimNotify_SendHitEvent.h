// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"
#include "MALAnimNotify_SendHitEvent.generated.h"

/**
 * UMALAnimNotify_SendHitEvent
 *
 * Instant notify placed at a montage's impact frame. Sends a GameplayEvent to the owning actor
 * so gameplay logic can hook that frame without the animation coupling to combat code.
 *
 * Melee attacks resolve their hits from the weapon collision window instead, so nothing in the
 * project listens for this today. It stays as the generic hook for event-driven abilities: an
 * ability that needs the frame adds a UAbilityTask_WaitGameplayEvent for MAL.Event.Hit.
 */
UCLASS(meta = (DisplayName = "MAL Send Hit Event"))
class MAL_API UMALAnimNotify_SendHitEvent : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	/** Event tag to send. If left empty, defaults to MAL.Event.Hit at runtime. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MAL|Notify", meta = (Categories = "MAL.Event"))
	FGameplayTag HitEventTag;
};
