// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/MALCharacterBase.h"
#include "MALCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;

/**
 * AMALCharacter
 *
 * Player character. Keeps the existing class name so BP_MALCharacter does not need a
 * redirect/reparent; all shared GAS setup now lives in AMALCharacterBase.
 *
 * The real ASC/AttributeSet still live on AMALPlayerState. AMALCharacterBase caches
 * pointers to them and initializes actor info with (PlayerState, this).
 */
UCLASS()
class MAL_API AMALCharacter : public AMALCharacterBase
{
	GENERATED_BODY()

public:
	AMALCharacter();

	/** The boom the aim zoom drives (see AMALCharacterBase::SetAiming). */
	UFUNCTION(BlueprintPure, Category = "MAL|Camera")
	USpringArmComponent* GetCameraBoom() const { return ThirdPersonBoom; }

	UFUNCTION(BlueprintPure, Category = "MAL|Camera")
	UCameraComponent* GetFollowCamera() const { return ThirdPersonCamera; }

	/**
	 * The pawn's view is this camera.
	 *
	 * BP_MALCharacter's construction script still contains a CameraBoom/FollowCamera pair left
	 * over from an earlier attempt at a rig: the camera hangs off the capsule rather than the
	 * boom, so it ignores the boom and every pitch input. Naming the native components
	 * differently keeps that asset compilable, and this override makes sure the view is taken
	 * from the working rig rather than from whichever camera the pawn happens to enumerate
	 * first.
	 */
	virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult) override;

protected:
	/** Players store the "default attributes applied" flag on the persistent PlayerState. */
	virtual bool AreDefaultAttributesApplied() const override;
	virtual void MarkDefaultAttributesApplied() override;

	/**
	 * Third-person rig.
	 *
	 * A pawn without a camera leaves the viewport falling back to its eyes, which is what made
	 * the aim pose read as "too close": the zoom had no arm to shorten. Authored in C++ rather
	 * than the character blueprint so every player pawn gets one; the blueprint can still
	 * override the arm length, the shoulder offset and the field of view.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MAL|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> ThirdPersonBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MAL|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> ThirdPersonCamera;
};
