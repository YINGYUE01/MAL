// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/MALHitFeedbackComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/MALAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MALHitFeedbackComponent)

UMALHitFeedbackComponent::UMALHitFeedbackComponent()
{
	// Ticking is only needed while a freeze is running: it is what keeps the montage clock
	// moving under a frozen pose.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	// Purely local presentation; the server never runs the freezes this component applies.
	SetIsReplicatedByDefault(false);
}

void UMALHitFeedbackComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bHitStopActive)
	{
		return;
	}

	// The frozen mesh no longer runs TickPose, so nothing else advances its montage. Without this
	// the stop silently delays every remaining notify by its own duration, and the ability ends
	// late on this machine while the server has already moved on -- which is exactly what other
	// players see as a truncated swing or a body that slides away from the animation.
	if (UMALAnimInstance* AnimInstance = Cast<UMALAnimInstance>(HitStopAnimInstance.Get()))
	{
		AnimInstance->AdvanceMontagesOnly(DeltaTime);
	}
}

void UMALHitFeedbackComponent::BeginHitStop(float Duration)
{
	if (Duration <= 0.0f)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Hit-stop only ever fakes time on the machine watching it. A listen server's host is both
	// the local viewer and the authority, so the Client RPC that gets us here runs against the
	// authoritative world: freezing the pose there would stall the montage position the server
	// replicates, and every other client would see the attacker hitch. The host keeps the camera
	// shake, which is genuinely local.
	const ENetMode NetMode = World->GetNetMode();
	if (NetMode == NM_ListenServer || NetMode == NM_DedicatedServer)
	{
		return;
	}

	if (!bHitStopActive)
	{
		bHitStopActive = true;

		const APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
		if (const ACharacter* ControlledCharacter = PlayerController ? Cast<ACharacter>(PlayerController->GetPawn()) : nullptr)
		{
			if (USkeletalMeshComponent* Mesh = ControlledCharacter->GetMesh())
			{
				HitStopMesh = Mesh;
				HitStopAnimInstance = Mesh->GetAnimInstance();
				bHitStopRestoreNoSkeletonUpdate = Mesh->bNoSkeletonUpdate;

				// Hold the pose without touching the character's tick. Time dilation would be the
				// obvious lever, but it also stops the movement component from ticking, and a client
				// that stops ticking stops sending ServerMove: the server then has no movement or root
				// motion to apply and the character freezes, for everyone, until the stop ends. Keeping
				// the skeleton frozen is purely local, so the authoritative timeline never notices.
				Mesh->bNoSkeletonUpdate = true;
			}
		}

		// Tick only for the duration of the stop; see TickComponent for what it does.
		SetComponentTickEnabled(true);
	}

	// Overlapping hits extend the stop instead of releasing the pose too early.
	FTimerManager& TimerManager = World->GetTimerManager();
	TimerManager.ClearTimer(HitStopTimer);
	TimerManager.SetTimer(HitStopTimer, this, &UMALHitFeedbackComponent::EndHitStop, Duration, false);
}

void UMALHitFeedbackComponent::EndHitStop()
{
	bHitStopActive = false;
	SetComponentTickEnabled(false);

	if (USkeletalMeshComponent* Mesh = HitStopMesh.Get())
	{
		Mesh->bNoSkeletonUpdate = bHitStopRestoreNoSkeletonUpdate;
	}

	HitStopMesh = nullptr;
	HitStopAnimInstance = nullptr;
}

void UMALHitFeedbackComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HitStopTimer);
	}

	// Never leave the mesh frozen if the controller goes away in the middle of a hit-stop.
	EndHitStop();

	Super::EndPlay(EndPlayReason);
}
