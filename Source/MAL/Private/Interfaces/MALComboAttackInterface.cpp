// Copyright Epic Games, Inc. All Rights Reserved.

#include "Interfaces/MALComboAttackInterface.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Abilities/GameplayAbility.h"
#include "Components/SkeletalMeshComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MALComboAttackInterface)

IMALComboAttackInterface* IMALComboAttackInterface::GetFromMesh(USkeletalMeshComponent* MeshComp)
{
	if (!MeshComp)
	{
		return nullptr;
	}

	AActor* Owner = MeshComp->GetOwner();
	const IAbilitySystemInterface* ASCInterface = Cast<IAbilitySystemInterface>(Owner);
	if (!ASCInterface)
	{
		return nullptr;
	}

	UAbilitySystemComponent* ASC = ASCInterface->GetAbilitySystemComponent();
	if (!ASC)
	{
		return nullptr;
	}

	// The ability that is currently driving the montage on this mesh.
	UGameplayAbility* AnimatingAbility = ASC->GetAnimatingAbility();
	return Cast<IMALComboAttackInterface>(AnimatingAbility);
}
