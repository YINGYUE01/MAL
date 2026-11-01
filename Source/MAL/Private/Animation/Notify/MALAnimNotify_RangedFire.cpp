// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/Notify/MALAnimNotify_RangedFire.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameAbilities/MALAbility_RangedAttack.h"
#include "GameFramework/Pawn.h"

FString UMALAnimNotify_RangedFire::GetNotifyName_Implementation() const
{
	return TEXT("MAL Ranged Fire");
}

void UMALAnimNotify_RangedFire::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	// Persona previews have no game world and no ability driving the preview montage.
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

	// Simulated proxies carry no active ability instance; only the server and the owning client
	// do, and the ability ignores clients for spawning.
	if (const APawn* Pawn = Cast<APawn>(Owner))
	{
		if (!Pawn->IsLocallyControlled() && Pawn->GetLocalRole() != ROLE_Authority)
		{
			return;
		}
	}

	UAbilitySystemComponent* AbilitySystem = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Owner);
	if (!AbilitySystem)
	{
		UE_LOG(LogTemp, Verbose, TEXT("UMALAnimNotify_RangedFire: %s has no ability system"), *Owner->GetName());
		return;
	}

	// Same lookup the weapon-collision notify uses: the ability currently driving this montage.
	UMALAbility_RangedAttack* RangedAbility = Cast<UMALAbility_RangedAttack>(AbilitySystem->GetAnimatingAbility());
	if (!RangedAbility)
	{
		UE_LOG(LogTemp, Verbose,
			TEXT("UMALAnimNotify_RangedFire: no UMALAbility_RangedAttack animating %s (owner: %s)"),
			*GetNameSafe(MeshComp), *Owner->GetName());
		return;
	}

	RangedAbility->NotifyFireProjectile();
}
