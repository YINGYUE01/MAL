// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/Notify/MALAnimNotify_WeaponCollision.h"

#include "Interfaces/MALComboAttackInterface.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MALAnimNotify_WeaponCollision)

void UMALAnimNotify_WeaponCollision::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp)
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
		UE_LOG(LogTemp, Warning, TEXT("UMALAnimNotify_WeaponCollision: No combo ability found on mesh"));
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
