// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameAbilities/MALAbility_BowShot.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/MALCharacterBase.h"
#include "Combat/MALProjectile.h"
#include "Combat/MALWeaponComponent.h"
#include "Combat/MALWeaponDefinition.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "Tags/MALGameplayTags.h"
#include "TimerManager.h"

UMALAbility_BowShot::UMALAbility_BowShot()
{
	const UMALGameplayTags* Tags = UMALGameplayTags::Get();

	// Status.Aiming is the tag this ability owns while it runs: the stance the character enters
	// and the tag animation/UI read share one lifetime, because both are this ability.
	ConfigureAbility(Tags->Ability_BowShot, Tags->Input_LMB, Tags->Status_Aiming);

	// The release has to reach the authority, or only the listen-server player could ever fire.
	bReplicateInputDirectly = true;

	HitImpactCueTag = Tags->GameplayCue_Hit_Impact;
	ProjectileClass = AMALProjectile::StaticClass();

	// A tap still shoots, it just hurts less than a full draw. Three tiers over 0.7s reads as
	// "quick shot / drawn / held", which is the whole input vocabulary of a one-button bow.
	FMALBowChargeTier& Quick = ChargeTiers.AddDefaulted_GetRef();
	Quick.ChargeTime = 0.0f;
	Quick.DamageScale = 0.6f;
	Quick.Speed = 4500.0f;

	FMALBowChargeTier& Drawn = ChargeTiers.AddDefaulted_GetRef();
	Drawn.ChargeTime = 0.35f;
	Drawn.DamageScale = 1.0f;
	Drawn.Speed = 6000.0f;

	FMALBowChargeTier& Full = ChargeTiers.AddDefaulted_GetRef();
	Full.ChargeTime = 0.7f;
	Full.DamageScale = 1.5f;
	Full.Speed = 7500.0f;
}

void UMALAbility_BowShot::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// Instanced per actor: a flag left over from the previous shot would swallow this one.
	bFired = false;

	const UWorld* World = GetWorld();
	ChargeStartTime = World ? World->GetTimeSeconds() : 0.0f;

	if (!CommitAbilityOrEnd(Handle, ActorInfo, ActivationInfo))
	{
		return;
	}

	if (!GetProjectileClass())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("%s: the equipped weapon has no ProjectileClass and the ability has no fallback, so there is nothing to fire."),
			*GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// The stance comes up with the draw. Entering it before the montage means the first frame of
	// the draw is already strafing, instead of the character turning away and snapping back.
	if (AMALCharacterBase* Character = GetMALCharacter())
	{
		Character->SetAiming(true);
	}

	// A draw with no animation still fires: the montage is presentation, the shot is the ability.
	FName StartSection = NAME_None;
	if (const UAnimMontage* Montage = GetAbilityMontage())
	{
		if (!DrawSection.IsNone() && Montage->IsValidSectionName(DrawSection))
		{
			StartSection = DrawSection;
		}
	}

	MontageTask = PlayAbilityMontage(StartSection);
}

void UMALAbility_BowShot::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RecoverTimer);
		World->GetTimerManager().ClearTimer(PostFireEndTimer);
	}
	RecoverTimer.Invalidate();
	PostFireEndTimer.Invalidate();

	// Dropping the stance here rather than in the fire path covers every way out: a completed
	// draw, a cancellation from death or a weapon swap, and a montage interrupted mid-draw.
	if (AMALCharacterBase* Character = GetMALCharacter())
	{
		Character->SetAiming(false);
	}

	MontageTask = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UMALAbility_BowShot::OnInputReleased(const FInputActionValue& Value)
{
	// The release is the shot. Nothing happens if the ability already ended, which is what lets
	// death or a swap cut a draw without firing an arrow on the way out.
	if (IsActive())
	{
		FireShot();
	}
}

void UMALAbility_BowShot::FireShot()
{
	if (!IsActive() || bFired)
	{
		return;
	}

	bFired = true;

	const UWorld* World = GetWorld();
	const float HeldSeconds = World ? World->GetTimeSeconds() - ChargeStartTime : 0.0f;
	const FMALBowChargeTier Tier = SelectTier(HeldSeconds);

	// Server-authoritative, like every other hit in this project: the client sees the arrow and
	// the cue, but only the server spends a round and resolves damage.
	if (AActor* Avatar = GetAvatarActorFromActorInfo())
	{
		if (Avatar->HasAuthority())
		{
			SpawnArrow(Tier);
		}
	}

	AdvanceMontageToFire();

	// The recovery is owned here rather than by the fire section: the animation is free to be as
	// long as it reads well, and the next shot arrives after a delay that was tuned on its own.
	SchedulePostFireEnd();
}

