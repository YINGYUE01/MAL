// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameAbilities/MALAbility_RangedAttack.h"

#include "Animation/AnimMontage.h"
#include "AI/MALEnemyAIController.h"
#include "Character/MALCharacterBase.h"
#include "Combat/MALProjectile.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Tags/MALGameplayTags.h"
#include "TimerManager.h"

UMALAbility_RangedAttack::UMALAbility_RangedAttack()
{
	const UMALGameplayTags* Tags = UMALGameplayTags::Get();

	// The generic attack tag is what the behaviour tree activates, so a ranged enemy needs no
	// branch of its own. Owning Status.Attacking is what makes the tree wait for the shot to
	// finish instead of re-activating mid-cast.
	ConfigureAbility(Tags->Ability_Attack, Tags->Input_LMB, Tags->Status_Attacking);

	ImpactCueTag = Tags->GameplayCue_Hit_Impact;
}

void UMALAbility_RangedAttack::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// Abilities are instanced and reused: a stale flag would swallow the next shot.
	bFired = false;

	if (!CommitAbilityOrEnd(Handle, ActorInfo, ActivationInfo))
	{
		return;
	}

	if (!ProjectileClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: no ProjectileClass set, the shot cannot fire"), *GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// An authored cast animation carries the wind-up and its release frame; without one there
	// is nothing to report the fire notify, so the shot goes out on activation instead.
	if (AbilityMontage)
	{
		if (!PlayAbilityMontage(GetMontageStartSection()))
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		}

		return;
	}

	FireProjectile();

	World->GetTimerManager().SetTimer(RecoveryTimer,
		FTimerDelegate::CreateWeakLambda(this, [this]() { K2_EndAbility(); }),
		FMath::Max(RecoveryTime, 0.01f), false);
}

void UMALAbility_RangedAttack::NotifyFireProjectile()
{
	// The notify also runs on clients and on a reused instance; FireProjectile itself checks the
	// authority and the per-activation fired flag.
	if (!IsActive())
	{
		return;
	}

	FireProjectile();
}

void UMALAbility_RangedAttack::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// The recovery timer is ability-scoped: a cancelled cast must not end a later activation.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RecoveryTimer);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

FMALWeaponHitRequest UMALAbility_RangedAttack::BuildDamageRequest() const
{
	FMALWeaponHitRequest Request;
	Request.Instigator = GetAvatarActorFromActorInfo();
	Request.SourceObject = const_cast<UMALAbility_RangedAttack*>(this);
	Request.DamageGEClass = DamageGEClass;
	Request.BaseDamage = BaseDamage;
	Request.AbilityLevel = GetAbilityLevel();
	Request.LevelScale = GetLevelScaledValue(TEXT("Damage"), 1.0f);
	Request.ImpactCueTag = ImpactCueTag;
	Request.HitStopDuration = HitStopDuration;

	return Request;
}

AActor* UMALAbility_RangedAttack::ResolveAimTarget(AActor* Avatar) const
{
	const APawn* AvatarPawn = Cast<APawn>(Avatar);
	if (!AvatarPawn)
	{
		return nullptr;
	}

	// The enemy brain keeps its current target on the blackboard, which is the body the shot is
	// supposed to hit. Null outside combat, where the fallback below keeps the shot sane.
	if (const AMALEnemyAIController* EnemyController = Cast<AMALEnemyAIController>(AvatarPawn->GetController()))
	{
		return EnemyController->GetCombatTarget();
	}

	return nullptr;
}

FVector UMALAbility_RangedAttack::ResolveAimDirection(const FVector& MuzzleLocation, AActor* Avatar) const
{
	AActor* Target = ResolveAimTarget(Avatar);
	if (!Target)
	{
		if (const APawn* AvatarPawn = Cast<APawn>(Avatar))
		{
			return AvatarPawn->GetBaseAimRotation().Vector();
		}

		return Avatar ? Avatar->GetActorForwardVector() : FVector::ForwardVector;
	}

	// Chest height instead of the origin at the feet, so a straight shot crosses the body.
	FVector AimPoint = Target->GetActorLocation() + FVector(0.0f, 0.0f, TargetAimHeightOffset);

	if (bLeadTarget && ProjectileSpeed > KINDA_SMALL_NUMBER)
	{
		const float TravelTime = FVector::Dist(MuzzleLocation, AimPoint) / ProjectileSpeed;
		AimPoint += Target->GetVelocity() * TravelTime;
	}

	return (AimPoint - MuzzleLocation).GetSafeNormal();
}

void UMALAbility_RangedAttack::FireProjectile()
{
	if (bFired || !IsActive())
	{
		return;
	}

	AActor* Avatar = GetAvatarActorFromActorInfo();
	UWorld* World = GetWorld();
	if (!Avatar || !World || !Avatar->HasAuthority())
	{
		return;
	}

	bFired = true;

	// Spawn where the weapon points; a character without a weapon falls back to its own body,
	// which keeps a bare ranged enemy firing from its chest instead of from the world origin.
	FTransform MuzzleTransform = Avatar->GetActorTransform();
	if (const AMALCharacterBase* Character = Cast<AMALCharacterBase>(Avatar))
	{
		MuzzleTransform = Character->GetWeaponMuzzleTransform(MuzzleSocketName);
	}

	// Aim from the muzzle toward the target rather than along the pawn's base aim: the base aim
	// is the body yaw, and a shot fired along it passes over a target that is closer or lower.
	const AActor* AimTarget = ResolveAimTarget(Avatar);
	const FVector AimDirection = ResolveAimDirection(MuzzleTransform.GetLocation(), Avatar);

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Avatar;
	SpawnParams.Instigator = Cast<APawn>(Avatar);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AMALProjectile* Projectile = World->SpawnActor<AMALProjectile>(
		ProjectileClass, MuzzleTransform.GetLocation(), AimDirection.Rotation(), SpawnParams);

	if (!Projectile)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: failed to spawn %s"), *GetName(), *GetNameSafe(ProjectileClass));
		return;
	}

	Projectile->InitializeProjectile(BuildDamageRequest(), Penetrations);

	// The velocity is set from the aim direction, not from the actor's forward axis: a spawn
	// rotation that did not stick would otherwise leave every arrow flying the same way.
	Projectile->LaunchTowardDirection(AimDirection, ProjectileSpeed);

	UE_LOG(LogTemp, Log, TEXT("%s: fired %s at %s from %s (socket %s) toward %s"),
		*GetName(), *Projectile->GetName(), *GetNameSafe(AimTarget),
		*MuzzleTransform.GetLocation().ToCompactString(), *MuzzleSocketName.ToString(),
		*AimDirection.ToCompactString());
}
