// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ViewModel/MALBossBarViewModel.h"

void UMALBossBarViewModel::SetBossName(const FText& InBossName)
{
	UE_MVVM_SET_PROPERTY_VALUE(BossName, InBossName);
}