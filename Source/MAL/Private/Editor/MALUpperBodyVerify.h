// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "MALUpperBodyVerify.generated.h"

/**
 * Proves the bow shot's upper/lower body split without a viewport.
 *
 * PIE cannot be driven from a commandlet, and -nullrhi never evaluates an anim graph on its own,
 * so this runs the three poses that settle the question by hand:
 *
 *  A. graph, no montage        - the locomotion pose (legs at rest)
 *  B. graph, shot montage      - what the game actually plays
 *  C. the montage's own source sequence, played raw - full body, no splitting
 *
 * With the split in place, B's legs match A (the montage no longer owns them) while its spine
 * follows C, and C's legs differ from A (so the montage does carry leg motion and the comparison
 * is not vacuous). Any of those three failing is a real defect, not a tuning difference.
 *
 *  -run=MALUpperBodyVerify            runs the comparison
 *  -run=MALUpperBodyVerify -Frames=40 raises the number of sampled pose ticks
 *
 * Exits with 1 when the split does not hold, so a build script can gate on it.
 */
UCLASS()
class UMALUpperBodyVerifyCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UMALUpperBodyVerifyCommandlet();

	virtual int32 Main(const FString& Params) override;
};
