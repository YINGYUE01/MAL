// Copyright Epic Games, Inc. All Rights Reserved.


#include "GameAbilities/MALAbility_Attack.h"

#include "GameAbilities/Tasks/MALAbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionMoveToForce.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "GameplayTagContainer.h"
#include "Tags/MALGameplayTags.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Components/PrimitiveComponent.h"

UMALAbility_Attack::UMALAbility_Attack()
{
	const UMALGameplayTags* Tags = UMALGameplayTags::Get();

	// Set ability tags
	AbilityTags.AddTag(Tags->Ability_Attack);
	
	// Block other attacks while attacking (mutual exclusion)
	ActivationBlockedTags.AddTag(Tags->Status_Attacking);
	
	// Tags applied during activation
	ActivationOwnedTags.AddTag(Tags->Status_Attacking);

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
	// asset; the SlotName argument here is documentation/debug only.
	UMALAbilityTask_PlayMontageAndWait* PlayTask = UMALAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		AttackMontage,
		1.0f,                  // Rate
		StartSection,          // StartSection
		true,                  // bStopWhenAbilityEnds
		1.0f,                  // AnimRootMotionTranslationScale
		0.0f,                  // StartTimeSeconds
		FName("DefaultSlot")); // SlotName (documentation/debug only)

	if (PlayTask)
	{
		PlayTask->OnCompleted.AddDynamic(TObjectPtr<UMALAbility_Attack>(this), &UMALAbility_Attack::OnMontageCompleted);
		PlayTask->OnBlendOut.AddDynamic(TObjectPtr<UMALAbility_Attack>(this), &UMALAbility_Attack::OnMontageBlendOut);
		PlayTask->OnInterrupted.AddDynamic(TObjectPtr<UMALAbility_Attack>(this), &UMALAbility_Attack::OnMontageInterrupted);
		PlayTask->OnCancelled.AddDynamic(TObjectPtr<UMALAbility_Attack>(this), &UMALAbility_Attack::OnMontageCancelled);

		// IMPORTANT: AbilityTasks must be explicitly activated. Without this the montage
		// never plays and the ability never ends via OnMontageCompleted (Status.Attacking
		// would stay applied forever).
		PlayTask->ReadyForActivation();
	}

	// Listen for hit events fired from the montage (AN_SendHitEvent -> MAL.Event.Hit).
	// Keeps combat decoupled from animation; damage resolution is wired here later.
	const UMALGameplayTags* Tags = UMALGameplayTags::Get();
	UAbilityTask_WaitGameplayEvent* HitTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, Tags->Event_Hit, nullptr, /*bOnlyTriggerOnce*/ false, /*bOnlyMatchExact*/ true);
	if (HitTask)
	{
		HitTask->EventReceived.AddDynamic(TObjectPtr<UMALAbility_Attack>(this), &UMALAbility_Attack::OnHitEventReceived);
		HitTask->ReadyForActivation();
	}

	// NOTE: the forward lunge is intentionally NOT applied here. It is triggered per-hit by
	// the montage's AN_Lunge notify -> TriggerLunge() -> ApplyLunge(), so each swing's lunge
	// timing matches its animation.
}

void UMALAbility_Attack::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	UE_LOG(LogTemp, Log, TEXT("UMALAbility_Attack::EndAbility - Attack ended. Cancelled: %s"), bWasCancelled ? TEXT("true") : TEXT("false"));

	// The lunge root motion task is owned by this ability and is destroyed on EndAbility;
	// its OnDestroy removes the root motion source and restores the previous movement mode
	// automatically. No manual timer/movement cleanup required here.

	// Clean up weapon collision in case the ability ends before the disable notify fires
	DisableWeaponCollision();

	// Reset combo state so the next activation starts fresh.
	ResetComboState();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UMALAbility_Attack::OnMontageCompleted()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UMALAbility_Attack::OnMontageBlendOut()
{
	// Montage is blending out but hasn't fully ended yet
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

	// Lunge toward the CONTROL direction (camera/aim yaw), flattened to the ground,
	// instead of the actor's facing. The server receives the client's ControlRotation via
	// ServerMove, so this is correct on both the client (predicted) and the server.
	// (Future: swap control direction for a direction-toward-target once enemies exist.)
	const AController* Controller = Character->GetController();
	const FRotator ControlRotation = Controller ? Controller->GetControlRotation() : Character->GetActorRotation();
	const FVector LungeDir = FRotationMatrix(FRotator(0.f, ControlRotation.Yaw, 0.f)).GetUnitAxis(EAxis::X);
	const FVector TargetLocation = Character->GetActorLocation() + LungeDir * LungeDistance;

	// Snap the character's facing to the lunge (control) direction so the body doesn't slide
	// sideways during the dash. Guarded by Controller: without one we keep the current facing.
	// NOTE: TargetLocation is derived from LungeDir above and is independent of actor rotation,
	// and a capsule is yaw-symmetric, so this does NOT change where MoveToForce travels.
	if (Controller)
	{
		Character->SetActorRotation(FRotator(0.f, ControlRotation.Yaw, 0.f));
	}

	// Drive the lunge through GAS root motion instead of manual SetActorLocation.
	// This respects collision/gravity, is network-prediction safe, and automatically
	// restores the previous movement mode when the task ends.
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
		LungeTask->OnTimedOut.AddDynamic(TObjectPtr<UMALAbility_Attack>(this), &UMALAbility_Attack::OnLungeFinished);
		LungeTask->OnTimedOutAndDestinationReached.AddDynamic(TObjectPtr<UMALAbility_Attack>(this), &UMALAbility_Attack::OnLungeFinished);
		LungeTask->ReadyForActivation();
	}
}

