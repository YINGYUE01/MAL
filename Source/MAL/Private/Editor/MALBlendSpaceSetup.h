// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "MALBlendSpaceSetup.generated.h"

/**
 * Fills the locomotion blend space with the GreatSword 8-way jog clips.
 *
 * The scripting layer cannot author blend space samples in this engine version (UBlendSpace
 * exposes no AddSample to Python), so the editor C++ API drives it directly:
 *
 *  -run=MALBlendSpaceSetup -BlendSpace=/Game/.../BS_Locomotion -Speed=300
 *
 * Existing samples are cleared first, so re-running the commandlet can never stack duplicates.
 */
UCLASS()
class UMALBlendSpaceSetupCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UMALBlendSpaceSetupCommandlet();

	virtual int32 Main(const FString& Params) override;
};
