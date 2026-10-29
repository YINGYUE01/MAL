// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TimerManager.h"

/**
 * FMALRetryPoll
 *
 * Two-stage polling for something that is not there yet: an ASC that replicates in after the
 * widget was created, an enemy whose abilities are granted on spawn.
 *
 * The first miss arms the fast interval and resets the counter; once FastCount fast attempts
 * have failed the poll drops to the slow interval and stays there. It never stops on its own,
 * because the owner may legitimately wait forever -- the HUD outlives possession changes, and a
 * pooled enemy is reused with a fresh ASC.
 *
 * Owns no UObject: the caller passes its own tick delegate on every call, so the timer manager
 * drops it when the owner is destroyed.
 */
struct MAL_API FMALRetryPoll
{
	/** Arm or advance the poll. Call once per failed attempt. */
	void OnMiss(UWorld* World, const FTimerDelegate& Tick, float FastInterval, int32 FastCount, float SlowInterval);

	/** Cancel the poll and reset the counter. Safe when nothing is armed. */
	void Reset(UWorld* World);

	bool IsActive() const { return Handle.IsValid(); }

private:
	FTimerHandle Handle;

	/** Misses counted since the fast interval was armed. */
	int32 AttemptCount = 0;
};