void UMALAbility_Attack::OnLungeFinished()
{
	// The root motion source is removed and the movement mode restored automatically when
	// the task ends. Kept as a hook for future follow-up logic (e.g. recovery/chain windows).
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
		// Pressed outside the window: buffer it and consume when the next window opens.
		bComboInputBuffered = true;
	}
}

void UMALAbility_Attack::AdvanceCombo()
{
	// No further section -> stay on the current one (it flows into Recovery and ends).
	if (!ComboSections.IsValidIndex(ComboStep + 1))
	{
		bComboInputBuffered = false;
		return;
	}

	ComboStep++;
	bComboInputBuffered = false;
	bComboWindowOpen = false; // closed until the next section's window notify reopens it

	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (AnimInstance && AttackMontage)
	{
		AnimInstance->Montage_JumpToSection(ComboSections[ComboStep], AttackMontage);
		UE_LOG(LogTemp, Log, TEXT("UMALAbility_Attack: advanced to combo step %d (section %s)"),
			ComboStep, *ComboSections[ComboStep].ToString());
	}
}

void UMALAbility_Attack::TryConsumeBufferedInput()
{
	if (bComboInputBuffered)
	{
		bComboInputBuffered = false;
		AdvanceCombo();
	}
}

void UMALAbility_Attack::ResetComboState()
{
	ComboStep = 0;
	bComboWindowOpen = false;
	bComboInputBuffered = false;
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
}

void UMALAbility_Attack::TriggerLunge()
{
	ApplyLunge();
}

void UMALAbility_Attack::OnHitEventReceived(FGameplayEventData Payload)
{
	// Placeholder: damage/hit resolution is wired here once enemies + a damage GE exist.
	// Payload.Instigator/Target and (future) EventMagnitude can carry the combo step.
	UE_LOG(LogTemp, Log, TEXT("UMALAbility_Attack: hit event received (combo step %d)"), ComboStep);
}

void UMALAbility_Attack::EnableWeaponCollision()
{
	// Guard: if already enabled, unbind first to avoid duplicate delegate binding
	if (WeaponCollisionComp)
	{
		WeaponCollisionComp->OnComponentBeginOverlap.RemoveDynamic(TObjectPtr<UMALAbility_Attack>(this), &UMALAbility_Attack::OnWeaponOverlapBegin);
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

	// Enable collision and bind overlap
	WeaponCollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	WeaponCollisionComp->SetCollisionObjectType(ECC_WorldDynamic);
	WeaponCollisionComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	WeaponCollisionComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	WeaponCollisionComp->OnComponentBeginOverlap.AddDynamic(TObjectPtr<UMALAbility_Attack>(this), &UMALAbility_Attack::OnWeaponOverlapBegin);
}

void UMALAbility_Attack::DisableWeaponCollision()
{
	if (WeaponCollisionComp)
	{
		WeaponCollisionComp->OnComponentBeginOverlap.RemoveDynamic(TObjectPtr<UMALAbility_Attack>(this), &UMALAbility_Attack::OnWeaponOverlapBegin);
		WeaponCollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WeaponCollisionComp = nullptr;
	}
}

void UMALAbility_Attack::OnWeaponOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == GetAvatarActorFromActorInfo())
	{
		return;
	}

	FMALHitResult HitResult;
	HitResult.HitActor = OtherActor;
	HitResult.WeaponSocketName = WeaponCollisionSocketName;
	HitResult.ComboStep = ComboStep;
	HitResult.HitLocation = OverlappedComp ? OverlappedComp->GetComponentLocation() : FVector::ZeroVector;

	OnWeaponHit(HitResult);
}

void UMALAbility_Attack::OnWeaponHit(const FMALHitResult& HitResult)
{
	if (!HitResult.HitActor || HitResult.HitActor == GetAvatarActorFromActorInfo())
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("UMALAbility_Attack: weapon hit on %s (combo step %d)"),
		*HitResult.HitActor->GetName(), HitResult.ComboStep);

	// TODO: Apply DamageGE once enemy ASC + GE assets exist
	// FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
	// ASC->ApplyGameplayEffectToTarget(...)

	// Broadcast WeaponHit event for future listeners
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (ActorInfo)
	{
		UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
		if (ASC)
		{
			FGameplayEventData EventData;
			EventData.Instigator = GetAvatarActorFromActorInfo();
			EventData.Target = HitResult.HitActor;
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(GetAvatarActorFromActorInfo(),
				UMALGameplayTags::Get()->Event_WeaponHit, EventData);
		}
	}
}
