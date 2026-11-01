// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MALLockOnComponent.generated.h"

class APlayerController;

/** Broadcast whenever the locked target changes, including the release to nullptr. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMALLockOnChanged, AActor*, LockedTarget);

/**
 * UMALLockOnComponent
 *
 * Turns the camera toward one hostile pawn and keeps it there until the player toggles it
 * off or the target stops being a valid one.
 *
 * Owned by the player controller rather than the pawn: the thing being rotated is the
 * controller's rotation, and it has to survive a respawn. Only the yaw is written here; the
 * pitch stays the player's, so looking up and down still works while locked.
 *
 * Lock-on is presentation plus a targeting convenience, not a gameplay authority: the server
 * never needs to know about it, so nothing here is replicated.
 */
UCLASS(ClassGroup = (MAL), meta = (BlueprintSpawnableComponent))
class MAL_API UMALLockOnComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMALLockOnComponent();

	/** Lock the best candidate, or release the current target when one is already held. */
	UFUNCTION(BlueprintCallable, Category = "MAL|LockOn")
	void ToggleLock();

	/** Release the current target, if any. Safe to call every frame. */
	UFUNCTION(BlueprintCallable, Category = "MAL|LockOn")
	void ClearLock();

	UFUNCTION(BlueprintPure, Category = "MAL|LockOn")
	AActor* GetLockedTarget() const { return LockedTarget.Get(); }

	UFUNCTION(BlueprintPure, Category = "MAL|LockOn")
	bool IsLocked() const { return LockedTarget.IsValid(); }

	UPROPERTY(BlueprintAssignable, Category = "MAL|LockOn")
	FMALLockOnChanged OnLockChanged;

	/** Furthest distance a new target may be picked at. */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|LockOn", meta = (ClampMin = "100.0"))
	float MaxLockDistance = 2000.0f;

	/**
	 * Distance past which an existing target is dropped.
	 *
	 * Deliberately larger than MaxLockDistance: a target that steps back a little should not
	 * make the player re-press the key, while one that ran away should.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|LockOn", meta = (ClampMin = "100.0"))
	float MaxBreakDistance = 2600.0f;

	/** Half-angle around the camera forward a candidate must fall inside to be picked. */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|LockOn", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float ViewConeDegrees = 50.0f;

	/** How fast the yaw chases the target. Higher is snappier, lower is more cinematic. */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|LockOn", meta = (ClampMin = "0.1"))
	float YawInterpSpeed = 10.0f;

	/** Seconds between line-of-sight traces; the target is not re-traced every frame. */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|LockOn", meta = (ClampMin = "0.0"))
	float LineOfSightCheckInterval = 0.15f;

	/** Height above the target's pivot the camera aims at, so the marker sits on the torso. */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|LockOn", meta = (ClampMin = "0.0"))
	float AimHeightOffset = 40.0f;

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	APlayerController* GetPlayerController() const;

	/** Best hostile candidate in front of the camera, or null when nothing qualifies. */
	AActor* FindBestTarget() const;

	/** Alive, hostile, and (optionally) within MaxLockDistance. */
	bool IsCandidateValid(const AActor* Candidate) const;

	/** Unobstructed trace from the camera to the aim point on the target. */
	bool HasLineOfSight(const AActor* Target) const;

	/** Rotate the controller's yaw toward the target; pitch is left alone. */
	void FaceTarget(const AActor* Target, float DeltaTime);

	TWeakObjectPtr<AActor> LockedTarget;

	/** Time accumulated since the last line-of-sight trace. */
	float TimeSinceSightCheck = 0.0f;

	/** Result of the last trace, so a momentary obstruction does not drop the lock. */
	bool bHadLineOfSight = true;
};
