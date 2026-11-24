// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/MALCombatEventsSubsystem.h"

#include "Character/MALEnemyCharacter.h"
#include "Data/MALBossAttackSet.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Player/MALPlayerController.h"
#include "Tags/MALGameplayTags.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogMALBossBar, Log, All);

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

void UMALCombatEventsSubsystem::RegisterBoss(AActor* Boss)
{
	if (!Boss || !GetWorld() || GetWorld()->GetNetMode() == NM_Client)
	{
		return;
	}

	AMALEnemyCharacter* Enemy = Cast<AMALEnemyCharacter>(Boss);
	const UMALGameplayTags* Tags = UMALGameplayTags::Get();
	if (!Enemy || !Tags || Enemy->IsDead() || !Enemy->EnemyTypeTag.MatchesTag(Tags->Enemy_Type_Boss))
	{
		return;
	}

	const TWeakObjectPtr<AActor> Key(Boss);
	if (KnownBosses.Contains(Key))
	{
		return;
	}

	KnownBosses.Add(Key);
	Boss->OnDestroyed.AddDynamic(this, &UMALCombatEventsSubsystem::HandleBossDestroyed);

	UE_LOG(LogMALBossBar, Log, TEXT("register %s radius=%.0f"),
		*GetNameSafe(Boss), GetBossBarRadius(Boss));

	UWorld* World = GetWorld();
	if (World && !BossPollTimer.IsValid())
	{
		World->GetTimerManager().SetTimer(BossPollTimer, this,
			&UMALCombatEventsSubsystem::PollBossBars, 0.1f, true);
	}
}

void UMALCombatEventsSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BossPollTimer);

		for (const TWeakObjectPtr<AActor>& WeakBoss : KnownBosses)
		{
			if (AActor* Boss = WeakBoss.Get())
			{
				Boss->OnDestroyed.RemoveDynamic(this, &UMALCombatEventsSubsystem::HandleBossDestroyed);
			}
		}
	}

	BossPollTimer.Invalidate();
	KnownBosses.Reset();
	VisibleBossesByController.Reset();
	OnDamageResolved.RemoveAll(this);

	Super::Deinitialize();
}

void UMALCombatEventsSubsystem::PollBossBars()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (int32 Index = KnownBosses.Num() - 1; Index >= 0; --Index)
	{
		AActor* Boss = KnownBosses[Index].Get();
		if (!Boss)
		{
			KnownBosses.RemoveAt(Index);
			SyncAllControllers();
			continue;
		}

		const AMALEnemyCharacter* Enemy = Cast<AMALEnemyCharacter>(Boss);
		if (!Enemy || Enemy->IsDead())
		{
			UnregisterBoss(Boss, true);
		}
	}

	// A controller can be destroyed without taking its weak-keyed entry with it.
	for (auto It = VisibleBossesByController.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (AMALPlayerController* PlayerController = Cast<AMALPlayerController>(It->Get()))
		{
			UpdateVisibilityForController(PlayerController);
		}
	}

	if (KnownBosses.Num() == 0)
	{
		World->GetTimerManager().ClearTimer(BossPollTimer);
		BossPollTimer.Invalidate();
		VisibleBossesByController.Reset();
	}
}

void UMALCombatEventsSubsystem::UpdateVisibilityForController(AMALPlayerController* Controller)
{
	if (!Controller)
	{
		return;
	}

	const TWeakObjectPtr<AMALPlayerController> ControllerKey(Controller);
	TArray<TWeakObjectPtr<AActor>>& Visible = VisibleBossesByController.FindOrAdd(ControllerKey);

	const APawn* Pawn = Controller->GetPawn();
	if (!Pawn)
	{
		for (const TWeakObjectPtr<AActor>& WeakBoss : Visible)
		{
			if (AActor* Boss = WeakBoss.Get())
			{
				Controller->ClientRemoveBossBar(Boss, false);
			}
		}

		Visible.Reset();
		return;
	}

	TArray<AActor*> DesiredBosses;
	TSet<AActor*> DesiredSet;
	for (const TWeakObjectPtr<AActor>& WeakBoss : KnownBosses)
	{
		AActor* Boss = WeakBoss.Get();
		const AMALEnemyCharacter* Enemy = Cast<AMALEnemyCharacter>(Boss);
		if (!Enemy || Enemy->IsDead())
		{
			continue;
		}

		const float Radius = GetBossBarRadius(Boss);
		if (Radius <= 0.0f)
		{
			continue;
		}

		if (FVector::Dist2D(Pawn->GetActorLocation(), Boss->GetActorLocation()) <= Radius)
		{
			DesiredBosses.Add(Boss);
			DesiredSet.Add(Boss);
		}
	}

	// Remove bars the player has walked away from. A Boss that died gets a fade; walking out of
	// range is a pure visibility change and removes immediately.
	for (int32 Index = Visible.Num() - 1; Index >= 0; --Index)
	{
		AActor* Boss = Visible[Index].Get();
		if (Boss && DesiredSet.Contains(Boss))
		{
			continue;
		}

		if (Boss)
		{
			const AMALEnemyCharacter* Enemy = Cast<AMALEnemyCharacter>(Boss);
			Controller->ClientRemoveBossBar(Boss, Enemy && Enemy->IsDead());
		}

		Visible.RemoveAt(Index);
	}

	TSet<AActor*> Existing;
	for (const TWeakObjectPtr<AActor>& WeakBoss : Visible)
	{
		if (AActor* Boss = WeakBoss.Get())
		{
			Existing.Add(Boss);
		}
	}

	for (AActor* Boss : DesiredBosses)
	{
		if (Existing.Contains(Boss))
		{
			continue;
		}

		Visible.Add(Boss);
		Controller->ClientAddBossBar(Boss);
	}
}

