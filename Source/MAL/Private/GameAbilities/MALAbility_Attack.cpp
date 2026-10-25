// Copyright Epic Games, Inc. All Rights Reserved.


#include "GameAbilities/MALAbility_Attack.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionMoveToForce.h"
#include "Character/MALCharacterBase.h"
#include "Combat/MALComboComponent.h"
#include "Combat/MALDamageExecutor.h"
#include "Combat/MALWeaponComponent.h"
#include "GameAbilities/MALHitCameraShake.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"
#include "Tags/MALGameplayTags.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"

UMALAbility_Attack::UMALAbility_Attack()
{
	const UMALGameplayTags* Tags = UMALGameplayTags::Get();

	// Set ability tags
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(Tags->Ability_Attack);
	SetAssetTags(AssetTags);
	
	// Block other attacks while attacking (mutual exclusion)
	ActivationBlockedTags.AddTag(Tags->Status_Attacking);
	
	// Tags applied during activation
	ActivationOwnedTags.AddTag(Tags->Status_Attacking);

	// Combo presses must reach the server so the step stays in sync; the ASC mirrors
	// the engine's ServerSetInputPressed/Released when this flag is set.
	bReplicateInputDirectly = true;

	// Default hit feedback; designers can override the camera shake class in BP.
	HitCameraShakeClass = UMALHitCameraShake::StaticClass();

	// The impact cue is resolved by tag, so the sound itself lives entirely in a
	// GameplayCueNotify_Burst Blueprint rather than in this ability.
	HitImpactCueTag = Tags->GameplayCue_Hit_Impact;

	// Default 4-step combo section names (override in Blueprint to match the montage).
	ComboSections = { FName("Combo1"), FName("Combo2"), FName("Combo3"), FName("Combo4") };
}

void UMALAbility_Attack::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// Abilities are instanced and reused, so a flag left over from the previous swing would hide
	// a montage that never opens a window.
	bHitWindowOpened = false;

	// Costs/cooldowns must be committed before the ability starts doing work.
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!AttackMontage)
	{
		UE_LOG(LogTemp, Warning, TEXT("UMALAbility_Attack::ActivateAbility - No AttackMontage assigned!"));
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
		WeaponComponent->OnWeaponHit.AddUObject(this, &UMALAbility_Attack::OnWeaponHit);
	}

	// Fresh combo each activation; the state machine lives on the character, not the ability.
	if (UMALComboComponent* ComboComponent = GetComboComponent())
	{
		ComboComponent->OnComboAdvanced.RemoveAll(this);
		ComboComponent->OnComboAdvanced.AddUObject(this, &UMALAbility_Attack::OnComboAdvanced);
		ComboComponent->OnRecoveryExpired.RemoveAll(this);
		ComboComponent->OnRecoveryExpired.AddUObject(this, &UMALAbility_Attack::ReleaseRecovery);

		// The montage section is the only value both sides agree on, so let the state machine
		// chain from it instead of from its own cached step.
		ComboComponent->MontageStepQuery.BindUObject(this, &UMALAbility_Attack::GetCurrentComboStepFromMontage);
		ComboComponent->BeginCombo(ComboSections, ComboInputBufferWindow, RecoveryReleaseDelay);
	}

	// Enter at the first combo section (falls back to montage start if none configured).
	const FName StartSection = ComboSections.IsValidIndex(0) ? ComboSections[0] : NAME_None;

	// Play the montage on its DefaultSlot (full body). Slot routing lives in the montage
	// asset; the engine task handles replication, blend-out and cancellation correctly.
	UAbilityTask_PlayMontageAndWait* PlayTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		AttackMontage,
		1.0f,                  // Rate
		StartSection,          // StartSection
		true,                  // bStopWhenAbilityEnds
		1.0f,                  // AnimRootMotionTranslationScale
		0.0f,                  // StartTimeSeconds
		false);                // bAllowInterruptAfterBlendOut

	if (!PlayTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	PlayTask->OnCompleted.AddDynamic(this, &UMALAbility_Attack::OnMontageCompleted);
	PlayTask->OnBlendOut.AddDynamic(this, &UMALAbility_Attack::OnMontageBlendOut);
	PlayTask->OnInterrupted.AddDynamic(this, &UMALAbility_Attack::OnMontageInterrupted);
	PlayTask->OnCancelled.AddDynamic(this, &UMALAbility_Attack::OnMontageCancelled);
	PlayTask->ReadyForActivation();

	// NOTE: the forward lunge is intentionally NOT applied here. It is triggered per-hit by
	// the montage's AN_Lunge notify -> TriggerLunge() -> ApplyLunge().
}

