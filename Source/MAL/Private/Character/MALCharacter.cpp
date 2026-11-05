// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/MALCharacter.h"

#include "Camera/CameraComponent.h"
#include "Combat/MALLockOnComponent.h"
#include "GameAbilities/MALAbility_Dodge.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Player/MALPlayerController.h"
#include "Player/MALPlayerState.h"
#include "TimerManager.h"

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

	// Skills are weapon-driven; the dodge below is granted by GrantAbilities as the single
	// player-only exception, so no innate list is authored here.

	// Third-person rig, created here because BP_MALCharacter authors no working camera. Without
	// it the viewport falls back to the pawn's eyes. The names differ from the blueprint's own
	// CameraBoom/FollowCamera nodes: those still exist in its construction script and would
	// otherwise collide with the native properties, which breaks the blueprint compile.
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

void AMALCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Free movement by default, then let the controller's current lock state override it.
	ApplyCurrentLockMovementMode();
	BindLockOnComponent();

	// A Blueprint BeginPlay may re-enable Orient Rotation To Movement on the movement component
	// after this call. Re-assert one frame later so the locked rotation model cannot be silently
	// replaced: that is what makes a strafe turn the whole body instead of stepping sideways.
	GetWorldTimerManager().SetTimerForNextTick([this]()
	{
		ApplyCurrentLockMovementMode();
	});
}

void AMALCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// The controller (and its lock component) outlives the pawn; drop the binding so a respawn
	// does not leave this character registered on someone else's component.
	if (const AMALPlayerController* PC = Cast<AMALPlayerController>(GetController()))
	{
		if (UMALLockOnComponent* LockOn = PC->GetLockOnComponent())
		{
			LockOn->OnLockChanged.RemoveAll(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AMALCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// Respawning or a controller swap re-binds against the new controller.
	BindLockOnComponent();
}

void AMALCharacter::BindLockOnComponent()
{
	AMALPlayerController* PC = Cast<AMALPlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	UMALLockOnComponent* LockOn = PC->GetLockOnComponent();
	if (!LockOn)
	{
		return;
	}

	LockOn->OnLockChanged.RemoveAll(this);
	LockOn->OnLockChanged.AddDynamic(this, &AMALCharacter::HandleLockChanged);

	// The component may already hold a target (e.g. after a respawn): sync immediately.
	HandleLockChanged(LockOn->GetLockedTarget());
}

void AMALCharacter::HandleLockChanged(AActor* LockedTarget)
{
	ApplyLockMovementMode(LockedTarget != nullptr);
}

void AMALCharacter::ApplyCurrentLockMovementMode()
{
	bool bLocked = false;

	if (const AMALPlayerController* PC = Cast<AMALPlayerController>(GetController()))
	{
		if (const UMALLockOnComponent* LockOn = PC->GetLockOnComponent())
		{
			bLocked = LockOn->IsLocked();
		}
	}

	ApplyLockMovementMode(bLocked);
}

void AMALCharacter::ApplyLockMovementMode(bool bLocked)
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!Movement)
	{
		return;
	}

	// Free: the body turns toward the stick and runs forward.
	// Locked: the body keeps facing the target via the controller and steps sideways instead,
	// which is what makes a strafe blend space meaningful.
	Movement->bOrientRotationToMovement = !bLocked;
	Movement->bUseControllerDesiredRotation = bLocked;
	Movement->RotationRate = FRotator(0.0f, bLocked ? LockedRotationRateYaw : FreeRotationRateYaw, 0.0f);

	// The controller owns yaw in both modes here; the movement component above decides whether
	// the body follows it directly (locked) or turns toward velocity (free).
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
}

void AMALCharacter::GrantAbilities()
{
	Super::GrantAbilities();

	// The dodge is the one ability that is not carried by a weapon: it stays available while
	// unarmed. Everything else a player can cast comes from the equipped weapon's ability sets
	// and unlocked skill-tree nodes, rebuilt by the Super call above.
	GrantAbilityIfMissing(UMALAbility_Dodge::StaticClass());
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
