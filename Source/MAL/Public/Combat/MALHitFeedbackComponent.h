// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "MALHitFeedbackComponent.generated.h"

class USkeletalMeshComponent;
class UAnimInstance;

/**
 * UMALHitFeedbackComponent
 *
 * Local impact feedback for a player controller: briefly freezes the controlled character's
 * pose when one of its hits lands.
 *
 * The freeze deliberately stops at the pose. Freezing the actor (CustomTimeDilation) or its
 * movement also stops the ServerMove stream, so the server has nothing to advance that
 * character with and every other client sees it hitch.
 *
 * bNoSkeletonUpdate is coarser than "hold this pose": it also gates ShouldTickPose, and TickPose
 * is the only caller of UAnimInstance::Montage_Advance. Left alone, each freeze rewinds the local
 * montage clock by its own duration. Everything that clock drives then trails the server by that
 * amount -- weapon window close, lunge start, and the ability's own end -- while remote clients
 * render the server's copy of the montage and see an attack that ends early or slides out of
 * sync with the body. TickComponent keeps the montage advancing while the pose is held, so the
 * freeze costs visuals only and gameplay timing stays authoritative.
 *
 * On a listen server the host's controller is also the server's, so a Client RPC becomes a local
 * call and even a pose freeze would stall the timeline the server samples for
 * RepAnimMontageInfo. BeginHitStop therefore refuses to run outside NM_Standalone/NM_Client,
 * which leaves the host with the camera shake alone.
 */
UCLASS(ClassGroup = (MAL), meta = (BlueprintSpawnableComponent))
class MAL_API UMALHitFeedbackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMALHitFeedbackComponent();

	/** Freeze the controlled character's pose briefly; overlapping hits extend the timer. */
	void BeginHitStop(float Duration);

	/** Restore the skeleton update flag. */
	void EndHitStop();

protected:
	//~ Begin UActorComponent
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent

private:
	FTimerHandle HitStopTimer;

	bool bHitStopActive = false;

	TWeakObjectPtr<USkeletalMeshComponent> HitStopMesh;

	/** Keeps the montage timeline moving while HitStopMesh has bNoSkeletonUpdate set. */
	TWeakObjectPtr<UAnimInstance> HitStopAnimInstance;

	/** Value to restore; something else may already have frozen the mesh. */
	bool bHitStopRestoreNoSkeletonUpdate = false;
};
