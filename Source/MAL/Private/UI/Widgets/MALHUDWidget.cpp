// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Widgets/MALHUDWidget.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Tags/MALGameplayTags.h"
#include "TimerManager.h"
#include "UI/ViewModel/MALAbilityBarViewModel.h"
#include "UI/ViewModel/MALProgressionViewModel.h"
#include "UI/ViewModel/MALStatViewModel.h"

namespace
{
	/** Default bar layout, used when the blueprint leaves AbilitySlotTags empty. */
	void AppendDefaultSlotTags(TArray<FGameplayTag>& OutTags)
	{
		const UMALGameplayTags* Tags = UMALGameplayTags::Get();
		OutTags.Add(Tags->Input_LMB);
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

	if (!ProgressionViewModel)
	{
		ProgressionViewModel = NewObject<UMALProgressionViewModel>(this);
	}

	if (ProgressionViewModelName.IsNone())
	{
		SetViewModelByClass(ProgressionViewModel);
	}
	else
	{
		SetViewModelByName(ProgressionViewModelName, ProgressionViewModel);
	}

	PublishProgressionToNestedWidgets();

	TryBindAbilitySystem();
}

void UMALHUDWidget::PublishProgressionToNestedWidgets()
{
	if (!WidgetTree)
	{
		return;
	}

	// Show() runs before InitializeForPlayer and is what realises the widget tree, so a nested
	// widget builds its view while this HUD's sources are still empty. A child that names the
	// progression source as a parent-context reference resolves that link against an empty
	// parent and never re-resolves, leaving its bindings pushing nothing. Publishing the view
	// model straight into the child gives it a source to read; a child without a source of this
	// type rejects the call and keeps its own assignment.
	TArray<UWidget*> NestedWidgets;
	WidgetTree->GetAllWidgets(NestedWidgets);

	for (UWidget* NestedWidget : NestedWidgets)
	{
		UMALUserWidget* MALWidget = Cast<UMALUserWidget>(NestedWidget);
		if (!MALWidget || MALWidget == this)
		{
			continue;
		}

		if (MALWidget->SetViewModelByClass(ProgressionViewModel))
		{
			UE_LOG(LogTemp, Log, TEXT("UMALHUDWidget: published the progression view model to %s."),
				*GetNameSafe(MALWidget));
		}
	}
}

void UMALHUDWidget::PublishAbilitySystemToNestedWidgets(UAbilitySystemComponent* InAbilitySystemComponent)
{
	if (!WidgetTree || !InAbilitySystemComponent)
	{
		return;
	}

	// Same sweep as PublishProgressionToNestedWidgets, different payload: a nested widget that
	// follows attributes gets the ASC here instead of polling for it on its own.
	TArray<UWidget*> NestedWidgets;
	WidgetTree->GetAllWidgets(NestedWidgets);

	for (UWidget* NestedWidget : NestedWidgets)
	{
		UMALUserWidget* MALWidget = Cast<UMALUserWidget>(NestedWidget);
		if (!MALWidget || MALWidget == this)
		{
			continue;
		}

		MALWidget->NativeAbilitySystemReady(InAbilitySystemComponent);
	}
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
		ProgressionViewModel->SetAbilitySystemComponent(AbilitySystemComponent);
		PublishAbilitySystemToNestedWidgets(AbilitySystemComponent);
		BindRetryPoll.Reset(GetWorld());
		return;
	}

	// On a client the PlayerState replicates in after the HUD is created, so keep polling.
	// Past the fast window the poll drops to one attempt per second and never stops, so a later
	// possession can still bind.
	BindRetryPoll.OnMiss(GetWorld(),
		FTimerDelegate::CreateUObject(this, &UMALHUDWidget::TryBindAbilitySystem),
		AbilitySystemBindRetryInterval, AbilitySystemBindFastRetryCount, AbilitySystemBindSlowRetryInterval);
}

void UMALHUDWidget::NativeDestruct()
{
	BindRetryPoll.Reset(GetWorld());

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

	if (ProgressionViewModel)
	{
		ProgressionViewModel->SetAbilitySystemComponent(nullptr);
	}

	Super::NativeDestruct();
}
