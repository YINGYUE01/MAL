// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameAbilities/MALGameplayCueNotify_Buff.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

AMALGameplayCueNotify_Buff::AMALGameplayCueNotify_Buff()
{
	// Manual teardown of the spawned aura happens in OnRemove, so the actor itself must not
	// linger once the effect is gone.
	bAutoDestroyOnRemove = true;

	// GameplayCueTag is deliberately left empty here.
	//
	// The setup that has to be avoided: AMALGameplayCueNotify_Buff hands a valid tag down to
	// the Blueprint CDO. AGameplayCueNotify_Actor::PostInitProperties then sees child and parent
	// holding the same tag, clears the child's tag to re-derive it from the asset name, and when
	// that derivation fails it restores GameplayCueTag but leaves GameplayCueName at None.
	// GameplayCueName is the registry-searchable mirror that UGameplayCueManager reads to route
	// cue events, so the Blueprint would silently drop out of the cue set and never fire.
	//
	// Keeping the base tag empty makes DeriveGameplayCueTagFromClass skip that branch, so the
	// tag a Blueprint sets on itself is used as-is and GameplayCueName gets mirrored correctly.
	// One base class, one tag per Blueprint subclass.
}

bool AMALGameplayCueNotify_Buff::OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	if (!AuraEffect || !IsValid(MyTarget))
	{
		return false;
	}

	// Characters get the aura on the mesh so it follows the animated skeleton; anything else
	// falls back to the root component.
	USceneComponent* AttachParent = MyTarget->GetRootComponent();
	if (const ACharacter* TargetCharacter = Cast<ACharacter>(MyTarget))
	{
		if (USkeletalMeshComponent* TargetMesh = TargetCharacter->GetMesh())
		{
			AttachParent = TargetMesh;
		}
	}

	if (!AttachParent)
	{
		return false;
	}

	// A second OnActive for the same cue (buff refreshed while it is still running) must not
	// stack a second aura on top of the existing one.
	if (ActiveAura)
	{
		ActiveAura->DestroyComponent();
		ActiveAura = nullptr;
	}

	// bAutoDestroy is off on purpose: the component is torn down in OnRemove, so its lifetime
	// is pinned to the effect rather than to the system's own duration.
	ActiveAura = UNiagaraFunctionLibrary::SpawnSystemAttached(
		AuraEffect,
		AttachParent,
		AttachSocketName,
		AttachLocationOffset,
		AttachRotationOffset,
		AttachScale,
		EAttachLocation::KeepRelativeOffset,
		/*bAutoDestroy*/ false,
		ENCPoolMethod::None);

	return ActiveAura != nullptr;
}

bool AMALGameplayCueNotify_Buff::OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	// The effect is over, whatever the reason: take the aura down with it.
	if (ActiveAura)
	{
		ActiveAura->DestroyComponent();
		ActiveAura = nullptr;
	}

	return true;
}
