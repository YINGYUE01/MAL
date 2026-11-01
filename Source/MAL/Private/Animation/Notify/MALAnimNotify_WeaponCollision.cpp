// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/Notify/MALAnimNotify_WeaponCollision.h"

#include "Interfaces/MALComboAttackInterface.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MALAnimNotify_WeaponCollision)

void UMALAnimNotify_WeaponCollision::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	// Persona / AnimBlueprint preview worlds host a bare preview actor with no ASC, so there
	// is never an animating ability to route to. Bail out silently instead of warning on
	// every preview loop.
	const UWorld* World = MeshComp->GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner)
	{
		return;
	}

	// Simulated proxies have no active ability instance driving the montage; the ability
	// only runs on the server and the owning client. Skipping here avoids warning spam.
	if (const APawn* Pawn = Cast<APawn>(Owner))
	{
		if (!Pawn->IsLocallyControlled() && Pawn->GetLocalRole() != ROLE_Authority)
		{
			return;
		}
	}

	IMALComboAttackInterface* ComboAbility = IMALComboAttackInterface::GetFromMesh(MeshComp);
	if (!ComboAbility)
	{
		// Legitimate game case: the montage is being played outside of an ability (plain
		// PlayAnimMontage), so there is nothing to toggle collision on. Verbose keeps it
		// debuggable without spamming the log.
		UE_LOG(LogTemp, Verbose, TEXT("UMALAnimNotify_WeaponCollision: No combo ability driving %s (owner: %s)"),
			*GetNameSafe(MeshComp), *GetNameSafe(Owner));
		return;
	}

	if (bEnable)
	{
		ComboAbility->EnableWeaponCollision();
	}
	else
	{
		ComboAbility->DisableWeaponCollision();
	}
}
