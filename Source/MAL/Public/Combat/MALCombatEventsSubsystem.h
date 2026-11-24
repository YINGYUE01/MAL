// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/WeakObjectPtr.h"
#include "Combat/MALCombatEvents.h"
#include "MALCombatEventsSubsystem.generated.h"

class AActor;
class AMALPlayerController;

DECLARE_MULTICAST_DELEGATE_OneParam(FMALDamageResolvedSignature, const FMALDamageResolvedEvent&);

/**
 * UMALCombatEventsSubsystem
 *
 * Server-side bus between combat resolution and presentation.
 *
 * UMALAttributeSet publishes resolved damage here; the subsystem routes it to the player
 * controllers that should draw a number. It also owns the per-player Boss bar visibility list:
 * a Boss is visible to a controller only while that controller's pawn is inside the Boss's
 * configured activation radius. Clients never infer this from replicated state.
 *
 * Everything here runs on the authority only: clients never resolve damage, and a client-side
 * predicted effect must not draw a number on top of the replicated one.
 */
UCLASS()
class MAL_API UMALCombatEventsSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/**
	 * Publish one resolved damage application. No-op on clients and when nobody is listening.
	 * Called from UMALAttributeSet::PostGameplayEffectExecute, which only runs on the authority.
	 */
	void BroadcastDamageResolved(const FMALDamageResolvedEvent& Event);

	/** Fired on the authority for every resolved damage event. */
	FMALDamageResolvedSignature OnDamageResolved;

	/** Register a live Boss so it participates in the radius check, even before it is damaged. */
	void RegisterBoss(AActor* Boss);

	/** Send the current per-player visible Boss list to one local controller (late join). */
	void SyncToController(AMALPlayerController* Controller);

protected:
	virtual void Deinitialize() override;

private:
	/** Deliver a resolved hit to every client that should draw a number for it. */
	void RouteDamageNumber(const FMALDamageResolvedEvent& Event);

	/** 0.1s server poll: drop dead Bosses and recompute each player's visible set. */
	void PollBossBars();

	/** Add/remove bars for one controller so its list matches its own pawn's radius. */
	void UpdateVisibilityForController(AMALPlayerController* Controller);

	/** Remove one Boss from the known list and from every player's visible list. */
	void UnregisterBoss(AActor* Boss, bool bFadeOut);

	/** Refresh every controller from scratch; used when a Boss actor is destroyed. */
	void SyncAllControllers();

	/** Current visible list for one controller, without sending or mutating state. */
	TArray<AActor*> BuildVisibleBossListFor(AMALPlayerController* Controller) const;

	/** Per-Boss activation radius, falling back to the config class default. */
	float GetBossBarRadius(const AActor* Boss) const;

	/** Destruction cannot be resolved to a per-actor remove reliably; refresh every client. */
	UFUNCTION()
	void HandleBossDestroyed(AActor* DestroyedActor);

	/** Server-only Boss registry, in registration order. */
	TArray<TWeakObjectPtr<AActor>> KnownBosses;

	/** Per-controller visible Boss list, in the order bars were added. */
	TMap<TWeakObjectPtr<AMALPlayerController>, TArray<TWeakObjectPtr<AActor>>> VisibleBossesByController;

	/** Repeating 0.1s poll while at least one Boss is known. */
	FTimerHandle BossPollTimer;
};