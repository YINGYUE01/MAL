// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/Notify/MALAnimNotify_Lunge.h"

#include "Interfaces/MALComboAttackInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MALAnimNotify_Lunge)

void UMALAnimNotify_Lunge::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (IMALComboAttackInterface* Combo = IMALComboAttackInterface::GetFromMesh(MeshComp))
	{
		Combo->TriggerLunge();
	}
}
