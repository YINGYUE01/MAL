// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameAbilities/MALHitCameraShake.h"
#include "Shakes/PerlinNoiseCameraShakePattern.h"

UMALHitCameraShake::UMALHitCameraShake(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UPerlinNoiseCameraShakePattern* Pattern =
		CreateDefaultSubobject<UPerlinNoiseCameraShakePattern>(TEXT("HitShakePattern"));

	// Short, punchy hit shake.
	Pattern->Duration = 0.18f;
	Pattern->BlendInTime = 0.02f;
	Pattern->BlendOutTime = 0.08f;

	Pattern->RotationAmplitudeMultiplier = 1.0f;
	Pattern->Pitch.Amplitude = 1.2f;
	Pattern->Pitch.Frequency = 18.0f;
	Pattern->Yaw.Amplitude = 0.8f;
	Pattern->Yaw.Frequency = 14.0f;
	Pattern->Roll.Amplitude = 0.0f;

	Pattern->LocationAmplitudeMultiplier = 1.0f;
	Pattern->Z.Amplitude = 1.5f;
	Pattern->Z.Frequency = 22.0f;
	Pattern->X.Amplitude = 0.0f;
	Pattern->Y.Amplitude = 0.0f;
	Pattern->FOV.Amplitude = 0.0f;

	SetRootShakePattern(Pattern);
}