void UMALAbility_BowShot::OnAbilityMontageEnded(bool bInterrupted)
{
	// Before the release this is the draw running out on its own; after it, the fire section
	// finishing is the natural end of the shot either way.
	const bool bWasCancelled = bInterrupted || !bFired;

	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(),
		true, bWasCancelled);
}

AMALCharacterBase* UMALAbility_BowShot::GetMALCharacter() const
{
	return Cast<AMALCharacterBase>(GetAvatarActorFromActorInfo());
}

UMALWeaponComponent* UMALAbility_BowShot::GetWeaponComponent() const
{
	const AMALCharacterBase* Character = GetMALCharacter();
	return Character ? Character->GetWeaponComponent() : nullptr;
}

const UMALWeaponDefinition* UMALAbility_BowShot::GetWeaponDefinition() const
{
	const UMALWeaponComponent* WeaponComponent = GetWeaponComponent();
	return WeaponComponent ? WeaponComponent->GetEquippedWeapon() : nullptr;
}

TSubclassOf<AMALProjectile> UMALAbility_BowShot::GetProjectileClass() const
{
	// The weapon wins, so one shot ability can serve a bow, a crossbow and a thrown knife, each
	// naming its own projectile. The ability's own class is the fallback for an unauthored weapon.
	const UMALWeaponDefinition* Definition = GetWeaponDefinition();
	if (Definition && Definition->ProjectileClass)
	{
		return Definition->ProjectileClass;
	}

	return ProjectileClass;
}

FMALBowChargeTier UMALAbility_BowShot::SelectTier(float HeldSeconds) const
{
	// Read in order and keep the last one reached: tiers are authored ascending, and a list that
	// happens to be out of order still resolves to something predictable instead of failing.
	if (ChargeTiers.Num() == 0)
	{
		return FMALBowChargeTier();
	}

	int32 SelectedIndex = 0;
	for (int32 Index = 0; Index < ChargeTiers.Num(); ++Index)
	{
		if (HeldSeconds >= ChargeTiers[Index].ChargeTime)
		{
			SelectedIndex = Index;
		}
	}

	return ChargeTiers[SelectedIndex];
}

bool UMALAbility_BowShot::SpawnArrow(const FMALBowChargeTier& Tier)
{
	AMALCharacterBase* Character = GetMALCharacter();
	UMALWeaponComponent* WeaponComponent = GetWeaponComponent();
	UWorld* World = GetWorld();
	const TSubclassOf<AMALProjectile> Class = GetProjectileClass();

	if (!Character || !WeaponComponent || !World || !Class)
	{
		return false;
	}

	// Rounds are spent before the arrow exists, so a refused shot leaves nothing in the level.
	// An unlimited weapon always passes this check.
	if (!WeaponComponent->TryConsumeAmmo(1))
	{
		return false;
	}

	// Aim from the camera and fire from the muzzle: the crosshair decides where the shot goes,
	// and the arrow still leaves the bow rather than the character's eyes.
	FVector ViewLocation = Character->GetActorLocation();
	FRotator ViewRotation = Character->GetActorRotation();

	if (const AController* Controller = Character->GetController())
	{
		Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	}

	const FVector ViewDirection = ViewRotation.Vector();
	const FVector TraceEnd = ViewLocation + ViewDirection * AimTraceDistance;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(MALBowAim), false, Character);
	FHitResult AimHit;
	const FVector AimPoint = World->LineTraceSingleByChannel(AimHit, ViewLocation, TraceEnd, ECC_Visibility, Params)
		? AimHit.ImpactPoint
		: TraceEnd;

	const FTransform Muzzle = Character->GetWeaponMuzzleTransform();

	FVector Direction = (AimPoint - Muzzle.GetLocation()).GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		Direction = ViewDirection;
	}

	const FTransform SpawnTransform(Direction.Rotation(), Muzzle.GetLocation());

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Character;
	SpawnParams.Instigator = Character;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AMALProjectile* Arrow = World->SpawnActor<AMALProjectile>(Class, SpawnTransform, SpawnParams);
	if (!Arrow)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: could not spawn %s for the shot."), *GetName(), *GetNameSafe(Class));
		return false;
	}

	Arrow->InitializeProjectile(BuildHitRequest(Tier), Tier.Penetrations);

	// The arrow art belongs to the weapon definition, so two bows can share one projectile class
	// and still look different.
	if (const UMALWeaponDefinition* Definition = GetWeaponDefinition())
	{
		Arrow->SetArrowVisual(Definition->ProjectileMesh, Definition->ProjectileMeshOffset);
	}

	Arrow->SetLaunchSpeed(Tier.Speed);

	// One line per shot: which tier the hold selected, where the crosshair landed and whether the
	// hit request came out with damage. Enough to tell a refused shot from one that missed.
	UE_LOG(LogTemp, Log, TEXT("%s: %s fired %s (charge %.0fms, aim %s, speed %.0f)."),
		*Character->GetName(), *GetName(), *GetNameSafe(Arrow),
		Tier.ChargeTime * 1000.0f, *AimPoint.ToCompactString(), Tier.Speed);

	return true;
}

