// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/Notify/MALAnimNotify_ComboWindow.h"

#include "Interfaces/MALComboAttackInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MALAnimNotify_ComboWindow)

void UMALAnimNotify_ComboWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (IMALComboAttackInterface* Combo = IMALComboAttackInterface::GetFromMesh(MeshComp))
	{
		Combo->OpenComboWindow();
	}
}

void UMALAnimNotify_ComboWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (IMALComboAttackInterface* Combo = IMALComboAttackInterface::GetFromMesh(MeshComp))
	{
		Combo->CloseComboWindow();
	}
}
