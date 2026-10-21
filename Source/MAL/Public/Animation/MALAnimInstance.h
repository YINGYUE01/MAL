// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "MALAnimInstance.generated.h"

class AMALCharacterBase;
class UCharacterMovementComponent;

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
	void UpdateMovementData(float DeltaSeconds);
	void UpdateAimOffset(float DeltaSeconds);
	EMALLocomotionState CalculateLocomotionState(float InSpeed) const;

private:
	UPROPERTY()
	TObjectPtr<AMALCharacterBase> CachedCharacter;
};