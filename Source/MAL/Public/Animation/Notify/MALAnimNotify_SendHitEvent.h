// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"
#include "MALAnimNotify_SendHitEvent.generated.h"

/**
 * UMALAnimNotify_SendHitEvent
 *
 * Instant notify placed at each combo section's impact frame. Sends a GameplayEvent to
 * the owning actor so the ability (via UAbilityTask_WaitGameplayEvent) can resolve the
 * hit without the animation coupling to combat code.
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
