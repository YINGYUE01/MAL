// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/MALWeaponComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Character/MALCharacterBase.h"
#include "Combat/MALDamageExecutor.h"
#include "Combat/MALWeaponDefinition.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Tags/MALGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MALWeaponComponent)

namespace
{
	/**
	 * Parent a weapon visual to the character's hand socket and apply the definition's offset.
	 *
	 * Both weapon components go through this: a definition that ships a static mesh and one that
	 * ships a skeletal mesh are meant to be interchangeable, so their placement cannot diverge.
	 */
	void PlaceWeaponVisual(USceneComponent* Visual, USkeletalMeshComponent* BodyMesh, const UMALWeaponDefinition& Definition)
	{
		if (!Visual)
		{
			return;
		}

		if (BodyMesh)
		{
			Visual->AttachToComponent(BodyMesh,
				FAttachmentTransformRules::SnapToTargetNotIncludingScale,
				Definition.AttachSocketName);
		}

		Visual->SetRelativeLocation(Definition.MeshRelativeLocation);
		Visual->SetRelativeRotation(Definition.MeshRelativeRotation);
		Visual->SetRelativeScale3D(Definition.MeshRelativeScale);
	}
}

UMALWeaponComponent::UMALWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// Which weapon is equipped changes what every machine draws, so that choice replicates.
	// The hit window does not: it stays driven by the montage notifies on the server.
	SetIsReplicatedByDefault(true);
}

void UMALWeaponComponent::BeginPlay()
{
	Super::BeginPlay();

	const AActor* Owner = GetOwner();
	if (Owner && Owner->HasAuthority())
	{
		// Routed through EquipWeapon so the authority takes the same path as a client that
		// receives the replicated value. The first loadout entry is the starting weapon; an empty
		// list still goes through EquipWeapon so the bar and the hit box get their first broadcast.
		EquipWeapon(Loadout.Num() > 0 ? Loadout[0].Get() : nullptr);
	}
	else
	{
		// A client can begin play before the replicated value arrives. Start from the unarmed
		// state; OnRep_EquippedWeapon applies whatever the server actually chose.
		ApplyVisuals();
		ApplyWeaponHitBox();
		ApplyAmmo();
	}
}

void UMALWeaponComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UMALWeaponComponent, EquippedWeapon);
	DOREPLIFETIME(UMALWeaponComponent, Ammo);
}

void UMALWeaponComponent::OnRep_EquippedWeapon()
{
	// The definition is an asset, so a client has everything it needs to rebuild the same visuals
	// and hit volume. Hit detection is deliberately not in this path: it stays on the authority.
	ApplyVisuals();
	ApplyWeaponHitBox();
	ApplyAmmo();

	// The abilities themselves were granted on the server and replicate on their own; this is
	// only the cue for listeners that resolve them again.
	OnEquippedWeaponChanged.Broadcast();
}

void UMALWeaponComponent::OnRep_Ammo()
{
	// Ammo drives UI only, so the replicated value needs no further application.
	OnAmmoChanged.Broadcast();
}

void UMALWeaponComponent::EquipWeapon(UMALWeaponDefinition* NewWeapon)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	// The old weapon's window is still bound to the old hit volume. Closing it here means the new
	// weapon never inherits a live overlap or a half-filled hit set.
	ReleaseCollision();

	if (EquippedWeapon == NewWeapon)
	{
		// Handing over the same asset is still a request to re-apply its setup.
		ApplyVisuals();
		ApplyWeaponHitBox();
		ApplyAmmo();
		ApplyAbilities();
		OnEquippedWeaponChanged.Broadcast();
		return;
	}

	EquippedWeapon = NewWeapon;

	// OnRep only runs on clients, so the authority applies the swap by hand.
	ApplyVisuals();
	ApplyWeaponHitBox();
	ApplyAmmo();

	// After the definition has landed: the ability layer reads the equipped weapon, so handing
	// it over before the assignment would rebuild from the previous one.
	ApplyAbilities();
	OnEquippedWeaponChanged.Broadcast();
}

