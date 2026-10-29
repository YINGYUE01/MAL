// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Framework/MALRetryPoll.h"

#include "Engine/World.h"

void FMALRetryPoll::OnMiss(UWorld* World, const FTimerDelegate& Tick, float FastInterval, int32 FastCount, float SlowInterval)
{
	if (!World)
	{
		return;
	}

	FTimerManager& TimerManager = World->GetTimerManager();

	if (!Handle.IsValid())
	{
		// First miss: arm the fast poll and start counting from zero.
		AttemptCount = 0;
		TimerManager.SetTimer(Handle, Tick, FMath::Max(FastInterval, 0.02f), true);
		return;
	}

	// Past the fast window the thing being waited on is not merely late, so ease off rather than
	// keeping a 10 Hz poll alive for the lifetime of the owner.
	if (++AttemptCount == FMath::Max(FastCount, 1))
	{
		TimerManager.ClearTimer(Handle);
		Handle.Invalidate();
		TimerManager.SetTimer(Handle, Tick, FMath::Max(SlowInterval, 0.02f), true);
	}
}

void FMALRetryPoll::Reset(UWorld* World)
{
	if (Handle.IsValid())
	{
		if (World)
		{
			World->GetTimerManager().ClearTimer(Handle);
		}
		Handle.Invalidate();
	}

	AttemptCount = 0;
}
