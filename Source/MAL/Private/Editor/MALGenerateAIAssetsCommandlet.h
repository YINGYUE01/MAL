// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "MALGenerateAIAssetsCommandlet.generated.h"

#if WITH_EDITOR

/**
 * One-shot editor commandlet that writes the Stone Golem Blackboard and Behavior Tree
 * assets. Run with:
 *   UnrealEditor-Cmd.exe <project> -run=MALGenerateAIAssets
 */
UCLASS()
class UMALGenerateAIAssetsCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UMALGenerateAIAssetsCommandlet();

	virtual int32 Main(const FString& Params) override;
};

#endif // WITH_EDITOR
