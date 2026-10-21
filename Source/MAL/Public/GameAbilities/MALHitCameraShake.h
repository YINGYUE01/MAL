// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"
#include "MALHitCameraShake.generated.h"

/**
 * Lightweight default hit camera shake.
 * Pitch/Yaw/Z perlin noise with a short duration; override HitCameraShakeClass on the
 * attack ability to use a bespoke camera shake asset instead.
 */
UCLASS()
class MAL_API UMALHitCameraShake : public UCameraShakeBase
{
	GENERATED_BODY()

public:
	UMALHitCameraShake(const FObjectInitializer& ObjectInitializer);
};
