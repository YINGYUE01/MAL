// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayTagContainer.h"
#include "Templates/SubclassOf.h"
#include "Combat/MALHitResult.h"
#include "MALDamageExecutor.generated.h"

class AActor;
class UCameraShakeBase;
class UGameplayEffect;

/**
 * FMALWeaponHitRequest
 *
 * Everything the damage pipeline needs for one resolved weapon hit. The ability gathers it
 * from its own (Blueprint-exposed) settings, so tuning stays on the ability while the
 * resolution itself lives in one place.
 */
USTRUCT(BlueprintType)
struct FMALWeaponHitRequest
{
	GENERATED_BODY()

	/** Avatar that owns the hitting ability. Must be on the authority. */
	UPROPERTY(BlueprintReadWrite, Category = "MAL|Combat")
	TObjectPtr<AActor> Instigator = nullptr;

	/** Target, combo step and impact point reported by the weapon component. */
	UPROPERTY(BlueprintReadWrite, Category = "MAL|Combat")
	FMALHitResult Hit;

	/**
	 * Recorded as the effect context / cue source object (normally the attacking ability).
	 * Falls back to Instigator when null.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "MAL|Combat")
	TObjectPtr<UObject> SourceObject = nullptr;

	/** Damage effect class; null falls back to UMALDamageGameplayEffect. */
	UPROPERTY(BlueprintReadWrite, Category = "MAL|Combat|Damage")
	TSubclassOf<UGameplayEffect> DamageGEClass;

	UPROPERTY(BlueprintReadWrite, Category = "MAL|Combat|Damage")
	float BaseDamage = 10.0f;

	/** Extra damage multiplier per combo step (0.15 => 15% per step). */
	UPROPERTY(BlueprintReadWrite, Category = "MAL|Combat|Damage")
	float ComboDamageBonusPerStep = 0.15f;

	/** Level written into the outgoing spec (ability level). */
	UPROPERTY(BlueprintReadWrite, Category = "MAL|Combat|Damage")
	float AbilityLevel = 1.0f;

	/** Result of the ability's level-scaling lookup for the "Damage" row. */
	UPROPERTY(BlueprintReadWrite, Category = "MAL|Combat|Damage")
	float LevelScale = 1.0f;

	/** Cue executed at the impact point; empty disables it. */
	UPROPERTY(BlueprintReadWrite, Category = "MAL|Combat|Feedback")
	FGameplayTag ImpactCueTag;

	/** Local hit-stop requested on the attacker's client. */
	UPROPERTY(BlueprintReadWrite, Category = "MAL|Combat|Feedback")
	float HitStopDuration = 0.06f;

	/** Camera shake played on the attacker's client. */
	UPROPERTY(BlueprintReadWrite, Category = "MAL|Combat|Feedback")
	TSubclassOf<UCameraShakeBase> CameraShakeClass;

	/** Whether the victim is pushed away from the attacker on a confirmed hit. */
	UPROPERTY(BlueprintReadWrite, Category = "MAL|Combat|Knockback")
	bool bKnockback = false;

	/** Push distance in world units, measured along the attacker -> victim line. */
	UPROPERTY(BlueprintReadWrite, Category = "MAL|Combat|Knockback")
	float KnockbackDistance = 0.0f;

	/** Seconds the push takes. Fixed distance, so this only sets how violent it looks. */
	UPROPERTY(BlueprintReadWrite, Category = "MAL|Combat|Knockback")
	float KnockbackDuration = 0.25f;
};

/**
 * UMALDamageExecutor
 *
 * Single entry point for "a hit was confirmed, resolve it": computes damage from the
 * attacker's settings, applies the damage GameplayEffect and then drives the feedback
 * (attacker hit-stop / camera shake, impact cue, MAL.Event.WeaponHit).
 *
 * Server-only by design. Every piece of presentation it triggers is either a client RPC or
 * a replicated cue, so calling it on a client would double up on effects.
 */
UCLASS()
class MAL_API UMALDamageExecutor : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Resolve one weapon hit on the authority.
	 *
	 * @return Damage actually applied; 0 when the hit was ignored (no ASC, same team as the
	 *         attacker, dead target, zero damage, failed spec or a target that rejected the effect).
	 */
	UFUNCTION(BlueprintCallable, Category = "MAL|Combat")
	static float ExecuteWeaponHit(const FMALWeaponHitRequest& Request);

	/** Damage for a given combo step, before attack/defense from the execution. */
	UFUNCTION(BlueprintPure, Category = "MAL|Combat")
	static float ComputeDamage(float BaseDamage, float LevelScale, float ComboDamageBonusPerStep, int32 ComboStep);

	/**
	 * True when both actors resolve to the same team and that team is not "no team".
	 *
	 * Lives next to the damage entry point because every path that applies damage has to agree
	 * with the hit window about what counts as friendly fire.
	 */
	static bool IsFriendly(const AActor* A, const AActor* B);

	/**
	 * Push a character away from the attacker. Server-only; silently does nothing when the victim
	 * cannot move (not a character, movement disabled by death, zero distance).
	 *
	 * The push is a fixed-distance root motion source rather than a launch impulse, so the
	 * authored distance is what the victim actually travels regardless of its friction settings.
	 */
	static void ApplyKnockback(AActor* Attacker, AActor* Victim, float Distance, float Duration);
};
