// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Combat/MALCombatEvents.h"
#include "MALCombatEventsSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FMALDamageResolvedSignature, const FMALDamageResolvedEvent&);

/**
 * UMALCombatEventsSubsystem
 *
 * Server-side bus between combat resolution and presentation.
 *
 * UMALAttributeSet publishes here after folding meta damage into Health; the subsystem owns the
 * routing to the player controllers that should draw a number. That keeps the attribute layer
 * free of any HUD/PlayerController dependency while still delivering the hit to both the victim's
 * owner and the attacker's owner.
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

private:
	/** Deliver a resolved hit to every client that should draw a number for it. */
	void RouteDamageNumber(const FMALDamageResolvedEvent& Event);
};
