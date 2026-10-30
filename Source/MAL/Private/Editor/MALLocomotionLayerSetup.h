// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "MALLocomotionLayerSetup.generated.h"

/**
 * Builds the weapon locomotion layer assets described by the Linked Anim Layer plan.
 *
 * The anim graphs cannot be authored from Python (UEdGraphPin is not a UObject, so pins and
 * links are invisible to the scripting layer), so this commandlet drives the editor graph API
 * directly:
 *
 *  -run=MALLocomotionLayerSetup -Mode=dump     prints the host animation graph so the state names
 *                                              and player nodes can be inspected before touching them.
 *  -run=MALLocomotionLayerSetup -Mode=apply    creates ABP_Locomotion_*, wires the layer function
 *                                              graphs, replaces the ground state players with layer
 *                                              nodes, inserts the Inertialization node and points the
 *                                              weapon definitions at the layer.
 *  -run=MALLocomotionLayerSetup -Mode=compile  recompiles every asset it touches, nothing else.
 */
UCLASS()
class UMALLocomotionLayerSetupCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UMALLocomotionLayerSetupCommandlet();

	virtual int32 Main(const FString& Params) override;
};
