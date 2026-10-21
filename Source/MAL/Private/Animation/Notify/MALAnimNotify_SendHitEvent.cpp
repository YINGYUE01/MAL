// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/Notify/MALAnimNotify_SendHitEvent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Tags/MALGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MALAnimNotify_SendHitEvent)

void UMALAnimNotify_SendHitEvent::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
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

	// Fall back to the project's default hit event tag when not overridden on the notify.
	FGameplayTag Tag = HitEventTag;
	if (!Tag.IsValid())
	{
		Tag = UMALGameplayTags::Get()->Event_Hit;
	}

	FGameplayEventData Payload;
	Payload.Instigator = Owner;
	Payload.EventTag = Tag;

	// The ability listens for this via UAbilityTask_WaitGameplayEvent (server-authoritative
	// hit resolution can be added there once enemies/damage exist).
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Owner, Tag, Payload);
}
