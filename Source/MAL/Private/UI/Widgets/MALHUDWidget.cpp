// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Widgets/MALHUDWidget.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Tags/MALGameplayTags.h"
#include "TimerManager.h"
#include "UI/ViewModel/MALAbilityBarViewModel.h"
#include "UI/ViewModel/MALStatViewModel.h"

namespace
{
	/** Default bar layout, used when the blueprint leaves AbilitySlotTags empty. */
	void AppendDefaultSlotTags(TArray<FGameplayTag>& OutTags)
	{
		const UMALGameplayTags* Tags = UMALGameplayTags::Get();
		OutTags.Add(Tags->Input_Attack);
		OutTags.Add(Tags->Input_Buff);
		OutTags.Add(Tags->Input_Defend);
		OutTags.Add(Tags->Input_Dash);
	}
}

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

	if (!AbilityBarViewModel)
	{
		AbilityBarViewModel = NewObject<UMALAbilityBarViewModel>(this);

		TArray<FGameplayTag> SlotTags = AbilitySlotTags;
		if (SlotTags.Num() == 0)
		{
			AppendDefaultSlotTags(SlotTags);
		}

		// Built once: rebuilding on every InitializeForPlayer would reallocate the slots the
		// widget blueprint is already bound to.
		AbilityBarViewModel->InitializeSlots(SlotTags);
	}

	if (AbilityBarViewModelName.IsNone())
	{
		SetViewModelByClass(AbilityBarViewModel);
	}
	else
	{
		SetViewModelByName(AbilityBarViewModelName, AbilityBarViewModel);
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
		AbilityBarViewModel->SetAbilitySystemComponent(AbilitySystemComponent);
		StopRetryTimer();
		return;
	}

	if (!BindRetryTimer.IsValid())
	{
		// First miss: arm the fast poll and start counting from zero.
		BindRetryCount = 0;
		ArmRetryTimer(AbilitySystemBindRetryInterval);
		return;
	}

	// On a client the PlayerState replicates in after the HUD is created, so keep polling
	// briefly instead of leaving the HUD permanently blank. Past the fast window the poll drops
	// to one attempt per second; it is never cancelled, so a later possession can still bind.
	if (++BindRetryCount == FMath::Max(AbilitySystemBindFastRetryCount, 1))
	{
		StopRetryTimer();
		ArmRetryTimer(AbilitySystemBindSlowRetryInterval);
	}
}

void UMALHUDWidget::ArmRetryTimer(float Interval)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		BindRetryTimer,
		this,
		&UMALHUDWidget::TryBindAbilitySystem,
		FMath::Max(Interval, 0.02f),
		true);
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

	if (AbilityBarViewModel)
	{
		AbilityBarViewModel->SetAbilitySystemComponent(nullptr);
	}

	Super::NativeDestruct();
}
