// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Framework/MALDamageNumberSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "UI/Widgets/MALDamageNumberWidget.h"

void UMALDamageNumberSubsystem::SetDamageNumberWidgetClass(TSubclassOf<UMALDamageNumberWidget> InWidgetClass)
{
	DamageNumberWidgetClass = InWidgetClass;
}

void UMALDamageNumberSubsystem::ShowDamageNumber(float Amount, bool bCritical, bool bMagic, bool bKillingBlow, FVector WorldLocation)
{
	if (Amount <= 0.0f)
	{
		return;
	}

	APlayerController* PlayerController = GetPlayerController();
	if (!PlayerController)
	{
		return;
	}

	UMALDamageNumberWidget* Widget = AcquireWidget(PlayerController);
	if (!Widget)
	{
		// No widget class configured. Silent by design: a prototype can run without the art.
		return;
	}

	// Rebind on every play. A pooled widget still holds the delegate from its previous owner, and
	// the weak lambda keeps a destroyed local player from being called back into.
	TWeakObjectPtr<UMALDamageNumberWidget> WeakWidget = Widget;
	Widget->OnPlaybackFinished = FSimpleDelegate::CreateWeakLambda(this, [this, WeakWidget]()
	{
		RecycleWidget(WeakWidget.Get());
	});

	Widget->PlayDamageNumber(Amount, bCritical, bMagic, bKillingBlow, WorldLocation);
}

void UMALDamageNumberSubsystem::RecycleWidget(UMALDamageNumberWidget* Widget)
{
	if (!Widget)
	{
		return;
	}

	// Hiding removes it from the viewport, which also stops its tick.
	Widget->Hide();

	// Past the cap the widget falls out of the pool and is garbage collected with everything else
	// that becomes unreachable.
	if (WidgetPool.Num() < MaxPooledWidgets)
	{
		WidgetPool.Add(Widget);
	}
}

UMALDamageNumberWidget* UMALDamageNumberSubsystem::AcquireWidget(APlayerController* PlayerController)
{
	// LIFO: the most recently released widget is the one whose Slate resources are still warm,
	// and it keeps a burst of simultaneous hits from walking the entire pool.
	while (WidgetPool.Num() > 0)
	{
		UMALDamageNumberWidget* Widget = WidgetPool.Pop();
		if (IsValid(Widget))
		{
			return Widget;
		}
	}

	if (!DamageNumberWidgetClass)
	{
		return nullptr;
	}

	return CreateWidget<UMALDamageNumberWidget>(PlayerController, DamageNumberWidgetClass);
}

APlayerController* UMALDamageNumberSubsystem::GetPlayerController() const
{
	if (const ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		return LocalPlayer->GetPlayerController(GetWorld());
	}

	return nullptr;
}

void UMALDamageNumberSubsystem::Deinitialize()
{
	// A LocalPlayer can be torn down without its widgets ever receiving NativeDestruct, so detach
	// them explicitly instead of relying on widget lifetime.
	for (const TObjectPtr<UMALDamageNumberWidget>& Widget : WidgetPool)
	{
		if (Widget)
		{
			Widget->RemoveFromParent();
		}
	}
	WidgetPool.Reset();

	Super::Deinitialize();
}