void UMALAbility_Attack::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	UE_LOG(LogTemp, Log, TEXT("UMALAbility_Attack::EndAbility - Attack ended. Cancelled: %s"), bWasCancelled ? TEXT("true") : TEXT("false"));

	// A montage without an opening notify plays a full swing that can never hit. Reporting it
	// here points straight at the asset instead of leaving it to guesswork during a fight.
	if (!bHitWindowOpened && !bWasCancelled)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UMALAbility_Attack::EndAbility - %s finished without opening a hit window: add a MAL Weapon Collision notify with bEnable=true to %s"),
			*GetName(), *GetNameSafe(AttackMontage));
	}

	// Stop any in-flight lunge explicitly; the task is also destroyed by EndAbility, but
	// ending it here guarantees the root motion source is removed before the movement mode
	// is restored.
	if (IsValid(ActiveLungeTask))
	{
		ActiveLungeTask->EndTask();
	}
	ActiveLungeTask = nullptr;

	// Clean up weapon collision in case the ability ends before the disable notify fires.
	// Not routed through DisableWeaponCollision on purpose: that also arms the recovery timer,
	// and a zero RecoveryReleaseDelay would re-enter EndAbility before Super finishes here.
	if (UMALWeaponComponent* WeaponComponent = GetWeaponComponent())
	{
		WeaponComponent->EndHitWindow();
		WeaponComponent->OnWeaponHit.RemoveAll(this);
	}

	if (UMALComboComponent* ComboComponent = GetComboComponent())
	{
		ComboComponent->OnComboAdvanced.RemoveAll(this);
		ComboComponent->OnRecoveryExpired.RemoveAll(this);
		ComboComponent->MontageStepQuery.Unbind();
		ComboComponent->EndCombo();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UMALAbility_Attack::OnMontageCompleted()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UMALAbility_Attack::OnMontageBlendOut()
{
	// Montage is blending out; the ability ends on OnMontageCompleted so the last section's
	// recovery keeps playing. Use bAllowInterruptAfterBlendOut if earlier release is needed.
}

void UMALAbility_Attack::OnMontageInterrupted()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
}

void UMALAbility_Attack::OnMontageCancelled()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
}

void UMALAbility_Attack::ApplyLunge()
{
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
		LungeTask->OnTimedOut.AddDynamic(this, &UMALAbility_Attack::OnLungeFinished);
		LungeTask->OnTimedOutAndDestinationReached.AddDynamic(this, &UMALAbility_Attack::OnLungeFinished);
		LungeTask->ReadyForActivation();
		ActiveLungeTask = LungeTask;
	}
}

void UMALAbility_Attack::OnLungeFinished()
{
	// The root motion source is removed and the movement mode restored automatically when
	// the task ends.
	ActiveLungeTask = nullptr;
	UE_LOG(LogTemp, Verbose, TEXT("UMALAbility_Attack: lunge finished"));
}

void UMALAbility_Attack::OnInputPressed(const FInputActionValue& Value)
{
	// Let the base forward/broadcast first, then hand the press to the combo state machine.
	Super::OnInputPressed(Value);

	if (UMALComboComponent* ComboComponent = GetComboComponent())
	{
		ComboComponent->PressAttack();
	}
}

UMALComboComponent* UMALAbility_Attack::GetComboComponent() const
{
	if (const AMALCharacterBase* Character = Cast<AMALCharacterBase>(GetAvatarActorFromActorInfo()))
	{
		return Character->GetComboComponent();
	}

	return nullptr;
}

