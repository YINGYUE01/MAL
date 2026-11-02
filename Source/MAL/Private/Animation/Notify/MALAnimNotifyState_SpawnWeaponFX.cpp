// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/Notify/MALAnimNotifyState_SpawnWeaponFX.h"

#include "Character/MALCharacterBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

namespace
{
	/** Picks the component the effect should hang from, or null when nothing usable exists. */
	USceneComponent* ResolveAttachComponent(
		const UMALAnimNotifyState_SpawnWeaponFX& Notify,
		USkeletalMeshComponent* MeshComp,
		AActor* Owner)
	{
		if (!Owner)
		{
			return nullptr;
		}

		// 1) Explicit component name wins: lets a montage target a prop or a second weapon actor.
		if (!Notify.AttachmentComponentName.IsNone())
		{
			TArray<UActorComponent*> Components;
			Owner->GetComponents(Components);
			for (UActorComponent* Component : Components)
			{
				if (Component && Component->GetFName() == Notify.AttachmentComponentName)
				{
					if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
					{
						return SceneComponent;
					}
				}
			}
		}

		// 2) The equipped weapon's visual: skeletal first, then static.
		if (const AMALCharacterBase* Character = Cast<AMALCharacterBase>(Owner))
		{
			USkeletalMeshComponent* SkeletalWeapon = Character->GetWeaponSkeletalMesh();
			UStaticMeshComponent* StaticWeapon = Character->GetWeaponMesh();

			const bool bSkeletalUsable = SkeletalWeapon
				&& SkeletalWeapon->IsVisible()
				&& SkeletalWeapon->GetSkeletalMeshAsset() != nullptr;

			const bool bStaticUsable = StaticWeapon
				&& StaticWeapon->IsVisible()
				&& StaticWeapon->GetStaticMesh() != nullptr;

			if (Notify.bPreferSkeletalWeapon ? bSkeletalUsable : (bSkeletalUsable && !bStaticUsable))
			{
				return SkeletalWeapon;
			}

			if (bStaticUsable)
			{
				return StaticWeapon;
			}

			if (bSkeletalUsable)
			{
				return SkeletalWeapon;
			}
		}

		// 3) No weapon equipped: the animating mesh keeps the effect visible somewhere sensible.
		return Notify.bFallbackToCharacterMesh ? static_cast<USceneComponent*>(MeshComp) : nullptr;
	}
}

FString UMALAnimNotifyState_SpawnWeaponFX::GetNotifyName_Implementation() const
{
	return TEXT("MAL Weapon FX State");
}

void UMALAnimNotifyState_SpawnWeaponFX::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!MeshComp || !SystemTemplate)
	{
		return;
	}

	const UWorld* World = MeshComp->GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}

	// Cosmetic, so it runs on clients; a dedicated server has nobody to render it for.
	if (World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner)
	{
		return;
	}

	USceneComponent* AttachComponent = ResolveAttachComponent(*this, MeshComp, Owner);
	if (!AttachComponent)
	{
		UE_LOG(LogTemp, Verbose,
			TEXT("UMALAnimNotifyState_SpawnWeaponFX: %s has no weapon visual for %s"),
			*GetNameSafe(Owner), *GetNameSafe(SystemTemplate));
		return;
	}

	const bool bHasSocket = !SocketName.IsNone() && AttachComponent->DoesSocketExist(SocketName);
	if (!SocketName.IsNone() && !bHasSocket)
	{
		UE_LOG(LogTemp, Verbose,
			TEXT("UMALAnimNotifyState_SpawnWeaponFX: socket '%s' missing on %s; attaching at the weapon root"),
			*SocketName.ToString(), *AttachComponent->GetName());
	}

	UNiagaraComponent* Spawned = nullptr;

	if (bUseSocketTransformOnly && bHasSocket)
	{
		// Attach at the mesh root and move to the socket, so user parameters authored in
		// weapon-mesh space keep their orientation instead of inheriting the socket's.
		Spawned = UNiagaraFunctionLibrary::SpawnSystemAttached(
			SystemTemplate,
			AttachComponent,
			NAME_None,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			EAttachLocation::SnapToTargetIncludingScale,
			false);

		if (Spawned)
		{
			const FTransform SocketTransform = AttachComponent->GetSocketTransform(SocketName, RTS_Component);
			const FVector LocalOffset = SocketTransform.GetLocation()
				+ SocketTransform.GetRotation().RotateVector(LocationOffset);

			Spawned->SetRelativeLocationAndRotation(LocalOffset, SocketTransform.Rotator() + RotationOffset);
		}
	}
	else
	{
		Spawned = UNiagaraFunctionLibrary::SpawnSystemAttached(
			SystemTemplate,
			AttachComponent,
			bHasSocket ? SocketName : NAME_None,
			LocationOffset,
			RotationOffset,
			EAttachLocation::KeepRelativeOffset,
			false);
	}

	if (!Spawned)
	{
		return;
	}

	if (!FMath::IsNearlyEqual(Scale, 1.0f))
	{
		Spawned->SetRelativeScale3D(FVector(Scale));
	}

	// Prune entries whose mesh is gone before recording the new one: a montage that was cut
	// without an End callback must not leak a dead handle into the next playback.
	for (int32 Index = ActiveEffects.Num() - 1; Index >= 0; --Index)
	{
		if (!ActiveEffects[Index].Mesh.IsValid() || !ActiveEffects[Index].Effect.IsValid())
		{
			ActiveEffects.RemoveAtSwap(Index);
		}
	}

	FMALWeaponFXInstance& Entry = ActiveEffects.AddDefaulted_GetRef();
	Entry.Mesh = MeshComp;
	Entry.Effect = Spawned;

	UE_LOG(LogTemp, Verbose, TEXT("UMALAnimNotifyState_SpawnWeaponFX: %s began on %s socket '%s'"),
		*GetNameSafe(SystemTemplate), *AttachComponent->GetName(),
		*(bHasSocket ? SocketName.ToString() : TEXT("None")));
}

void UMALAnimNotifyState_SpawnWeaponFX::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	// The effect is attached to the weapon mesh, so it follows the animation without any work
	// here. The override exists so a subclass can drive parameters over the window.
}

void UMALAnimNotifyState_SpawnWeaponFX::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	for (int32 Index = ActiveEffects.Num() - 1; Index >= 0; --Index)
	{
		const FMALWeaponFXInstance& Entry = ActiveEffects[Index];
		if (Entry.Mesh.Get() != MeshComp)
		{
			continue;
		}

		if (bDestroyOnEnd)
		{
			if (UNiagaraComponent* Effect = Entry.Effect.Get())
			{
				Effect->DestroyComponent();
			}
		}
		else if (UNiagaraComponent* Effect = Entry.Effect.Get())
		{
			Effect->Deactivate();
		}

		ActiveEffects.RemoveAtSwap(Index);
	}
}
