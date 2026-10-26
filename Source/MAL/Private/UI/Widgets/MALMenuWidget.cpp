// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Widgets/MALMenuWidget.h"

#include "AbilitySystemComponent.h"
#include "Blueprint/GameViewportSubsystem.h"
#include "Player/MALPlayerState.h"

void UMALMenuWidget::ActivateMenu()
{
	if (IsShown())
	{
		return;
	}

	Show();

	// AddToViewport anchors a widget to all four edges, so it is laid out at full screen size and
	// its content ends up wherever the widget's own alignment puts it -- for a panel, the corner,
	// on top of the HUD's icons. Pinning only the horizontal edge and centring vertically is what
	// turns it into a side panel that keeps its designed size.
	if (UGameViewportSubsystem* ViewportSubsystem = UGameViewportSubsystem::Get())
	{
		const bool bAnchoredLeft = HorizontalAnchor < 0.5f;
		const bool bAnchoredRight = HorizontalAnchor > 0.5f;

		FGameViewportWidgetSlot ViewportSlot = ViewportSubsystem->GetWidgetSlot(this);
		ViewportSlot.Anchors = FAnchors(HorizontalAnchor, 0.5f);
		ViewportSlot.Alignment = FVector2D(HorizontalAnchor, 0.5f);
		ViewportSlot.Offsets = FMargin(bAnchoredLeft ? ScreenEdgeMargin : 0.f, 0.f,
			bAnchoredRight ? ScreenEdgeMargin : 0.f, 0.f);
		ViewportSubsystem->SetWidgetSlot(this, ViewportSlot);
	}

	// A menu is created outside the HUD's own tree, so the HUD never forwards the ability system
	// the way it does for its nested widgets. Resolving it here is enough: a menu is only opened
	// during gameplay, by which point the PlayerState has long since replicated in.
	if (AMALPlayerState* PlayerState = GetOwningPlayerState<AMALPlayerState>())
	{
		NativeAbilitySystemReady(PlayerState->GetAbilitySystemComponent());
	}

	OnMenuActivated();
}

void UMALMenuWidget::DeactivateMenu()
{
	if (!IsShown())
	{
		return;
	}

	OnMenuDeactivated();
	Hide();
}
