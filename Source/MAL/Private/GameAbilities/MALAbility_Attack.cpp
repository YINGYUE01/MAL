// Copyright Epic Games, Inc. All Rights Reserved.


#include "GameAbilities/MALAbility_Attack.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionMoveToForce.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "GameAbilities/MALDamageGameplayEffect.h"
#include "GameAbilities/MALHitCameraShake.h"
#include "Player/MALPlayerController.h"
#include "AbilitySystem/Attributes/MALAttributeSet.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "Tags/MALGameplayTags.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "GenericTeamAgentInterface.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace
{
	/** True when both actors expose a team id and share the same non-neutral team. */
	bool MALIsFriendly(AActor* A, AActor* B)
	{
		const IGenericTeamAgentInterface* TeamA = Cast<IGenericTeamAgentInterface>(A);
		const IGenericTeamAgentInterface* TeamB = Cast<IGenericTeamAgentInterface>(B);
		if (TeamA && TeamB)
		{
			const FGenericTeamId IdA = TeamA->GetGenericTeamId();
			const FGenericTeamId IdB = TeamB->GetGenericTeamId();
			return IdA != FGenericTeamId::NoTeam && IdA == IdB;
		}
		return false;
	}
}

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

	// Combo presses must reach the server so ComboStep stays in sync; the ASC mirrors
	// the engine's ServerSetInputPressed/Released when this flag is set.
	bReplicateInputDirectly = true;

	// Default hit feedback; designers can override the camera shake class in BP.
	HitCameraShakeClass = UMALHitCameraShake::StaticClass();

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

	// Fresh combo each activation.
	ResetComboState();

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

	// Listen for hit events fired from the montage (AN_SendHitEvent -> MAL.Event.Hit).
	// Kept as an optional decoupled path; weapon collision is the active damage source.
	const UMALGameplayTags* Tags = UMALGameplayTags::Get();
	UAbilityTask_WaitGameplayEvent* HitTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, Tags->Event_Hit, nullptr, /*bOnlyTriggerOnce*/ false, /*bOnlyMatchExact*/ true);
	if (HitTask)
	{
		HitTask->EventReceived.AddDynamic(this, &UMALAbility_Attack::OnHitEventReceived);
		HitTask->ReadyForActivation();
	}

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

	// Stop any in-flight lunge explicitly; the task is also destroyed by EndAbility, but
	// ending it here guarantees the root motion source is removed before the movement mode
	// is restored.
	if (IsValid(ActiveLungeTask))
	{
		ActiveLungeTask->EndTask();
	}
	ActiveLungeTask = nullptr;

	// Clean up weapon collision in case the ability ends before the disable notify fires
	DisableWeaponCollision();

	// Reset combo state so the next activation starts fresh.
	ResetComboState();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

	// DisableWeaponCollision above may have armed a release for a hit window that is now moot.
	// Clearing after Super also covers the ability ending on its own, normally or cancelled.
	CancelRecoveryRelease();
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
	// Let the base forward/broadcast first, then handle combo chaining.
	Super::OnInputPressed(Value);
	HandleComboInput();
}

void UMALAbility_Attack::HandleComboInput()
{
	if (bComboWindowOpen)
	{
		AdvanceCombo();
	}
	else
	{
		// Pressed outside the window: buffer it briefly and consume when the next window opens.
		bComboInputBuffered = true;
		ComboInputBufferTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	}
}

void UMALAbility_Attack::AdvanceCombo()
{
	// No further section -> stay on the current one (it flows into Recovery and ends).
	if (!ComboSections.IsValidIndex(ComboStep + 1))
	{
		bComboInputBuffered = false;
		ComboInputBufferTime = 0.0;
		return;
	}

	ComboStep++;
	bComboInputBuffered = false;
	bComboWindowOpen = false; // closed until the next section's window notify reopens it
	ComboInputBufferTime = 0.0;

	// Use the ability API (ASC::CurrentMontageJumpToSection under the hood): it sends a
	// reliable server RPC from clients and replicates the section to other clients.
	MontageJumpToSection(ComboSections[ComboStep]);

	UE_LOG(LogTemp, Log, TEXT("UMALAbility_Attack: advanced to combo step %d (section %s)"),
		ComboStep, *ComboSections[ComboStep].ToString());
}

