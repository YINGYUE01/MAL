// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MALGameMode.generated.h"

UCLASS()
class MAL_API AMALGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMALGameMode();

protected:
	virtual void BeginPlay() override;
};
