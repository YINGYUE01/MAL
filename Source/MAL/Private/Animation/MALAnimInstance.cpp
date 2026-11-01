// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/MALAnimInstance.h"
#include "Character/MALCharacterBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PawnMovementComponent.h"

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

	UpdateMovementData();
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

void UMALAnimInstance::UpdateMovementData()
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
	bHasMovementInput = !Acceleration.IsNearlyZero();

	// CalculateDirection: angle between Velocity (XY) and Actor forward direction
	{
		const FVector Velocity2D = Velocity.GetSafeNormal2D();
		const FVector Forward = CachedCharacter->GetActorForwardVector().GetSafeNormal2D();
		const float Dot = FVector::DotProduct(Forward, Velocity2D);
		const float Cross = FVector::CrossProduct(Forward, Velocity2D).Z;
		Direction = FMath::RadiansToDegrees(FMath::Atan2(Cross, Dot));
	}

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