// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameAbilities/MALAbility_MeleeAttack.h"

#include "Abilities/Tasks/AbilityTask_ApplyRootMotionMoveToForce.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Character/MALCharacterBase.h"
#include "Combat/MALDamageExecutor.h"
#include "Combat/MALWeaponComponent.h"
#include "Engine/World.h"
#include "GameAbilities/MALHitCameraShake.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/RootMotionSource.h"
#include "GameplayTagContainer.h"
#include "Tags/MALGameplayTags.h"
#include "TimerManager.h"

UMALAbility_MeleeAttack::UMALAbility_MeleeAttack()
{
	const UMALGameplayTags* Tags = UMALGameplayTags::Get();

	// Default hit feedback; designers can override the camera shake class in BP. Kept here
	// rather than on each subclass so a new swing ability gets the same feel for free.
	HitCameraShakeClass = UMALHitCameraShake::StaticClass();

	// The impact cue is resolved by tag, so the sound itself lives entirely in a
	// GameplayCueNotify_Burst Blueprint rather than in this ability.
	HitImpactCueTag = Tags->GameplayCue_Hit_Impact;

	// Melee abilities chain on further input presses, and those have to reach the server so both
	// sides agree on the step. Every melee ability used to inherit this from the combo class, so
	// it belongs on the shared base instead of being re-declared per subclass.
	bReplicateInputDirectly = true;
}

void UMALAbility_MeleeAttack::PostLoad()
{
	Super::PostLoad();

	// AbilityMontage replaced AttackMontage when the montage moved to UMALGameplayAbility. Assets
	// saved before that still carry the old property; copy it once so existing blueprints keep
	// the montage they were configured with.
	if (!AttackMontage)
	{
		return;
	}

	if (!AbilityMontage)
	{
		AbilityMontage = AttackMontage;
	}
	else if (AbilityMontage != AttackMontage)
	{
		// Both set: the new property is the one that is still visible in the details panel, so
		// it wins. Logged because the old reference is about to be dropped for good.
		UE_LOG(LogTemp, Warning,
			TEXT("%s: both AbilityMontage (%s) and deprecated AttackMontage (%s) are set; keeping AbilityMontage"),
			*GetName(), *GetNameSafe(AbilityMontage), *GetNameSafe(AttackMontage));
	}

	AttackMontage = nullptr;
}

void UMALAbility_MeleeAttack::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// Abilities are instanced and reused, so a flag left over from the previous swing would
	// hide a montage that never opens a window.
	bHitWindowOpened = false;

	if (!CommitAbilityOrEnd(Handle, ActorInfo, ActivationInfo))
	{
		return;
	}

	if (!AbilityMontage)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: no AbilityMontage set, this swing cannot open a hit window"), *GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Route confirmed hits from the character's weapon component into this ability. The
	// component only ever broadcasts on the authority, so binding on both sides is harmless.
	if (UMALWeaponComponent* WeaponComponent = GetWeaponComponent())
	{
		// Abilities are instanced and reused; a stale binding from a previous activation
		// would resolve hits twice.
		WeaponComponent->OnWeaponHit.RemoveAll(this);
		WeaponComponent->OnWeaponHit.AddUObject(this, &UMALAbility_MeleeAttack::OnWeaponHit);
	}

	// The forward lunge is intentionally NOT applied here: it is triggered per-hit by the
	// montage's MAL Lunge notify -> TriggerLunge().
	if (!PlayAbilityMontage(GetMontageStartSection()))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
}

