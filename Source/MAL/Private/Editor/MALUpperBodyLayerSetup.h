// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "MALUpperBodyLayerSetup.generated.h"

/**
 * Splits the bow shot into an upper-body montage over a locomotion-only lower body.
 *
 * The shot montage used to animate the whole character, so the legs stopped running for as long
 * as the draw lasted. The host graph now blends the spine and everything above it from a second
 * slot, and the shot montage plays in that slot instead of the full-body one. Melee montages keep
 * DefaultSlot, which still drives the whole pose, so nothing about the sword changes.
 *
 * The lower half comes from ABP_Locomotion_Bow, which only had forward-facing sequences while the
 * aim stance strafes. The bow layer therefore branches on the aiming stance: while aiming it plays
 * a speed/direction blend space built from the in-place archer set, and the rest of the time it
 * plays the forward sequences it always did, because carrying a bow is not shooting.
 *
 * Kept as a commandlet, like the locomotion layer setup, because anim graph pins are not UObjects
 * and cannot be authored from Python:
 *
 *  -run=MALUpperBodyLayerSetup -Mode=dump     prints the host graph pins, the shot montage's slot
 *                                             tracks and the bow layer graphs
 *  -run=MALUpperBodyLayerSetup -Mode=apply    inserts UpperBodySlot plus a Layered Blend Per Bone
 *                                             on spine_01, moves the shot montage onto that slot,
 *                                             builds BS_Bow_Move and puts it behind a bIsAiming
 *                                             branch in every bow layer
 *  -run=MALUpperBodyLayerSetup -Mode=compile  recompiles every asset it touches, nothing else
 *
 * Optional switches for bisecting a failure: -SkipHostBlend, -SkipMontage, -SkipBowLayer,
 * -SkipCompile.
 */
UCLASS()
class UMALUpperBodyLayerSetupCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UMALUpperBodyLayerSetupCommandlet();

	virtual int32 Main(const FString& Params) override;
};
