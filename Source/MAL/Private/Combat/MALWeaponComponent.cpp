// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/MALWeaponComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Combat/MALDamageExecutor.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "Tags/MALGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MALWeaponComponent)

UMALWeaponComponent::UMALWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// The hit window is driven by the montage notifies that already replicate through the
	// ability; the component itself has no state a remote client needs.
	SetIsReplicatedByDefault(false);
}

void UMALWeaponComponent::StartHitWindow(int32 ComboStep, FName CollisionComponentName)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		// Hit detection is server-authoritative. On a client every overlap would be discarded
		// anyway, so skip the lookup and the binding entirely.
		return;
	}

	// A window that was never closed would otherwise keep the previous overlap binding.
	ReleaseCollision();

	CurrentComboStep = ComboStep;
	ActiveComponentName = CollisionComponentName;

	CollisionComponent = FindCollisionComponent(CollisionComponentName);
	if (!CollisionComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("UMALWeaponComponent::StartHitWindow - No component named '%s' found on %s!"),
			*CollisionComponentName.ToString(), *Owner->GetName());
		return;
	}

	HitActors.Reset();
	bHitWindowOpen = true;

	// QueryOnly + Pawn overlap: the weapon volume never blocks the world, it only reports
	// characters passing through it during the swing.
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComponent->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CollisionComponent->SetGenerateOverlapEvents(true);

	CollisionComponent->OnComponentBeginOverlap.AddDynamic(this, &UMALWeaponComponent::HandleOverlapBegin);
}

void UMALWeaponComponent::EndHitWindow()
{
	ReleaseCollision();
}

void UMALWeaponComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// A character dying mid-swing still has overlap bound to a component that is about to go
	// away; release it before the owner tears down.
	ReleaseCollision();

	Super::EndPlay(EndPlayReason);
}

UPrimitiveComponent* UMALWeaponComponent::FindCollisionComponent(FName ComponentName) const
{
	const AActor* Owner = GetOwner();
	if (!Owner || ComponentName.IsNone())
	{
		return nullptr;
	}

	TArray<UActorComponent*> Components;
	Owner->GetComponents(Components);
	for (UActorComponent* Component : Components)
	{
		UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component);
		if (Primitive && Primitive->GetFName() == ComponentName)
		{
			return Primitive;
		}
	}

	return nullptr;
}

void UMALWeaponComponent::ReleaseCollision()
{
	bHitWindowOpen = false;
	HitActors.Reset();

	if (CollisionComponent)
	{
		CollisionComponent->OnComponentBeginOverlap.RemoveDynamic(this, &UMALWeaponComponent::HandleOverlapBegin);
		CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		CollisionComponent = nullptr;
	}
}

void UMALWeaponComponent::HandleOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	AActor* Owner = GetOwner();
	if (!bHitWindowOpen || !Owner || !Owner->HasAuthority())
	{
		return;
	}

	if (!OtherActor || OtherActor == Owner)
	{
		return;
	}

	// One hit per actor per swing. The set holds weak pointers, so a target destroyed
	// mid-window cannot leave a dangling entry behind.
	const TWeakObjectPtr<AActor> OtherActorKey(OtherActor);
	if (HitActors.Contains(OtherActorKey))
	{
		return;
	}

	if (UMALDamageExecutor::IsFriendly(Owner, OtherActor))
	{
		return;
	}

	// Corpses keep their collision. A swing passing over one must not report a hit: the damage
	// would be zeroed by Health already being zero, but the number would still be drawn.
	const UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OtherActor);
	if (TargetASC && TargetASC->HasMatchingGameplayTag(UMALGameplayTags::Get()->Status_Dead))
	{
		return;
	}

	HitActors.Add(OtherActorKey);

	FMALHitResult HitResult;
	HitResult.HitActor = OtherActor;
	HitResult.WeaponSocketName = ActiveComponentName;
	HitResult.ComboStep = CurrentComboStep;

	if (bFromSweep && SweepResult.bBlockingHit)
	{
		HitResult.HitLocation = SweepResult.ImpactPoint;
	}
	else if (OtherComp)
	{
		// Overlap events carry no impact point, and a component location is its pivot - for a
		// character mesh that is at the feet. Ask the target for the point closest to the weapon
		// volume so the impact cue and 3D audio land where the hit actually happened.
		const FVector WeaponLocation = OverlappedComp ? OverlappedComp->GetComponentLocation() : Owner->GetActorLocation();

		FVector ClosestPoint(ForceInit);
		if (OtherComp->GetClosestPointOnCollision(WeaponLocation, ClosestPoint) >= 0.0f)
		{
			HitResult.HitLocation = ClosestPoint;
		}
		else
		{
			// Target has no query collision (overlap-only setup); its pivot is all we have.
			HitResult.HitLocation = OtherComp->GetComponentLocation();
		}
	}
	else
	{
		HitResult.HitLocation = OverlappedComp ? OverlappedComp->GetComponentLocation() : FVector::ZeroVector;
	}

	OnWeaponHit.Broadcast(HitResult);
}