void UMALWeaponComponent::ApplyAbilities()
{
	// Delegated to the character: the ability system can outlive this component (it lives on the
	// player state) and is not always ready when BeginPlay equips the starting weapon, so the
	// grant has to be owned by whoever also grants the class kit and can retry.
	if (AMALCharacterBase* Character = Cast<AMALCharacterBase>(GetOwner()))
	{
		Character->RefreshWeaponGrantedAbilities();
	}
}

void UMALWeaponComponent::UnequipWeapon()
{
	EquipWeapon(nullptr);
}

void UMALWeaponComponent::CycleWeapon(int32 Direction)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	const int32 Count = Loadout.Num();
	if (Count < 2)
	{
		// One weapon is a loadout that never changes, and re-equipping it would only re-apply
		// the same definition.
		return;
	}

	// Empty-handed has no slot, so the first press lands on an end of the list rather than
	// wrapping from nowhere: forward starts at the first weapon, backward at the last.
	const int32 Current = EquippedWeapon ? Loadout.IndexOfByKey(EquippedWeapon) : INDEX_NONE;
	const int32 Step = Direction < 0 ? -1 : 1;
	const int32 Next = Current == INDEX_NONE
		? (Step > 0 ? 0 : Count - 1)
		: (Current + Step + Count) % Count;

	UE_LOG(LogTemp, Log, TEXT("CycleWeapon: %s -> %s (slot %d/%d)."),
		*GetNameSafe(Owner), *GetNameSafe(Loadout[Next]), Next + 1, Count);

	EquipWeapon(Loadout[Next]);
}

float UMALWeaponComponent::GetDamageMultiplier() const
{
	return EquippedWeapon ? EquippedWeapon->DamageMultiplier : 1.0f;
}

void UMALWeaponComponent::ApplyAmmo()
{
	// Unarmed and unlimited weapons both read as INDEX_NONE: there is no number to show and
	// nothing to spend, so a UI that draws the count simply draws nothing.
	const int32 NewAmmo = EquippedWeapon ? EquippedWeapon->MagazineSize : INDEX_NONE;
	if (Ammo == NewAmmo)
	{
		return;
	}

	Ammo = NewAmmo;
	OnAmmoChanged.Broadcast();
}

bool UMALWeaponComponent::TryConsumeAmmo(int32 Count)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		// The round count is spent on the server only; a client that predicted it would desync
		// from the replicated value on the next shot.
		return false;
	}

	if (Count <= 0 || Ammo == INDEX_NONE)
	{
		return true;
	}

	if (Ammo < Count)
	{
		UE_LOG(LogTemp, Verbose, TEXT("TryConsumeAmmo: %s is empty (%d left, %d requested)."),
			*GetNameSafe(Owner), Ammo, Count);
		return false;
	}

	Ammo -= Count;
	OnAmmoChanged.Broadcast();
	return true;
}

void UMALWeaponComponent::StartHitWindow(int32 ComboStep)
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

	// The hit volume is always the box generated on the equipped weapon; there is no character
	// authored box and no name lookup any more.
	if (!WeaponHitBox)
	{
		ApplyWeaponHitBox();
	}

	CollisionComponent = WeaponHitBox;
	if (!CollisionComponent)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UMALWeaponComponent::StartHitWindow - no weapon hit volume on %s (component %s, weapon %s)"),
			*Owner->GetName(), *GetName(), *GetNameSafe(EquippedWeapon));
		return;
	}

	ActiveComponentName = CollisionComponent->GetFName();

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

	UE_LOG(LogTemp, Log, TEXT("UMALWeaponComponent::StartHitWindow - %s opened on %s (%s, step %d, extent %s)"),
		*Owner->GetName(), *CollisionComponent->GetName(), *GetNameSafe(EquippedWeapon), ComboStep,
		*CollisionComponent->Bounds.BoxExtent.ToCompactString());
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
	DestroyWeaponHitBox();

	Super::EndPlay(EndPlayReason);
}