void UMALAbility_Attack::TryConsumeBufferedInput()
{
	if (!bComboInputBuffered)
	{
		return;
	}

	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : ComboInputBufferTime;
	if (Now - ComboInputBufferTime > static_cast<double>(ComboInputBufferWindow))
	{
		// Stale press: don't let an input from several seconds ago auto-chain.
		bComboInputBuffered = false;
		return;
	}

	bComboInputBuffered = false;
	AdvanceCombo();
}

void UMALAbility_Attack::ResetComboState()
{
	ComboStep = 0;
	bComboWindowOpen = false;
	bComboInputBuffered = false;
	ComboInputBufferTime = 0.0;
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

	return ComboStep;
}

void UMALAbility_Attack::OpenComboWindow()
{
	bComboWindowOpen = true;

	// If the player pressed attack before the window opened, chain immediately.
	TryConsumeBufferedInput();
}

void UMALAbility_Attack::CloseComboWindow()
{
	bComboWindowOpen = false;

	// Past this point the chain can no longer be extended, so the rest of the montage is pure
	// recovery. Re-arming the timer here (rather than relying only on the hit window close)
	// keeps the grace period aligned with the last frame the player could still act.
	ScheduleRecoveryRelease();
}

void UMALAbility_Attack::TriggerLunge()
{
	ApplyLunge();
}

void UMALAbility_Attack::OnHitEventReceived(FGameplayEventData Payload)
{
	// Optional event-driven path (requires a MAL Send Hit Event notify in the montage).
	UE_LOG(LogTemp, Log, TEXT("UMALAbility_Attack: hit event received (combo step %d)"),
		GetCurrentComboStepFromMontage());
}

void UMALAbility_Attack::EnableWeaponCollision()
{
	// A new hit window means this section still owns the ability: a release left over from the
	// previous section would end it in the middle of the swing.
	CancelRecoveryRelease();

	// Guard: if already enabled, unbind first to avoid duplicate delegate binding
	if (WeaponCollisionComp)
	{
		WeaponCollisionComp->OnComponentBeginOverlap.RemoveDynamic(this, &UMALAbility_Attack::OnWeaponOverlapBegin);
		WeaponCollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WeaponCollisionComp = nullptr;
	}

	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		return;
	}

	// Find the weapon collision component by name (skip CapsuleComponent etc.)
	WeaponCollisionComp = nullptr;
	TArray<UActorComponent*> Comps;
	Character->GetComponents(Comps);
	for (UActorComponent* Comp : Comps)
	{
		UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Comp);
		if (PrimComp && PrimComp->GetFName() == WeaponCollisionSocketName)
		{
			WeaponCollisionComp = PrimComp;
			break;
		}
	}

	if (!WeaponCollisionComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("UMALAbility_Attack::EnableWeaponCollision - No component named '%s' found on Character!"), *WeaponCollisionSocketName.ToString());
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("UMALAbility_Attack::EnableWeaponCollision - Found weapon collision component: %s"), *WeaponCollisionComp->GetName());

	// New swing: clear the per-swing hit set so each target can be hit once.
	HitActorsThisSwing.Empty();

	// Enable collision and bind overlap
	WeaponCollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	WeaponCollisionComp->SetCollisionObjectType(ECC_WorldDynamic);
	WeaponCollisionComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	WeaponCollisionComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	WeaponCollisionComp->SetGenerateOverlapEvents(true);

	WeaponCollisionComp->OnComponentBeginOverlap.AddDynamic(this, &UMALAbility_Attack::OnWeaponOverlapBegin);
}

void UMALAbility_Attack::DisableWeaponCollision()
{
	if (WeaponCollisionComp)
	{
		WeaponCollisionComp->OnComponentBeginOverlap.RemoveDynamic(this, &UMALAbility_Attack::OnWeaponOverlapBegin);
		WeaponCollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WeaponCollisionComp = nullptr;
	}

	HitActorsThisSwing.Empty();

	// The hit window is over; everything left in the montage is recovery. Do not keep the
	// character locked for animation nobody is going to watch.
	ScheduleRecoveryRelease();
}

