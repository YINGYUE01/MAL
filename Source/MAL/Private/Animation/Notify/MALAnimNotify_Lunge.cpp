// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/Notify/MALAnimNotify_Lunge.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Interfaces/MALComboAttackInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MALAnimNotify_Lunge)

void UMALAnimNotify_Lunge::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	// Persona previews have no game world and no ability driving the montage.
	const UWorld* World = MeshComp->GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}

	if (IMALComboAttackInterface* Combo = IMALComboAttackInterface::GetFromMesh(MeshComp))
	{
		Combo->TriggerLunge();
		return;
	}

	// The frame is authored but nothing can act on it: either the montage is not driven by a
	// melee ability, or the ability already ended before this notify was reached.
	UE_LOG(LogTemp, Warning,
		TEXT("UMALAnimNotify_Lunge: %s has no animating melee ability (owner: %s)"),
		*GetNameSafe(MeshComp), *GetNameSafe(MeshComp->GetOwner()));
}