TArray<AActor*> UMALCombatEventsSubsystem::BuildVisibleBossListFor(AMALPlayerController* Controller) const
{
	TArray<AActor*> VisibleBosses;
	const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	if (!Pawn)
	{
		return VisibleBosses;
	}

	for (const TWeakObjectPtr<AActor>& WeakBoss : KnownBosses)
	{
		AActor* Boss = WeakBoss.Get();
		const AMALEnemyCharacter* Enemy = Cast<AMALEnemyCharacter>(Boss);
		if (!Enemy || Enemy->IsDead())
		{
			continue;
		}

		const float Radius = GetBossBarRadius(Boss);
		if (Radius <= 0.0f)
		{
			continue;
		}

		if (FVector::Dist2D(Pawn->GetActorLocation(), Boss->GetActorLocation()) <= Radius)
		{
			VisibleBosses.Add(Boss);
		}
	}

	return VisibleBosses;
}

void UMALCombatEventsSubsystem::SyncToController(AMALPlayerController* Controller)
{
	if (!Controller)
	{
		return;
	}

	const TArray<AActor*> VisibleBosses = BuildVisibleBossListFor(Controller);
	Controller->ClientSyncBossBars(VisibleBosses);

	const TWeakObjectPtr<AMALPlayerController> ControllerKey(Controller);
	TArray<TWeakObjectPtr<AActor>>& Stored = VisibleBossesByController.FindOrAdd(ControllerKey);
	Stored.Reset();
	for (AActor* Boss : VisibleBosses)
	{
		Stored.Add(Boss);
	}
}

void UMALCombatEventsSubsystem::SyncAllControllers()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (AMALPlayerController* PlayerController = Cast<AMALPlayerController>(It->Get()))
		{
			SyncToController(PlayerController);
		}
	}
}

void UMALCombatEventsSubsystem::UnregisterBoss(AActor* Boss, bool bFadeOut)
{
	if (!Boss)
	{
		return;
	}

	const TWeakObjectPtr<AActor> Key(Boss);
	if (!KnownBosses.Contains(Key))
	{
		return;
	}

	KnownBosses.Remove(Key);

	if (IsValid(Boss))
	{
		Boss->OnDestroyed.RemoveDynamic(this, &UMALCombatEventsSubsystem::HandleBossDestroyed);
	}

	for (auto It = VisibleBossesByController.CreateIterator(); It; ++It)
	{
		TArray<TWeakObjectPtr<AActor>>& Visible = It.Value();
		if (Visible.Remove(Key) > 0)
		{
			if (AMALPlayerController* PlayerController = It.Key().Get())
			{
				PlayerController->ClientRemoveBossBar(Boss, bFadeOut);
			}
		}
	}

	UE_LOG(LogMALBossBar, Log, TEXT("remove %s fade=%d"), *GetNameSafe(Boss), bFadeOut ? 1 : 0);

	if (KnownBosses.Num() == 0)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(BossPollTimer);
		}

		BossPollTimer.Invalidate();
		VisibleBossesByController.Reset();
	}
}

float UMALCombatEventsSubsystem::GetBossBarRadius(const AActor* Boss) const
{
	// The bar radius lives on the Boss move set asset: per-Boss tuning without a component.
	const AMALEnemyCharacter* Enemy = Cast<AMALEnemyCharacter>(Boss);
	const UMALBossAttackSet* AttackSet = Enemy ? Enemy->BossAttackSet : nullptr;

	return AttackSet
		? FMath::Max(AttackSet->BossBarActivationRadius, 0.0f)
		: 1500.0f;
}

void UMALCombatEventsSubsystem::HandleBossDestroyed(AActor* DestroyedActor)
{
	if (!DestroyedActor)
	{
		return;
	}

	const TWeakObjectPtr<AActor> Key(DestroyedActor);
	if (!KnownBosses.Contains(Key))
	{
		return;
	}

	KnownBosses.Remove(Key);

	UE_LOG(LogMALBossBar, Log, TEXT("destroyed %s"), *GetNameSafe(DestroyedActor));

	// The actor is already going away, so a per-actor remove cannot be resolved reliably on
	// clients; refresh every list instead. The destroyed Boss is already absent from the new
	// lists, so its row disappears without the death fade.
	SyncAllControllers();
}