void UMALWeaponComponent::ApplyVisuals()
{
	AMALCharacterBase* Character = Cast<AMALCharacterBase>(GetOwner());
	if (!Character)
	{
		return;
	}

	// Restore the blade the previous weapon hid before deciding what the new one hides. The slot
	// name cannot be read back from EquippedWeapon, which already points at the new definition.
	if (!HiddenBuiltInSlot.IsNone())
	{
		Character->SetBuiltInWeaponHidden(false, HiddenBuiltInSlot);
		HiddenBuiltInSlot = NAME_None;
	}

	UStaticMeshComponent* StaticMeshComponent = Character->GetWeaponMesh();
	USkeletalMeshComponent* SkeletalMeshComponent = Character->GetWeaponSkeletalMesh();
	if (!StaticMeshComponent && !SkeletalMeshComponent)
	{
		return;
	}

	// Rebuilt on every swap: socket, offset and trail all come from the definition.
	if (TrailComponent)
	{
		TrailComponent->Deactivate();
		TrailComponent->DestroyComponent();
		TrailComponent = nullptr;
	}

	// A rigged weapon takes the skeletal slot and blanks the static one; a plain blade does the
	// opposite. Both are cleared when nothing is equipped, so a swap never leaves the previous
	// weapon's mesh behind.
	const bool bUseSkeletal = EquippedWeapon && EquippedWeapon->WeaponSkeletalMesh != nullptr;
	const bool bDrawStatic = EquippedWeapon && !bUseSkeletal && EquippedWeapon->WeaponMesh != nullptr;

	if (StaticMeshComponent)
	{
		StaticMeshComponent->SetStaticMesh(bDrawStatic ? EquippedWeapon->WeaponMesh.Get() : nullptr);
		StaticMeshComponent->SetVisibility(bDrawStatic);
	}

	if (SkeletalMeshComponent)
	{
		SkeletalMeshComponent->SetSkeletalMeshAsset(bUseSkeletal ? EquippedWeapon->WeaponSkeletalMesh.Get() : nullptr);

		// Reference pose is the right default: the swing comes from the character, not from the
		// weapon, so a weapon without an animation of its own is not a defect.
		SkeletalMeshComponent->SetAnimInstanceClass(bUseSkeletal ? EquippedWeapon->WeaponAnimClass.Get() : nullptr);
		SkeletalMeshComponent->SetVisibility(bUseSkeletal);
	}

	if (!EquippedWeapon)
	{
		return;
	}

	USkeletalMeshComponent* BodyMesh = Character->GetMesh();
	PlaceWeaponVisual(StaticMeshComponent, BodyMesh, *EquippedWeapon);
	PlaceWeaponVisual(SkeletalMeshComponent, BodyMesh, *EquippedWeapon);

	// Only a definition that draws something of its own may hide the character's blade. A skin
	// has no mesh, and hiding the blade for it would leave the character visibly unarmed.
	if (EquippedWeapon->bHideBuiltInWeapon && (EquippedWeapon->WeaponMesh || EquippedWeapon->WeaponSkeletalMesh))
	{
		Character->SetBuiltInWeaponHidden(true, EquippedWeapon->BuiltInWeaponMaterialSlot);
		HiddenBuiltInSlot = EquippedWeapon->BuiltInWeaponMaterialSlot;
	}

	if (EquippedWeapon->TrailEffect)
	{
		USceneComponent* TrailParent = bUseSkeletal
			? static_cast<USceneComponent*>(SkeletalMeshComponent)
			: static_cast<USceneComponent*>(StaticMeshComponent);

		if (TrailParent)
		{
			// Attached to the mesh without a socket on purpose. A socket hands over its own frame,
			// and a socket added to one of these rigid weapon meshes inherits its bone's
			// orientation rather than the mesh's, which tilts every user parameter authored in
			// mesh space away from the blade. Position comes from the socket below instead, so
			// naming a socket moves the effect without changing its shape.
			TrailComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
				EquippedWeapon->TrailEffect,
				TrailParent,
				NAME_None,
				FVector::ZeroVector,
				FRotator::ZeroRotator,
				EAttachLocation::SnapToTargetIncludingScale,
				false);

			if (TrailComponent && !EquippedWeapon->TrailSocketName.IsNone())
			{
				if (TrailParent->DoesSocketExist(EquippedWeapon->TrailSocketName))
				{
					TrailComponent->SetRelativeLocation(
						TrailParent->GetSocketTransform(EquippedWeapon->TrailSocketName, RTS_Component).GetLocation());
				}
				else
				{
					// Says which weapon asked for it: the engine's own socket warning names the
					// component and leaves the definition to be guessed at.
					UE_LOG(LogTemp, Warning,
						TEXT("Trail socket '%s' not found on '%s'; the trail starts at the weapon root instead."),
						*EquippedWeapon->TrailSocketName.ToString(), *TrailParent->GetName());
				}
			}
		}
	}
}