FMALWeaponHitRequest UMALAbility_BowShot::BuildHitRequest(const FMALBowChargeTier& Tier)
{
	FMALWeaponHitRequest Request;
	Request.Instigator = GetAvatarActorFromActorInfo();
	Request.SourceObject = this;
	Request.DamageGEClass = DamageGEClass;
	Request.BaseDamage = BaseDamage * Tier.DamageScale;

	// The weapon scales the shot the same way it scales a swing: one ability asset, many bows.
	if (const UMALWeaponComponent* WeaponComponent = GetWeaponComponent())
	{
		Request.BaseDamage *= WeaponComponent->GetDamageMultiplier();
	}

	// A charged shot has no chain, so no per-step bonus is possible or wanted.
	Request.ComboDamageBonusPerStep = 0.0f;
	Request.AbilityLevel = GetAbilityLevel();
	Request.LevelScale = GetLevelScaledValue(TEXT("Damage"), 1.0f);
	Request.ImpactCueTag = HitImpactCueTag;
	Request.HitStopDuration = HitStopDuration;
	Request.CameraShakeClass = CameraShakeClass;
	Request.bKnockback = false;

	return Request;
}

void UMALAbility_BowShot::AdvanceMontageToFire()
{
	UAnimMontage* Montage = GetAbilityMontage();

	if (!Montage || !MontageTask || FireSection.IsNone() || !Montage->IsValidSectionName(FireSection))
	{
		if (PostFireEndDelay < 0.0f)
		{
			// Negative delay: the shot ends on the animation's clock, and with a usable fire
			// section the tail is what drives it. Without one, RecoverTime is the only clock
			// left, so it stands in for the tail rather than competing with it.
			ScheduleRecoverEnd();
		}

		// A zero or positive delay ends the shot on the post-fire timer scheduled by FireShot.
		// Scheduling RecoverTime here as well would end it twice, and the earlier of the two
		// would cut the other short.
		return;
	}

	// Jumped inside the running montage instead of playing a second one: two montages on the
	// same slot would interrupt each other and the draw would never blend into the release.
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	USkeletalMeshComponent* MeshComponent = Character ? Character->GetMesh() : nullptr;
	UAnimInstance* AnimInstance = MeshComponent ? MeshComponent->GetAnimInstance() : nullptr;

	if (AnimInstance)
	{
		AnimInstance->Montage_JumpToSection(FireSection, Montage);

		// The tail the jump started would otherwise decide the end of the ability, which is the
		// coupling PostFireEndDelay exists to break: the shot now ends on its own clock, and the
		// montage is stopped there by bStopWhenAbilityEnds.
		if (PostFireEndDelay >= 0.0f)
		{
			MontageTask->OnCompleted.Clear();
			MontageTask->OnBlendOut.Clear();
		}

		return;
	}

	// The jump could not run, so there is no tail to overrule: RecoverTime ends this shot.
	// The post-fire timer is not scheduled in that case, so nothing ends the shot twice.
	if (PostFireEndDelay < 0.0f)
	{
		ScheduleRecoverEnd();
	}
}

void UMALAbility_BowShot::SchedulePostFireEnd()
{
	// Negative means "let the fire animation end the shot".
	if (PostFireEndDelay < 0.0f)
	{
		return;
	}

	if (PostFireEndDelay == 0.0f)
	{
		HandlePostFireEnd();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		// Bound with CreateUObject rather than a lambda: the timer outlives nothing, but the
		// weak-object binding also drops the callback if the ability instance is gone.
		World->GetTimerManager().SetTimer(PostFireEndTimer, this,
			&UMALAbility_BowShot::HandlePostFireEnd, PostFireEndDelay, false);
	}
}

void UMALAbility_BowShot::HandlePostFireEnd()
{
	PostFireEndTimer.Invalidate();

	if (!IsActive())
	{
		return;
	}

	// Not flagged as cancelled: the shot did fire, and a cancel would read as an interruption
	// in the hit feedback and in anything gating on the ability's end state.
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UMALAbility_BowShot::ScheduleRecoverEnd()
{
	const FGameplayAbilitySpecHandle Handle = GetCurrentAbilitySpecHandle();
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	const FGameplayAbilityActivationInfo ActivationInfo = GetCurrentActivationInfo();

	if (RecoverTime <= 0.0f)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	World->GetTimerManager().SetTimer(RecoverTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (IsActive())
		{
			EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
		}
	}), RecoverTime, false);
}
