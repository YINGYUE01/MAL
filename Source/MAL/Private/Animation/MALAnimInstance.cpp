// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/MALAnimInstance.h"
#include "Character/MALCharacterBase.h"
#include "Combat/MALLockOnComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Player/MALPlayerController.h"

UMALAnimInstance::UMALAnimInstance()
{
}

void UMALAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	CachedCharacter = Cast<AMALCharacterBase>(TryGetPawnOwner());
}

void UMALAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!CachedCharacter)
	{
		CachedCharacter = Cast<AMALCharacterBase>(TryGetPawnOwner());
		return;
	}

	UpdateMovementData(DeltaSeconds);
	UpdateAimOffset();
}

void UMALAnimInstance::AdvanceMontagesOnly(float DeltaSeconds)
{
	if (DeltaSeconds <= 0.f)
	{
		return;
	}

	// Montage_Advance is protected, so a subclass is the only place it can be driven from. It
	// touches nothing but the montage instances, which is exactly the state that has to keep
	// moving while the mesh is held still.
	Montage_Advance(DeltaSeconds);
}

void UMALAnimInstance::UpdateMovementData(float DeltaSeconds)
{
	const UCharacterMovementComponent* MovementComp = CachedCharacter->GetCharacterMovement();
	if (!MovementComp)
	{
		return;
	}

	const FVector Velocity = MovementComp->Velocity;
	Speed = Velocity.Size2D();
	VelocityZ = Velocity.Z;
	bIsInAir = MovementComp->IsFalling();

	const FVector Acceleration = MovementComp->GetCurrentAcceleration();

	// Direction answers "where is the player pushing" rather than "where is the body drifting".
	// Velocity lags a stick flick by a few frames and reads zero on the first frame of a turn,
	// which parks a strafe blend space on the forward cell. Input is available immediately.
	FVector InputDirection = MovementComp->GetPendingInputVector();
	if (InputDirection.IsNearlyZero())
	{
		InputDirection = MovementComp->GetLastInputVector();
	}
	InputDirection.Z = 0.0f;

	bHasMovementInput = !InputDirection.IsNearlyZero() || !Acceleration.IsNearlyZero();

	bIsLockedOn = IsLockedOn();
	bUseStrafeBlendSpace = bIsLockedOn;

	// Free movement is a 1D speed blend: the body always faces the travel direction, so there
	// is no meaningful angle to publish. Locked movement is the 2D blend: the body faces the
	// target through the controller, so the angle is measured against the controller's forward.
	if (bIsLockedOn)
	{
		FVector FacingForward = CachedCharacter->GetActorForwardVector().GetSafeNormal2D();
		if (const AController* Controller = CachedCharacter->GetController())
		{
			FacingForward = FRotator(0.0f, Controller->GetControlRotation().Yaw, 0.0f).Vector();
		}

		// Velocity is what the body is actually doing; input covers the first frames of a turn
		// where velocity has not built up yet.
		FVector MoveDirection = Speed >= 1.0f ? Velocity.GetSafeNormal2D() : InputDirection.GetSafeNormal2D();

		if (!MoveDirection.IsNearlyZero() && !FacingForward.IsNearlyZero())
		{
			const float Dot = FVector::DotProduct(FacingForward, MoveDirection);
			const float Cross = FVector::CrossProduct(FacingForward, MoveDirection).Z;
			const float TargetDirection = FMath::RadiansToDegrees(FMath::Atan2(Cross, Dot));

			// Tween along the shortest arc. Without this a left-to-right flick moves the axis 180
			// degrees in one frame and the blend space snaps between the two side-step poses.
			const float DeltaAngle = FMath::UnwindDegrees(TargetDirection - Direction);
			Direction = FMath::UnwindDegrees(
				FMath::FInterpTo(Direction, Direction + DeltaAngle, DeltaSeconds, DirectionInterpSpeed));
		}
		else
		{
			Direction = FMath::UnwindDegrees(
				FMath::FInterpTo(Direction, 0.0f, DeltaSeconds, DirectionInterpSpeed));
		}
	}
	else
	{
		// Free mode reads the 1D blend space, but the axis is still kept at 0 so a stale angle
		// cannot leak into anything that reads it.
		Direction = FMath::UnwindDegrees(
			FMath::FInterpTo(Direction, 0.0f, DeltaSeconds, DirectionInterpSpeed));
	}

	// The layer graphics read X/Y straight from here; C++ keeps the single source of truth for
	// the axis ranges and for "is this strafing".
	LocomotionBlendInput = FVector2D(Direction, Speed);
	bIsStrafing = bIsLockedOn && bHasMovementInput && FMath::Abs(Direction) >= StrafeDirectionThreshold;

	LocomotionState = CalculateLocomotionState(Speed);
}

void UMALAnimInstance::UpdateAimOffset()
{
	if (const AController* Controller = CachedCharacter->GetController())
	{
		AimRotation = Controller->GetControlRotation();
	}
	else
	{
		AimRotation = CachedCharacter->GetActorRotation();
	}

	const FRotator ActorRotation = CachedCharacter->GetActorRotation();
	const FRotator Delta = (AimRotation - ActorRotation).GetNormalized();
	AimOffset.Pitch = FMath::ClampAngle(Delta.Pitch, -90.f, 90.f);
	AimOffset.Yaw = FMath::ClampAngle(Delta.Yaw, -90.f, 90.f);
	AimOffset.Roll = 0.f;
}

EMALLocomotionState UMALAnimInstance::CalculateLocomotionState(float InSpeed) const
{
	if (InSpeed < JogSpeedThreshold)
	{
		return EMALLocomotionState::Idle;
	}
	if (InSpeed < SprintSpeedThreshold)
	{
		return EMALLocomotionState::Jog;
	}
	return EMALLocomotionState::Sprint;
}

AMALCharacterBase* UMALAnimInstance::GetMALCharacter() const
{
	return CachedCharacter.Get();
}

bool UMALAnimInstance::IsLockedOn() const
{
	if (!CachedCharacter)
	{
		return false;
	}

	const AMALPlayerController* PC = Cast<AMALPlayerController>(CachedCharacter->GetController());
	if (!PC)
	{
		return false;
	}

	const UMALLockOnComponent* LockOn = PC->GetLockOnComponent();
	return LockOn && LockOn->IsLocked();
}