void UMALAbility_Attack::OnComboAdvanced(int32 Step, FName Section)
{
	// Use the ability API (ASC::CurrentMontageJumpToSection under the hood): it sends a
	// reliable server RPC from clients and replicates the section to other clients.
	MontageJumpToSection(Section);

	UE_LOG(LogTemp, Log, TEXT("UMALAbility_Attack: advanced to combo step %d (section %s)"),
		Step, *Section.ToString());
}

int32 UMALAbility_Attack::GetCurrentComboStepFromMontage() const
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		const FName Section = ASC->GetCurrentMontageSectionName();
		const int32 Index = ComboSections.IndexOfByKey(Section);
		if (Index != INDEX_NONE)
		{
			return Index;
		}
	}

	// Montage is between sections (transition frame, or no montage yet): fall back to the
	// state machine's current step.
	if (const UMALComboComponent* ComboComponent = GetComboComponent())
	{
		return ComboComponent->GetStep();
	}

	return 0;
}

void UMALAbility_Attack::OpenComboWindow()
{
	if (UMALComboComponent* ComboComponent = GetComboComponent())
	{
		ComboComponent->OpenWindow();
	}
}

void UMALAbility_Attack::CloseComboWindow()
{
	if (UMALComboComponent* ComboComponent = GetComboComponent())
	{
		ComboComponent->CloseWindow();
	}
}

void UMALAbility_Attack::TriggerLunge()
{
	ApplyLunge();
}

void UMALAbility_Attack::EnableWeaponCollision()
{
	bHitWindowOpened = true;

	// A new hit window means this section still owns the ability: a release left over from the
	// previous section would end it in the middle of the swing.
	if (UMALComboComponent* ComboComponent = GetComboComponent())
	{
		ComboComponent->CancelRecovery();
	}

	// The component owns the collision state and the per-swing hit list; the ability only says
	// when a swing starts. StartHitWindow is a no-op on clients.
	if (UMALWeaponComponent* WeaponComponent = GetWeaponComponent())
	{
		WeaponComponent->StartHitWindow(GetCurrentComboStepFromMontage(), WeaponCollisionSocketName);
	}
}

void UMALAbility_Attack::DisableWeaponCollision()
{
	if (UMALWeaponComponent* WeaponComponent = GetWeaponComponent())
	{
		WeaponComponent->EndHitWindow();
	}

	// The hit window is over; everything left in the montage is recovery. Do not keep the
	// character locked for animation nobody is going to watch.
	if (UMALComboComponent* ComboComponent = GetComboComponent())
	{
		ComboComponent->ScheduleRecovery();
	}
}

void UMALAbility_Attack::ReleaseRecovery()
{
	if (!IsActive())
	{
		return;
	}

	// EndAbility stops the montage through bStopWhenAbilityEnds, so the tail fades out on the
	// montage's own blend-out time instead of snapping. Status.Attacking drops with the ability,
	// which is exactly what AMALPlayerController gates Move and Jump on.
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

UMALWeaponComponent* UMALAbility_Attack::GetWeaponComponent() const
{
	if (const AMALCharacterBase* Character = Cast<AMALCharacterBase>(GetAvatarActorFromActorInfo()))
	{
		return Character->GetWeaponComponent();
	}

	return nullptr;
}

void UMALAbility_Attack::OnWeaponHit(const FMALHitResult& HitResult)
{
	FMALWeaponHitRequest Request;
	Request.Instigator = GetAvatarActorFromActorInfo();
	Request.Hit = HitResult;
	Request.SourceObject = this;
	Request.DamageGEClass = DamageGEClass;
	Request.BaseDamage = BaseDamage;
	Request.ComboDamageBonusPerStep = ComboDamageBonusPerStep;
	Request.AbilityLevel = GetAbilityLevel();
	Request.LevelScale = GetLevelScaledValue(TEXT("Damage"), 1.0f);
	Request.ImpactCueTag = HitImpactCueTag;
	Request.HitStopDuration = HitStopDuration;
	Request.CameraShakeClass = HitCameraShakeClass;

	// The executor owns damage, feedback and the WeaponHit event; this ability only supplies
	// its tuning. Server-only, like the weapon component that called us.
	UMALDamageExecutor::ExecuteWeaponHit(Request);
}
