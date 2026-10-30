// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/MALCharacter.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Player/MALPlayerState.h"

namespace
{
	/** Resting arm length and the shoulder offset the boom hangs at. */
	constexpr float DefaultArmLength = 320.0f;
	const FVector DefaultSocketOffset(0.0f, 50.0f, 60.0f);

	/** Resting field of view, wide enough to read the field around the character. */
	constexpr float DefaultCameraFOV = 90.0f;
}

AMALCharacter::AMALCharacter()
{
	// Without an explicit team the friendly-fire filter cannot tell a teammate from an enemy, so
	// every player-controlled pawn starts on the player side. Blueprints can still override it.
	TeamId = FGenericTeamId(MALTeam::Player);

	// Third-person rig, created here because BP_MALCharacter authors no working camera. Without
	// it the viewport falls back to the pawn's eyes and AMALCharacterBase's aim zoom has no
	// spring arm to shorten. The names differ from the blueprint's own CameraBoom/FollowCamera
	// nodes: those still exist in its construction script and would otherwise collide with the
	// native properties, which breaks the blueprint compile and with it every authored
	// component (weapon_collision included).
	ThirdPersonBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("ThirdPersonBoom"));
	ThirdPersonBoom->SetupAttachment(RootComponent);
	ThirdPersonBoom->TargetArmLength = DefaultArmLength;
	ThirdPersonBoom->SocketOffset = DefaultSocketOffset;
	// The boom follows the controller; the character keeps its own rotation policy, which is
	// what lets the aim stance strafe instead of turning.
	ThirdPersonBoom->bUsePawnControlRotation = true;
	ThirdPersonBoom->bDoCollisionTest = true;

	ThirdPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ThirdPersonCamera"));
	ThirdPersonCamera->SetupAttachment(ThirdPersonBoom, USpringArmComponent::SocketName);
	ThirdPersonCamera->bUsePawnControlRotation = false;
	ThirdPersonCamera->FieldOfView = DefaultCameraFOV;
}

void AMALCharacter::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
	if (ThirdPersonCamera && ThirdPersonCamera->IsActive())
	{
		ThirdPersonCamera->GetCameraView(DeltaTime, OutResult);
		return;
	}

	Super::CalcCamera(DeltaTime, OutResult);
}

bool AMALCharacter::AreDefaultAttributesApplied() const
{
	if (const AMALPlayerState* MALPS = GetPlayerState<AMALPlayerState>())
	{
		return MALPS->AreDefaultAttributesApplied();
	}

	return Super::AreDefaultAttributesApplied();
}

void AMALCharacter::MarkDefaultAttributesApplied()
{
	if (AMALPlayerState* MALPS = GetPlayerState<AMALPlayerState>())
	{
		MALPS->MarkDefaultAttributesApplied();
		return;
	}

	Super::MarkDefaultAttributesApplied();
}
