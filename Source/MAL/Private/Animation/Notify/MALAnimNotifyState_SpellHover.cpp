// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/Notify/MALAnimNotifyState_SpellHover.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameAbilities/MALAbility_SpellCast.h"
#include "GameFramework/Pawn.h"

static UMALAbility_SpellCast* GetAnimatingSpellForHover(USkeletalMeshComponent* MeshComp)
{
	if (!MeshComp || !MeshComp->GetWorld() || !MeshComp->GetWorld()->IsGameWorld())
	{
		return nullptr;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	if (const APawn* Pawn = Cast<APawn>(Owner))
	{
		if (!Pawn->IsLocallyControlled() && !Pawn->HasAuthority())
		{
			return nullptr;
		}
	}

	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Owner);
	return ASC ? Cast<UMALAbility_SpellCast>(ASC->GetAnimatingAbility()) : nullptr;
}

FString UMALAnimNotifyState_SpellHover::GetNotifyName_Implementation() const
{
	return TEXT("MAL Spell Hover");
}

void UMALAnimNotifyState_SpellHover::NotifyBegin(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (UMALAbility_SpellCast* Spell = GetAnimatingSpellForHover(MeshComp))
	{
		Spell->NotifyBeginHover();
	}
}