void UMALAbility_MeleeAttack::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// A montage without an opening notify plays a full swing that can never hit. Reporting it
	// here points straight at the asset instead of leaving it to guesswork during a fight.
	if (!bHitWindowOpened && !bWasCancelled)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("%s finished without opening a hit window: add a MAL Weapon Collision notify with bEnable=true to %s"),
			*GetName(), *GetNameSafe(AbilityMontage));
	}

	// The ability is ending on any path (montage finished, cancelled, released below), so a
	// pending release has nothing left to do.
	CancelPostHitRelease();

	// Stop any in-flight lunge explicitly; the task is also destroyed by EndAbility, but
	// ending it here guarantees the root motion source is removed before the movement mode
	// is restored.
	if (IsValid(ActiveLungeTask))
	{
		ActiveLungeTask->EndTask();
	}
	ActiveLungeTask = nullptr;

	// Clean up weapon collision in case the ability ends before the disable notify fires.
	// Not routed through DisableWeaponCollision on purpose: a subclass override may arm a
	// recovery timer, and that would re-enter EndAbility before Super finishes here.
	if (UMALWeaponComponent* WeaponComponent = GetWeaponComponent())
	{
		WeaponComponent->EndHitWindow();
		WeaponComponent->OnWeaponHit.RemoveAll(this);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UMALAbility_MeleeAttack::TriggerLunge()
{
	ApplyLunge();
}

void UMALAbility_MeleeAttack::EnableWeaponCollision()
{
	bHitWindowOpened = true;

	// A new window means this activation is still swinging: a release armed by a previous
	// window would end the ability in the middle of the montage.
	CancelPostHitRelease();

	// The component owns the collision state and the per-swing hit list; the ability only says
	// when a swing starts. StartHitWindow is a no-op on clients.
	if (UMALWeaponComponent* WeaponComponent = GetWeaponComponent())
	{
		WeaponComponent->StartHitWindow(GetHitComboStep(), WeaponCollisionSocketName);
	}
}

void UMALAbility_MeleeAttack::DisableWeaponCollision()
{
	// Close the window first: the release below must not fire while hits are still resolving.
	if (UMALWeaponComponent* WeaponComponent = GetWeaponComponent())
	{
		WeaponComponent->EndHitWindow();
	}

	// Everything left in the montage is recovery. Releasing here keeps the character free
	// while the tail blends out, instead of locking it for animation nobody watches.
	SchedulePostHitRelease();
}

void UMALAbility_MeleeAttack::SchedulePostHitRelease()
{
	// Negative means "play the montage out"; subclasses that own their recovery opt out here.
	if (PostHitReleaseDelay < 0.f)
	{
		return;
	}

	if (PostHitReleaseDelay == 0.f)
	{
		HandlePostHitRelease();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			PostHitReleaseTimer, this, &UMALAbility_MeleeAttack::HandlePostHitRelease, PostHitReleaseDelay, false);
	}
}

void UMALAbility_MeleeAttack::CancelPostHitRelease()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PostHitReleaseTimer);
	}

	PostHitReleaseTimer.Invalidate();
}

