// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/MALCombatEventsSubsystem.h"
#include "Player/MALPlayerController.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"

bool UMALCombatEventsSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}

	// Editor previews and asset editors have no game loop to resolve damage in.
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void UMALCombatEventsSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// Clients never resolve damage, so only the authority needs the routing listener.
	if (InWorld.GetNetMode() != NM_Client)
	{
		OnDamageResolved.AddUObject(this, &UMALCombatEventsSubsystem::RouteDamageNumber);
	}
}

void UMALCombatEventsSubsystem::BroadcastDamageResolved(const FMALDamageResolvedEvent& Event)
{
	if (!GetWorld() || GetWorld()->GetNetMode() == NM_Client)
	{
		return;
	}

	OnDamageResolved.Broadcast(Event);
}

void UMALCombatEventsSubsystem::RouteDamageNumber(const FMALDamageResolvedEvent& Event)
{
	if (!Event.Victim || Event.Amount <= 0.0f)
	{
		return;
	}

	// Both roles can belong to the same client (self damage), and a hit is usually relevant to two
	// different ones, so deduplicate before sending.
	TSet<AMALPlayerController*> Recipients;

	auto NotifyOwner = [&Recipients, &Event](AActor* Actor)
	{
		// The event reports whoever owns the AttributeSet: the PlayerState for players, the pawn
		// for enemies. Resolve both to the pawn that the controller actually hangs off.
		APawn* Pawn = Cast<APawn>(Actor);
		if (!Pawn)
		{
			if (const APlayerState* PlayerState = Cast<APlayerState>(Actor))
			{
				Pawn = PlayerState->GetPawn();
			}
		}

		// An AI-driven pawn has no MAL player controller, so enemies simply never see their own
		// numbers.
		if (!Pawn)
		{
			return;
		}

		AMALPlayerController* PlayerController = Cast<AMALPlayerController>(Pawn->GetController());
		if (!PlayerController || Recipients.Contains(PlayerController))
		{
			return;
		}

		Recipients.Add(PlayerController);
		PlayerController->ClientShowDamageNumber(
			Event.Amount, Event.bCritical, Event.bMagic, Event.bKillingBlow, Event.WorldLocation);
	};

	NotifyOwner(Event.Victim);
	NotifyOwner(Event.Instigator);
}
