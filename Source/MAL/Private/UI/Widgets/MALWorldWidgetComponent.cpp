// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Widgets/MALWorldWidgetComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UI/ViewModel/MALStatViewModel.h"
#include "UI/Widgets/MALUserWidget.h"

UMALWorldWidgetComponent::UMALWorldWidgetComponent()
{
	// Screen space keeps a nameplate facing the camera and legible at any angle. Switch to
	// EWidgetSpace::World if the bar should be occluded by geometry.
	SetWidgetSpace(EWidgetSpace::Screen);
	SetDrawSize(FVector2D(140.0f, 20.0f));

	// Bottom-centred pivot so the bar hangs above the component's location.
	SetPivot(FVector2D(0.5f, 1.0f));

	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
}

void UMALWorldWidgetComponent::InitWidget()
{
	Super::InitWidget();

	// The widget instance exists from here on; publish the view model so its bindings resolve on
	// construct instead of waiting for the first attribute change.
	if (!StatViewModel)
	{
		StatViewModel = NewObject<UMALStatViewModel>(this);
	}

	PushViewModelToWidget();
}

void UMALWorldWidgetComponent::BeginPlay()
{
	// Super calls InitWidget(), which creates the view model before this returns.
	Super::BeginPlay();

	ResolveOwnerAbilitySystem();
}

void UMALWorldWidgetComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	BindRetryPoll.Reset(GetWorld());

	// Unbind before the widget is released so the view model cannot push into a dead widget.
	if (StatViewModel)
	{
		StatViewModel->SetAbilitySystemComponent(nullptr);
	}

	Super::EndPlay(EndPlayReason);
}

void UMALWorldWidgetComponent::SetAbilitySystemComponent(UAbilitySystemComponent* InAbilitySystemComponent)
{
	if (!StatViewModel)
	{
		StatViewModel = NewObject<UMALStatViewModel>(this);
	}

	StatViewModel->SetAbilitySystemComponent(InAbilitySystemComponent);
	BindRetryPoll.Reset(GetWorld());
}

void UMALWorldWidgetComponent::ResolveOwnerAbilitySystem()
{
	if (!StatViewModel)
	{
		StatViewModel = NewObject<UMALStatViewModel>(this);
	}

	if (UAbilitySystemComponent* AbilitySystemComponent = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()))
	{
		StatViewModel->SetAbilitySystemComponent(AbilitySystemComponent);
		BindRetryPoll.Reset(GetWorld());
		return;
	}

	// Enemies own their ASC as a subobject so this normally succeeds on the first try; the retry
	// covers actors that grant it later (spawned summons, pooled enemies). Past the fast window
	// the poll drops to one attempt per second and never stops, so a pooled enemy that is
	// released and reused still gets picked up.
	BindRetryPoll.OnMiss(GetWorld(),
		FTimerDelegate::CreateUObject(this, &UMALWorldWidgetComponent::ResolveOwnerAbilitySystem),
		AbilitySystemBindRetryInterval, AbilitySystemBindFastRetryCount, AbilitySystemBindSlowRetryInterval);
}

void UMALWorldWidgetComponent::PushViewModelToWidget()
{
	UMALUserWidget* UserWidget = Cast<UMALUserWidget>(GetWidget());
	if (!UserWidget || !StatViewModel)
	{
		// Null on a dedicated server, where UWidgetComponent never creates a widget.
		return;
	}

	if (StatViewModelName.IsNone())
	{
		UserWidget->SetViewModelByClass(StatViewModel);
	}
	else
	{
		UserWidget->SetViewModelByName(StatViewModelName, StatViewModel);
	}
}
