// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/MALLockOnComponent.h"

#include "Character/MALCharacterBase.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GenericTeamAgentInterface.h"

UMALLockOnComponent::UMALLockOnComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;

	// Nothing to do until a target exists: an empty lock costs nothing per frame.
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UMALLockOnComponent::BeginPlay()
{
	Super::BeginPlay();

	// A previous possession may have left a target behind; starting clean keeps the first
	// toggle from immediately releasing something the player never asked for.
	ClearLock();
}

void UMALLockOnComponent::ToggleLock()
{
	if (LockedTarget.IsValid())
	{
		ClearLock();
		return;
	}

	AActor* Target = FindBestTarget();
	if (!Target)
	{
		UE_LOG(LogTemp, Verbose, TEXT("MALLockOn: nothing hostile in view to lock onto."));
		return;
	}

	LockedTarget = Target;
	TimeSinceSightCheck = 0.0f;
	bHadLineOfSight = true;
	SetComponentTickEnabled(true);

	OnLockChanged.Broadcast(Target);
	UE_LOG(LogTemp, Log, TEXT("MALLockOn: locked %s."), *Target->GetName());
}

void UMALLockOnComponent::ClearLock()
{
	if (!LockedTarget.IsValid())
	{
		SetComponentTickEnabled(false);
		return;
	}

	AActor* Previous = LockedTarget.Get();
	LockedTarget.Reset();
	SetComponentTickEnabled(false);

	OnLockChanged.Broadcast(nullptr);
	UE_LOG(LogTemp, Log, TEXT("MALLockOn: released %s."), *GetNameSafe(Previous));
}

void UMALLockOnComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	APlayerController* PlayerController = GetPlayerController();
	APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	AActor* Target = LockedTarget.Get();

	if (!Pawn || !Target)
	{
		ClearLock();
		return;
	}

	// A corpse keeps its collision for a while; holding a lock on it would keep the camera
	// pointed at the ground where it fell.
	const AMALCharacterBase* TargetCharacter = Cast<AMALCharacterBase>(Target);
	if (!TargetCharacter || TargetCharacter->IsDead())
	{
		ClearLock();
		return;
	}

	if (FVector::DistSquared(Pawn->GetActorLocation(), Target->GetActorLocation())
		> FMath::Square(MaxBreakDistance))
	{
		ClearLock();
		return;
	}

	// Traced on an interval rather than per frame: a three-hundred-unit capsule against the
	// level is cheap, but doing it sixty times a second for a lock that rarely changes is not.
	TimeSinceSightCheck += DeltaTime;
	if (TimeSinceSightCheck >= LineOfSightCheckInterval)
	{
		TimeSinceSightCheck = 0.0f;
		bHadLineOfSight = HasLineOfSight(Target);
	}

	if (!bHadLineOfSight)
	{
		// Wall between the two: the player can no longer see what the camera is chasing.
		ClearLock();
		return;
	}

	FaceTarget(Target, DeltaTime);
}

void UMALLockOnComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearLock();

	Super::EndPlay(EndPlayReason);
}

APlayerController* UMALLockOnComponent::GetPlayerController() const
{
	return Cast<APlayerController>(GetOwner());
}

AActor* UMALLockOnComponent::FindBestTarget() const
{
	APlayerController* PlayerController = GetPlayerController();
	APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	UWorld* World = GetWorld();
	if (!Pawn || !World)
	{
		return nullptr;
	}

	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector ViewDirection = ViewRotation.Vector();

	const float CosCone = FMath::Cos(FMath::DegreesToRadians(ViewConeDegrees));

	AActor* BestTarget = nullptr;
	float BestScore = -1.0f;

	for (TActorIterator<AMALCharacterBase> It(World); It; ++It)
	{
		AMALCharacterBase* Candidate = *It;
		if (!Candidate || !IsCandidateValid(Candidate))
		{
			continue;
		}

		const FVector ToTarget = Candidate->GetActorLocation() - ViewLocation;
		const float Distance = ToTarget.Size();
		if (Distance <= KINDA_SMALL_NUMBER || Distance > MaxLockDistance)
		{
			continue;
		}

		const float Facing = FVector::DotProduct(ViewDirection, ToTarget / Distance);
		if (Facing < CosCone)
		{
			continue;
		}

		if (!HasLineOfSight(Candidate))
		{
			continue;
		}

		// Centred beats near: the target under the crosshair is the one the player meant, even
		// when a closer body happens to sit inside the same cone. Distance only breaks ties.
		const float Score = Facing - 0.0001f * Distance;
		if (Score > BestScore)
		{
			BestScore = Score;
			BestTarget = Candidate;
		}
	}

	return BestTarget;
}

bool UMALLockOnComponent::IsCandidateValid(const AActor* Candidate) const
{
	const APlayerController* PlayerController = GetPlayerController();
	const APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	const AMALCharacterBase* Character = Cast<AMALCharacterBase>(Candidate);

	if (!Pawn || !Character || Character == Pawn || Character->IsDead())
	{
		return false;
	}

	// Attitude rather than team ids: unassigned pawns are neutral, and a neutral body must not
	// become a lock target just because both sides happen to have no team.
	return FGenericTeamId::GetAttitude(Pawn, Candidate) == ETeamAttitude::Hostile;
}

bool UMALLockOnComponent::HasLineOfSight(const AActor* Target) const
{
	APlayerController* PlayerController = GetPlayerController();
	APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	UWorld* World = GetWorld();
	if (!Pawn || !Target || !World)
	{
		return false;
	}

	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);

	const FVector AimPoint = Target->GetActorLocation() + FVector(0.0f, 0.0f, AimHeightOffset);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(MALLockOnSight), false, Pawn);
	Params.AddIgnoredActor(Target);

	FHitResult Hit;
	const bool bBlocked = World->LineTraceSingleByChannel(
		Hit, ViewLocation, AimPoint, ECC_Visibility, Params);

	return !bBlocked;
}

void UMALLockOnComponent::FaceTarget(const AActor* Target, float DeltaTime)
{
	APlayerController* PlayerController = GetPlayerController();
	APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	if (!Pawn || !Target)
	{
		return;
	}

	FVector ToTarget = Target->GetActorLocation() - Pawn->GetActorLocation();
	ToTarget.Z = 0.0f;
	if (ToTarget.IsNearlyZero())
	{
		return;
	}

	// Only yaw is driven: pitch belongs to the player, otherwise looking up and down would
	// fight the lock every frame.
	const FRotator Current = PlayerController->GetControlRotation();
	const FRotator Desired(Current.Pitch, ToTarget.Rotation().Yaw, Current.Roll);

	PlayerController->SetControlRotation(
		FMath::RInterpTo(Current, Desired, DeltaTime, YawInterpSpeed));
}
