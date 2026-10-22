// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Widgets/MALHUDWidget.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"
#include "UI/ViewModel/MALStatViewModel.h"

void UMALHUDWidget::InitializeForPlayer(APlayerController* InPlayerController)
{
	OwningPlayerController = InPlayerController;

	if (!StatViewModel)
	{
		StatViewModel = NewObject<UMALStatViewModel>(this);
	}

	// Publish the view model before the ASC exists: the widget's bindings then show the view
	// model's default values instead of logging a missing-source error on every construct.
	if (StatViewModelName.IsNone())
	{
		SetViewModelByClass(StatViewModel);
	}
	else
	{
		SetViewModelByName(StatViewModelName, StatViewModel);
	}

	TryBindAbilitySystem();
}

void UMALHUDWidget::TryBindAbilitySystem()
{
	UAbilitySystemComponent* AbilitySystemComponent = nullptr;

	if (APlayerController* PlayerController = OwningPlayerController.Get())
	{
		// The pawn's IAbilitySystemInterface already forwards to the PlayerState-owned ASC for
		// player characters, so this covers both ownership layouts.
		if (APawn* Pawn = PlayerController->GetPawn())
		{
			AbilitySystemComponent = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn);
		}

		// Fallback for the window where the pawn is not spawned yet but the PlayerState is.
		if (!AbilitySystemComponent)
		{
			if (APlayerState* PlayerState = PlayerController->PlayerState)
			{
				AbilitySystemComponent = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PlayerState);
			}
		}
	}

	if (AbilitySystemComponent)
	{
		StatViewModel->SetAbilitySystemComponent(AbilitySystemComponent);
		StopRetryTimer();
		return;
	}

	// On a client the PlayerState replicates in after the HUD is created, so keep polling
	// briefly instead of leaving the HUD permanently blank.
	if (!BindRetryTimer.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				BindRetryTimer,
				this,
				&UMALHUDWidget::TryBindAbilitySystem,
				FMath::Max(AbilitySystemBindRetryInterval, 0.02f),
				true);
		}
	}
}

void UMALHUDWidget::StopRetryTimer()
{
	if (BindRetryTimer.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(BindRetryTimer);
		}
		BindRetryTimer.Invalidate();
	}
}

void UMALHUDWidget::NativeDestruct()
{
	StopRetryTimer();

	// Unbind before the widget goes away so the view model cannot push into half-destroyed
	// widget bindings.
	if (StatViewModel)
	{
		StatViewModel->SetAbilitySystemComponent(nullptr);
	}

	Super::NativeDestruct();
}
