// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/MALDamageExecutor.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameAbilities/MALDamageGameplayEffect.h"
#include "GameplayEffect.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GenericTeamAgentInterface.h"
#include "Player/MALPlayerController.h"
#include "Tags/MALGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MALDamageExecutor)

float UMALDamageExecutor::ComputeDamage(float BaseDamage, float LevelScale, float ComboDamageBonusPerStep, int32 ComboStep)
{
	return FMath::Max(0.0f, BaseDamage * LevelScale * (1.0f + ComboDamageBonusPerStep * ComboStep));
}

bool UMALDamageExecutor::IsFriendly(const AActor* A, const AActor* B)
{
	if (!A || !B)
	{
		return false;
	}

	const FGenericTeamId TeamA = FGenericTeamId::GetTeamIdentifier(A);
	const FGenericTeamId TeamB = FGenericTeamId::GetTeamIdentifier(B);

	// NoTeam means "unassigned", not a side. Comparing it as a value would make every neutral
	// actor friendly to every other neutral actor and disable combat against unconfigured pawns.
	return TeamA != FGenericTeamId::NoTeam && TeamA == TeamB;
}

float UMALDamageExecutor::ExecuteWeaponHit(const FMALWeaponHitRequest& Request)
{
	AActor* Avatar = Request.Instigator;
	AActor* HitActor = Request.Hit.HitActor;
	if (!Avatar || !Avatar->HasAuthority() || !HitActor || HitActor == Avatar)
	{
		return 0.0f;
	}

	UAbilitySystemComponent* SourceASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Avatar);
	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(HitActor);
	if (!SourceASC || !TargetASC)
	{
		return 0.0f;
	}

	// Corpses keep their collision. UMALWeaponComponent filters them already; this guards every
	// other caller of the damage entry point (future abilities, scripted damage).
	if (TargetASC->HasMatchingGameplayTag(UMALGameplayTags::Get()->Status_Dead))
	{
		return 0.0f;
	}

	// Friendly fire is rejected here as well as in the hit window. This is a public entry point:
	// an ability that resolves damage without going through UMALWeaponComponent would otherwise
	// bypass the team filter entirely.
	if (IsFriendly(Avatar, HitActor))
	{
		return 0.0f;
	}

	const int32 Step = Request.Hit.ComboStep;
	const float Damage = ComputeDamage(Request.BaseDamage, Request.LevelScale, Request.ComboDamageBonusPerStep, Step);
	if (Damage <= 0.0f)
	{
		return 0.0f;
	}

	TSubclassOf<UGameplayEffect> GEClass = Request.DamageGEClass;
	if (!GEClass)
	{
		GEClass = UMALDamageGameplayEffect::StaticClass();
	}

	UE_LOG(LogTemp, Verbose, TEXT("UMALDamageExecutor: weapon hit on %s (combo step %d, damage %.1f, GE %s, targetSets %d)"),
		*HitActor->GetName(), Step, Damage, *GetNameSafe(GEClass.Get()), TargetASC->GetSpawnedAttributes().Num());

	// Source object stays the attacking ability when one is supplied: cues and execution code
	// read it, so substituting the avatar here would change what gameplay cue blueprints see.
	UObject* SourceObject = Request.SourceObject ? Request.SourceObject.Get() : Avatar;

	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddInstigator(Avatar, Avatar);
	Context.AddSourceObject(SourceObject);

	// The contact point travels on the context because it is the only channel the attribute layer
	// can read when it places the damage number and the impact cue. Without it that code falls
	// back to the ASC owner's location, which is the PlayerState for player characters and does
	// not follow the pawn.
	FHitResult HitResult;
	HitResult.HitObjectHandle = FActorInstanceHandle(Request.Hit.HitActor);
	HitResult.Location = Request.Hit.HitLocation;
	HitResult.ImpactPoint = Request.Hit.HitLocation;
	HitResult.ImpactNormal = FVector::UpVector;
	HitResult.bBlockingHit = true;

	// TraceStart becomes the context's world origin, so anchoring it at the hit keeps the
	// replicated hit result precise even far from the map origin.
	HitResult.TraceStart = Request.Hit.HitLocation;
	Context.AddHitResult(HitResult, true);

	const FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(GEClass, Request.AbilityLevel, Context);
	if (!SpecHandle.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("UMALDamageExecutor: MakeOutgoingSpec failed for %s"), *GetNameSafe(GEClass.Get()));
		return 0.0f;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(UMALGameplayTags::Get()->Data_Damage, Damage);

	// The effect can still be rejected on arrival (immunity tags, application requirements, a
	// missing attribute set). Feedback must follow the damage: a hit that was never applied must
	// not freeze the attacker or fire the impact cue.
	const FActiveGameplayEffectHandle EffectHandle = SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
	if (!EffectHandle.WasSuccessfullyApplied())
	{
		UE_LOG(LogTemp, Verbose, TEXT("UMALDamageExecutor: %s rejected the damage effect (no feedback fired)"), *HitActor->GetName());
		return 0.0f;
	}

	// Confirmed hit: ask the attacker's owning client to play camera shake + local hit-stop.
	if (ACharacter* AttackerCharacter = Cast<ACharacter>(Avatar))
	{
		if (AMALPlayerController* PlayerController = Cast<AMALPlayerController>(AttackerCharacter->GetController()))
		{
			PlayerController->ClientOnHitLanded(Request.HitStopDuration, Request.CameraShakeClass);
		}
	}

	// Impact feedback at the point of contact. Nothing here plays locally: this code only runs
	// on the server, so the cue is what replicates the effect out to the clients.
	if (Request.ImpactCueTag.IsValid())
	{
		FGameplayCueParameters CueParams;
		CueParams.Location = Request.Hit.HitLocation;
		CueParams.Instigator = Avatar;
		CueParams.EffectCauser = Avatar;
		CueParams.SourceObject = SourceObject;
		CueParams.RawMagnitude = Damage;
		CueParams.AbilityLevel = Request.AbilityLevel;

		SourceASC->ExecuteGameplayCue(Request.ImpactCueTag, CueParams);
	}

	// Broadcast WeaponHit event for future listeners (UI, hit-stop, audio, ...)
	FGameplayEventData EventData;
	EventData.Instigator = Avatar;
	EventData.Target = HitActor;
	EventData.EventMagnitude = Damage;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		Avatar, UMALGameplayTags::Get()->Event_WeaponHit, EventData);

	return Damage;
}
