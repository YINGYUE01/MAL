// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/ViewModel/MALStatViewModel.h"
#include "MALBossBarViewModel.generated.h"

/**
 * UMALBossBarViewModel
 *
 * Vitals view model for one top-screen Boss bar.
 *
 * Inherits the health/mana/stamina and trailing-bar presentation from UMALStatViewModel; the
 * only Boss-specific data is the display name. Splitting it this way keeps the health path
 * identical to the player HUD and enemy nameplates, so a fix to the trailing bar lands in one
 * place instead of three.
 */
UCLASS(BlueprintType)
class MAL_API UMALBossBarViewModel : public UMALStatViewModel
{
	GENERATED_BODY()

public:
	/** Name shown above the bar. Set before the widget binds, so the first frame is correct. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Boss")
	FText BossName;

	UFUNCTION(BlueprintCallable, Category = "MAL|UI|Boss")
	void SetBossName(const FText& InBossName);
};