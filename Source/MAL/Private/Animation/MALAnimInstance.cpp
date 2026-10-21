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

	UpdateMovementData(DeltaSeconds);
	UpdateAimOffset(DeltaSeconds);
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

void UMALAnimInstance::UpdateAimOffset(float DeltaSeconds)
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