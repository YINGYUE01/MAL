// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "MALAnimInstance.generated.h"

class AMALCharacterBase;

UENUM(BlueprintType)
enum class EMALLocomotionState : uint8
{
	Idle	UMETA(DisplayName = "Idle"),
	Jog		UMETA(DisplayName = "Jog"),
	Sprint	UMETA(DisplayName = "Sprint")
};

UCLASS()
class MAL_API UMALAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UMALAnimInstance();

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	/**
	 * Advance every montage timeline and fire its notifies without evaluating the anim graph.
	 * Needed while the mesh has bNoSkeletonUpdate set: the pose stays frozen, but montage position
	 * and notify timing keep running on the authoritative clock instead of falling behind by the
	 * length of the freeze.
	 */
	void AdvanceMontagesOnly(float DeltaSeconds);

	// ---- Locomotion ----

	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	float Speed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	float Direction = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	float VelocityZ = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	bool bIsInAir = false;

	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	bool bHasMovementInput = false;

	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	EMALLocomotionState LocomotionState = EMALLocomotionState::Idle;

	// ---- Locomotion Blend Space ----
	//
	// Directional movement is authored per weapon inside its locomotion layer anim blueprint
	// (ABP_Locomotion_*), so the asset reference lives there, not here. This class only
	// publishes the two axis values that layer needs.

	/**
	 * Value to feed the layer's blend space: X = Direction (-180..180), Y = Speed (cm/s).
	 *
	 * Direction is measured from movement input against the body facing, which is what makes a
	 * strafe read as a strafe; Speed is the planar velocity.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion|BlendSpace")
	FVector2D LocomotionBlendInput = FVector2D::ZeroVector;

	/** True while moving sideways or backward relative to facing, i.e. when strafing is expected. */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion|BlendSpace")
	bool bIsStrafing = false;

	/**
	 * True while the player holds a lock-on target.
	 *
	 * Drives which blend space the weapon locomotion layer plays: a 1D speed blend while free,
	 * the 2D direction/speed blend while locked.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion|BlendSpace")
	bool bIsLockedOn = false;

	/** Convenience alias read by the layer graphs (= bIsLockedOn). */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion|BlendSpace")
	bool bUseStrafeBlendSpace = false;

	/** |Direction| past which movement counts as strafing rather than forward locomotion. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Locomotion|BlendSpace", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float StrafeDirectionThreshold = 45.0f;

	UFUNCTION(BlueprintPure, Category = "Locomotion|BlendSpace")
	FVector2D GetLocomotionBlendInput() const { return LocomotionBlendInput; }

	// ---- Thresholds ----

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Locomotion|Thresholds")
	float JogSpeedThreshold = 100.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Locomotion|Thresholds")
	float SprintSpeedThreshold = 400.f;

	// ---- Aim Offset ----

	UPROPERTY(BlueprintReadOnly, Category = "AimOffset")
	FRotator AimRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "AimOffset")
	FRotator AimOffset = FRotator::ZeroRotator;

protected:
	AMALCharacterBase* GetMALCharacter() const;
	void UpdateMovementData();
	void UpdateAimOffset();
	EMALLocomotionState CalculateLocomotionState(float InSpeed) const;

	/** Reads the lock state from the owning player controller; false for AI or unpossessed pawns. */
	bool IsLockedOn() const;

private:
	UPROPERTY()
	TObjectPtr<AMALCharacterBase> CachedCharacter;
};