void UMALAbility_MeleeAttack::HandlePostHitRelease()
{
	PostHitReleaseTimer.Invalidate();

	if (!IsActive())
	{
		return;
	}

	// EndAbility stops the montage through bStopWhenAbilityEnds, so the tail fades out on the
	// montage's own blend-out time instead of snapping. Status.Attacking drops with the
	// ability, which is exactly what AMALPlayerController gates Move and Jump on.
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

UMALWeaponComponent* UMALAbility_MeleeAttack::GetWeaponComponent() const
{
	if (const AMALCharacterBase* Character = Cast<AMALCharacterBase>(GetAvatarActorFromActorInfo()))
	{
		return Character->GetWeaponComponent();
	}

	return nullptr;
}

void UMALAbility_MeleeAttack::OnWeaponHit(const FMALHitResult& HitResult)
{
	FMALWeaponHitRequest Request;
	BuildHitRequest(Request, HitResult);

	// The executor owns damage, feedback and the WeaponHit event; this ability only supplies
	// its tuning. Server-only, like the weapon component that called us.
	UMALDamageExecutor::ExecuteWeaponHit(Request);
}

void UMALAbility_MeleeAttack::BuildHitRequest(FMALWeaponHitRequest& OutRequest, const FMALHitResult& HitResult)
{
	OutRequest.Instigator = GetAvatarActorFromActorInfo();
	OutRequest.Hit = HitResult;
	OutRequest.SourceObject = this;
	OutRequest.DamageGEClass = DamageGEClass;
	OutRequest.BaseDamage = BaseDamage;
	OutRequest.ComboDamageBonusPerStep = ComboDamageBonusPerStep;
	OutRequest.AbilityLevel = GetAbilityLevel();
	OutRequest.LevelScale = GetLevelScaledValue(TEXT("Damage"), 1.0f);
	OutRequest.ImpactCueTag = HitImpactCueTag;
	OutRequest.HitStopDuration = HitStopDuration;
	OutRequest.CameraShakeClass = HitCameraShakeClass;

	OutRequest.bKnockback = bCanKnockback;
	OutRequest.KnockbackDistance = KnockbackDistance;
	OutRequest.KnockbackDuration = KnockbackDuration;
}

void UMALAbility_MeleeAttack::ApplyLunge()
{
	// The lunge notify resolves this ability through the montage it animates, and ending the
	// ability stops that montage. Guard anyway: a root motion task created on a finished ability
	// would move the character with nobody left to end it.
	if (!IsActive())
	{
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (!ActorInfo)
	{
		return;
	}

	ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
	if (!Character || LungeDistance <= 0.f || LungeDuration <= 0.f)
	{
		return;
	}

	// One active lunge at a time: stacking root motion sources produces erratic movement
	// and makes cleanup ambiguous.
	if (IsValid(ActiveLungeTask))
	{
		ActiveLungeTask->EndTask();
		ActiveLungeTask = nullptr;
	}

	// Lunge toward the CONTROL direction (camera/aim yaw), flattened to the ground,
	// instead of the actor's facing.
	const AController* Controller = Character->GetController();
	const FRotator ControlRotation = Controller ? Controller->GetControlRotation() : Character->GetActorRotation();
	const FVector LungeDir = FRotationMatrix(FRotator(0.f, ControlRotation.Yaw, 0.f)).GetUnitAxis(EAxis::X);
	const FVector TargetLocation = Character->GetActorLocation() + LungeDir * LungeDistance;

	// Snap the character's facing to the lunge direction so the body doesn't slide
	// sideways during the dash. Guarded by Controller.
	if (Controller)
	{
		Character->SetActorRotation(FRotator(0.f, ControlRotation.Yaw, 0.f));
	}

	// Drive the lunge through GAS root motion instead of manual SetActorLocation.
	UAbilityTask_ApplyRootMotionMoveToForce* LungeTask =
		UAbilityTask_ApplyRootMotionMoveToForce::ApplyRootMotionMoveToForce(
			this,
			NAME_None,                                  // TaskInstanceName
			TargetLocation,
			LungeDuration,
			false,                                      // bSetNewMovementMode: keep current mode so gravity/ground following stay intact
			MOVE_Walking,                               // unused when bSetNewMovementMode is false
			true,                                       // bRestrictSpeedToExpected: avoid velocity spikes when blocked by collision
			nullptr,                                    // PathOffsetCurve
			ERootMotionFinishVelocityMode::SetVelocity, // stop cleanly at the end (no post-lunge slide)
			FVector::ZeroVector,                        // SetVelocityOnFinish
			0.f);                                       // ClampVelocityOnFinish (unused)

	if (LungeTask)
	{
		LungeTask->OnTimedOut.AddDynamic(this, &UMALAbility_MeleeAttack::OnLungeFinished);
		LungeTask->OnTimedOutAndDestinationReached.AddDynamic(this, &UMALAbility_MeleeAttack::OnLungeFinished);
		LungeTask->ReadyForActivation();
		ActiveLungeTask = LungeTask;
	}
}

void UMALAbility_MeleeAttack::OnLungeFinished()
{
	// The root motion source is removed and the movement mode restored automatically when
	// the task ends.
	ActiveLungeTask = nullptr;
	UE_LOG(LogTemp, Verbose, TEXT("%s: lunge finished"), *GetName());
}