void UMALAbility_Attack::ScheduleRecoveryRelease()
{
	UWorld* World = GetWorld();
	if (!World || !IsActive())
	{
		return;
	}

	// Re-arm cleanly: one montage can open and close several hit windows.
	World->GetTimerManager().ClearTimer(RecoveryReleaseTimer);

	if (RecoveryReleaseDelay <= 0.0f)
	{
		ReleaseRecovery();
		return;
	}

	World->GetTimerManager().SetTimer(RecoveryReleaseTimer, this,
		&UMALAbility_Attack::ReleaseRecovery, RecoveryReleaseDelay, false);
}

void UMALAbility_Attack::CancelRecoveryRelease()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RecoveryReleaseTimer);
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

void UMALAbility_Attack::OnWeaponOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar || !Avatar->HasAuthority())
	{
		// Hit resolution is server-authoritative; clients only play cosmetics.
		return;
	}

	if (!OtherActor || OtherActor == Avatar)
	{
		return;
	}

	// One hit per actor per swing.
	if (HitActorsThisSwing.Contains(OtherActor))
	{
		return;
	}

	if (MALIsFriendly(Avatar, OtherActor))
	{
		return;
	}

	HitActorsThisSwing.Add(OtherActor);

	FMALHitResult HitResult;
	HitResult.HitActor = OtherActor;
	HitResult.WeaponSocketName = WeaponCollisionSocketName;
	HitResult.ComboStep = GetCurrentComboStepFromMontage();

	if (bFromSweep && SweepResult.bBlockingHit)
	{
		HitResult.HitLocation = SweepResult.ImpactPoint;
	}
	else if (OtherComp)
	{
		HitResult.HitLocation = OtherComp->GetComponentLocation();
	}
	else
	{
		HitResult.HitLocation = OverlappedComp ? OverlappedComp->GetComponentLocation() : FVector::ZeroVector;
	}

	OnWeaponHit(HitResult);
}

void UMALAbility_Attack::OnWeaponHit(const FMALHitResult& HitResult)
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar || !Avatar->HasAuthority())
	{
		return;
	}

	if (!HitResult.HitActor || HitResult.HitActor == Avatar)
	{
		return;
	}

	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(HitResult.HitActor);
	if (!SourceASC || !TargetASC)
	{
		return;
	}

	const int32 Step = GetCurrentComboStepFromMontage();
	const float LevelScale = GetLevelScaledValue(TEXT("Damage"), 1.0f);
	const float Damage = FMath::Max(0.0f, BaseDamage * LevelScale * (1.0f + ComboDamageBonusPerStep * Step));

	TSubclassOf<UGameplayEffect> GEClass = DamageGEClass;
	if (!GEClass)
	{
		GEClass = UMALDamageGameplayEffect::StaticClass();
	}

	UE_LOG(LogTemp, Verbose, TEXT("UMALAbility_Attack: weapon hit on %s (combo step %d, damage %.1f, GE %s, targetSets %d)"),
		*HitResult.HitActor->GetName(), Step, Damage, *GetNameSafe(GEClass.Get()), TargetASC->GetSpawnedAttributes().Num());

	if (Damage <= 0.0f)
	{
		return;
	}

	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddInstigator(Avatar, Avatar);
	Context.AddSourceObject(this);

	const FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(GEClass, GetAbilityLevel(), Context);
	if (SpecHandle.IsValid())
	{
		SpecHandle.Data->SetSetByCallerMagnitude(UMALGameplayTags::Get()->Data_Damage, Damage);
		SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UMALAbility_Attack: MakeOutgoingSpec failed for %s"), *GetNameSafe(GEClass.Get()));
	}

	// Confirmed hit: ask the attacker's owning client to play camera shake + local hit-stop.
	if (ACharacter* AttackerCharacter = Cast<ACharacter>(Avatar))
	{
		if (AMALPlayerController* PC = Cast<AMALPlayerController>(AttackerCharacter->GetController()))
		{
			PC->ClientOnHitLanded(HitStopDuration, HitCameraShakeClass);
		}
	}

	// Broadcast WeaponHit event for future listeners (UI, hit-stop, audio, ...)
	FGameplayEventData EventData;
	EventData.Instigator = Avatar;
	EventData.Target = HitResult.HitActor;
	EventData.EventMagnitude = Damage;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		Avatar, UMALGameplayTags::Get()->Event_WeaponHit, EventData);
}