USceneComponent* UMALWeaponComponent::GetActiveWeaponVisual() const
{
	const AMALCharacterBase* Character = Cast<AMALCharacterBase>(GetOwner());
	if (!Character || !EquippedWeapon)
	{
		return nullptr;
	}

	// Mirror ApplyVisuals: a rigged weapon takes the skeletal slot, a plain blade the static one.
	if (EquippedWeapon->WeaponSkeletalMesh && Character->GetWeaponSkeletalMesh())
	{
		return Character->GetWeaponSkeletalMesh();
	}

	if (EquippedWeapon->WeaponMesh && Character->GetWeaponMesh())
	{
		return Character->GetWeaponMesh();
	}

	return nullptr;
}

void UMALWeaponComponent::ApplyWeaponHitBox()
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		// Collision only matters on the server; clients draw the weapon and nothing else.
		return;
	}

	USceneComponent* Visual = GetActiveWeaponVisual();
	if (!Visual || !EquippedWeapon)
	{
		DestroyWeaponHitBox();
		return;
	}

	if (!WeaponHitBox || WeaponHitBox->GetAttachParent() != Visual)
	{
		DestroyWeaponHitBox();

		WeaponHitBox = NewObject<UBoxComponent>(Owner);
		WeaponHitBox->SetupAttachment(Visual);
		WeaponHitBox->RegisterComponent();
		WeaponHitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WeaponHitBox->SetCollisionObjectType(ECC_WorldDynamic);
		WeaponHitBox->SetGenerateOverlapEvents(false);
		WeaponHitBox->SetCanEverAffectNavigation(false);

		UE_LOG(LogTemp, Log, TEXT("UMALWeaponComponent: runtime hit box created on %s (%s, component %s)"),
			*Visual->GetName(), *GetNameSafe(EquippedWeapon), *GetName());
	}

	FVector Extent = EquippedWeapon->HitBoxExtent;
	FVector Location = EquippedWeapon->HitBoxRelativeLocation;

	if (EquippedWeapon->bAutoFitHitBox)
	{
		// Local bounds keep the box in the weapon mesh's own space, so the socket offset, scale
		// and rotation authored on the definition all apply without a second set of numbers.
		const FBoxSphereBounds LocalBounds = Visual->CalcBounds(FTransform::Identity);
		Extent = LocalBounds.BoxExtent;

		const int32 LengthAxis = FMath::Clamp(static_cast<int32>(EquippedWeapon->HitBoxLengthAxis), 0, 2);
		const float Thickness = FMath::Clamp(EquippedWeapon->HitBoxScale, 0.1f, 1.0f);
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			if (Axis != LengthAxis)
			{
				Extent[Axis] *= Thickness;
			}
		}

		Location = LocalBounds.Origin + EquippedWeapon->HitBoxRelativeLocation;
	}

	WeaponHitBox->SetBoxExtent(Extent, /*bUpdateOverlaps*/ false);
	WeaponHitBox->SetRelativeLocation(Location);

	UE_LOG(LogTemp, Log, TEXT("UMALWeaponComponent: runtime hit box on %s extent %s location %s"),
		*Visual->GetName(), *Extent.ToCompactString(), *Location.ToCompactString());
}

void UMALWeaponComponent::DestroyWeaponHitBox()
{
	if (!WeaponHitBox)
	{
		return;
	}

	WeaponHitBox->DestroyComponent();
	WeaponHitBox = nullptr;
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

	UE_LOG(LogTemp, Verbose, TEXT("UMALWeaponComponent %s: overlap with %s during the hit window."),
		*Owner->GetName(), *OtherActor->GetName());

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
