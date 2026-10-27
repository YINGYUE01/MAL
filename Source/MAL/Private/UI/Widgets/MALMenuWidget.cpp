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
	bClosing = false;

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

	if (OpenAnimation)
	{
		PlayAnimation(OpenAnimation, 0.f, 1, EUMGSequencePlayMode::Forward, 1.f);
	}
	else if (TransitionTime > 0.f)
	{
		// Start from nothing so the panel slides and fades in rather than appearing on the
		// first frame it is drawn.
		TransitionAlpha = 0.f;
		TargetAlpha = 1.f;
		ApplyTransition();
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
	if (!IsShown() || bClosing)
	{
		return;
	}

	OnMenuDeactivated();

	if (CloseAnimation)
	{
		// The menu has to outlive the animation, otherwise the panel disappears half way
		// through its own exit.
		bClosing = true;

		if (!bCloseFinishedBound)
		{
			CloseFinishedEvent.BindDynamic(this, &UMALMenuWidget::HandleCloseAnimationFinished);
			BindToAnimationFinished(CloseAnimation, CloseFinishedEvent);
			bCloseFinishedBound = true;
		}

		PlayAnimation(CloseAnimation, 0.f, 1, EUMGSequencePlayMode::Forward, 1.f);
		return;
	}

	if (TransitionTime > 0.f)
	{
		bClosing = true;
		TargetAlpha = 0.f;
		return;
	}

	Hide();
}

void UMALMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Whichever phase is running may be driven by a UMG animation instead; the built-in
	// transition then has to stay out of the way, or the two would fight over the same render
	// properties.
	const bool bDrivenByAnimation = bClosing ? (CloseAnimation != nullptr) : (OpenAnimation != nullptr);
	if (bDrivenByAnimation || FMath::IsNearlyEqual(TransitionAlpha, TargetAlpha))
	{
		return;
	}

	const float Speed = TransitionTime > 0.f ? 1.f / TransitionTime : 1000.f;
	TransitionAlpha = FMath::FInterpConstantTo(TransitionAlpha, TargetAlpha, InDeltaTime, Speed);
	ApplyTransition();

	if (bClosing && FMath::IsNearlyEqual(TransitionAlpha, TargetAlpha))
	{
		bClosing = false;
		Hide();
	}
}

void UMALMenuWidget::ApplyTransition()
{
	// Eased rather than linear: a side panel that stops dead reads as a glitch, and the fade
	// and the slide then land together.
	const float Eased = FMath::InterpEaseOut(0.f, 1.f, TransitionAlpha, 2.f);

	SetRenderOpacity(Eased);
	SetRenderTranslation(FVector2D((1.f - Eased) * -TransitionSlideDistance, 0.f));
}

void UMALMenuWidget::HandleCloseAnimationFinished()
{
	bClosing = false;
	Hide();
}
