// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Widgets/MALDamageNumberWidget.h"

#include "Blueprint/GameViewportSubsystem.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "UI/ViewModel/MALDamageNumberViewModel.h"

void UMALDamageNumberWidget::PlayDamageNumber(float InAmount, bool bInCritical, bool bInMagic, bool bInKillingBlow, const FVector& InWorldLocation)
{
	if (!ViewModel)
	{
		ViewModel = NewObject<UMALDamageNumberViewModel>(this);
	}

	// Adding to the player screen runs the construct path, which is what materializes the
	// compiled view bindings. Pushing the view model before that would be a silent no-op, so the
	// add comes first and NativeConstruct does the push.
	if (!IsInViewport())
	{
		AddToPlayerScreenCentered();
	}

	// Values are written after the bindings are live, so the first frame already shows this hit
	// rather than the previous one the pooled widget was carrying. The palette goes first: SetDamage
	// resolves the colour against it, and a pooled widget may have outlived a blueprint change.
	ViewModel->SetColorPalette(Colors);
	ViewModel->SetDamage(InAmount, bInCritical, bInMagic, bInKillingBlow);

	bShowAsCritical = bInCritical;

	WorldLocation = InWorldLocation;
	ElapsedTime = 0.0f;

	// Criticals spread wider: they usually land right after the fast normal hits that preceded
	// them, and sharing their exact jitter would pile the numbers into one illegible blob.
	const float Scatter = bInCritical ? HorizontalScatter * CriticalScatterScale : HorizontalScatter;
	LateralOffset = FMath::FRandRange(-Scatter, Scatter);
	bPlaying = true;

	SetRenderOpacity(1.0f);
	SetRenderScale(FVector2D(1.0));

	// HitTestInvisible forces the whole subtree to ignore the cursor: a number drifting over the
	// screen must never swallow a click meant for a menu underneath.
	SetVisibility(ESlateVisibility::HitTestInvisible);
	bProjectedOnScreen = true;

	UpdateScreenPosition();

	OnDamageNumberPlayed();
}

void UMALDamageNumberWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Runs on every re-entry into the viewport, which is exactly what a pooled widget needs: the
	// bindings are rebuilt here and the source is empty again.
	PushViewModel();
}

void UMALDamageNumberWidget::PushViewModel()
{
	if (!ViewModel)
	{
		return;
	}

	if (ViewModelName.IsNone())
	{
		SetViewModelByClass(ViewModel);
	}
	else
	{
		SetViewModelByName(ViewModelName, ViewModel);
	}
}

void UMALDamageNumberWidget::AddToPlayerScreenCentered()
{
	UGameViewportSubsystem* Subsystem = UGameViewportSubsystem::Get();
	ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	if (!Subsystem || !LocalPlayer)
	{
		return;
	}

	// AddToPlayerScreen leaves Anchors at the struct default, which is (0,0,1,1): the blueprint's
	// root would be stretched across everything from the hit to the bottom-right corner. A point
	// anchor with a centred alignment is what a floating number needs.
	FGameViewportWidgetSlot ViewportSlot;
	ViewportSlot.ZOrder = ZOrder;
	ViewportSlot.Anchors = FAnchors(0.0f, 0.0f);
	ViewportSlot.Alignment = FVector2D(0.5f, 0.5f);

	Subsystem->AddWidgetForPlayer(this, LocalPlayer, ViewportSlot);
}

void UMALDamageNumberWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bPlaying)
	{
		return;
	}

	ElapsedTime += InDeltaTime;

	const float SafeLifetime = GetEffectiveLifetime();
	const float Age = FMath::Clamp(ElapsedTime / SafeLifetime, 0.0f, 1.0f);

	// Decelerating drift: it leaps out of the hit and settles while fading. Linear motion reads
	// as text mechanically sliding upwards.
	const float EffectiveRise = bShowAsCritical ? RiseSpeed * CriticalRiseScale : RiseSpeed;
	WorldLocation.Z += EffectiveRise * InDeltaTime * (1.0f - Age);

	// Hold at full opacity first, then fade over the remainder of the lifetime.
	const float FadeStart = FMath::Clamp(HoldTime / SafeLifetime, 0.0f, 1.0f);
	SetRenderOpacity(1.0f - FMath::SmoothStep(FadeStart, 1.0f, Age));

	UpdateScreenPosition();

	if (Age >= 1.0f)
	{
		bPlaying = false;
		OnPlaybackFinished.ExecuteIfBound();
	}
}

void UMALDamageNumberWidget::UpdateScreenPosition()
{
	const APlayerController* PlayerController = GetOwningPlayer();
	if (!PlayerController)
	{
		return;
	}

	FVector Projected;
	// WithDistance also yields the camera distance in Z, which drives the perspective scale.
	if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPositionWithDistance(
		PlayerController, WorldLocation, Projected, /*bPlayerViewportRelative*/ false))
	{
		// Behind the camera. Hiding beats clamping the number to a screen edge, which would look
		// like damage coming from the wrong direction.
		if (bProjectedOnScreen)
		{
			SetVisibility(ESlateVisibility::Collapsed);
			bProjectedOnScreen = false;
		}
		return;
	}

	if (!bProjectedOnScreen)
	{
		SetVisibility(ESlateVisibility::HitTestInvisible);
		bProjectedOnScreen = true;
	}

	UGameViewportSubsystem* Subsystem = UGameViewportSubsystem::Get();
	if (!Subsystem)
	{
		return;
	}

	FGameViewportWidgetSlot ViewportSlot = Subsystem->GetWidgetSlot(this);

	// The layout size has to be handed to the slot explicitly. With a zero anchor range the
	// canvas derives the widget size from Offsets.Right/Bottom, and a zero size collapses the
	// centred alignment back into a top-left placement.
	ViewportSlot = UGameViewportSubsystem::SetWidgetSlotDesiredSize(ViewportSlot, MeasureWidgetSize());

	// Centred alignment puts the middle of the widget on the projected point. Without it the
	// number starts at the hit and grows rightwards, which reads as "offset to the right".
	ViewportSlot.Alignment = FVector2D(0.5f, 0.5f);

	// The crit shake rides on top of the scatter, so a critical both lands further aside and
	// trembles at its own position instead of dragging its neighbours along.
	const float ShakeOffset = ComputeCriticalShake();

	// bRemoveDPIScale must stay false: Projected is already in viewport units. Asking the helper
	// to strip the DPI scale again would divide it a second time and shift the number sideways
	// by an amount that grows with the viewport size.
	ViewportSlot = UGameViewportSubsystem::SetWidgetSlotPosition(ViewportSlot, this,
		FVector2D(Projected.X + LateralOffset + ShakeOffset, Projected.Y), /*bRemoveDPIScale*/ false);

	Subsystem->SetWidgetSlot(this, ViewportSlot);

	SetRenderScale(FVector2D(ComputeRenderScale(Projected.Z)));
}

FVector2D UMALDamageNumberWidget::MeasureWidgetSize() const
{
	// Desired size only becomes valid after a layout pass, and a blueprint whose root is a
	// CanvasPanel may never report one at all. Both cases would leave the number pinned by its
	// top-left corner instead of centred.
	const FVector2D DesiredSize = GetDesiredSize();
	if (DesiredSize.X > 1.0f && DesiredSize.Y > 1.0f)
	{
		return DesiredSize;
	}

	const FVector2D CachedSize = GetCachedGeometry().GetLocalSize();
	if (CachedSize.X > 1.0f && CachedSize.Y > 1.0f)
	{
		return CachedSize;
	}

	return FallbackWidgetSize;
}

float UMALDamageNumberWidget::ComputeDistanceScale(float CameraDistance) const
{
	const float Scale = ReferenceDistance / FMath::Max(CameraDistance, 1.0f);
	return FMath::Clamp(Scale, MinDistanceScale, MaxDistanceScale);
}

float UMALDamageNumberWidget::ComputeRenderScale(float CameraDistance) const
{
	float Scale = ComputeDistanceScale(CameraDistance);

	if (bShowAsCritical)
	{
		// The rest scale holds for the whole life and the burst decays on top of it, so the number
		// never shrinks back to normal size halfway through its own animation.
		Scale *= CriticalRestScale * ComputeCriticalPopScale();
	}

	return Scale;
}

float UMALDamageNumberWidget::ComputeCriticalPopScale() const
{
	if (!bShowAsCritical || CriticalPopScale <= 1.0f)
	{
		return 1.0f;
	}

	const float Window = FMath::Max(CriticalPopDuration, KINDA_SMALL_NUMBER);
	const float Progress = FMath::Clamp(ElapsedTime / Window, 0.0f, 1.0f);
	if (Progress >= 1.0f)
	{
		return 1.0f;
	}

	// Cubic falloff: oversized for a couple of frames and then a hard snap back. A linear ramp
	// reads as a deliberate zoom rather than an impact.
	const float Remaining = 1.0f - Progress;
	return FMath::Lerp(1.0f, CriticalPopScale, Remaining * Remaining * Remaining);
}

float UMALDamageNumberWidget::ComputeCriticalShake() const
{
	if (!bShowAsCritical || CriticalShakeAmplitude <= 0.0f || CriticalShakeFrequency <= 0.0f)
	{
		return 0.0f;
	}

	// Shake lives inside the burst window only. A number that keeps vibrating for its whole life
	// reads as broken text instead of a hit.
	const float Window = FMath::Max(CriticalPopDuration, KINDA_SMALL_NUMBER);
	const float Progress = FMath::Clamp(ElapsedTime / Window, 0.0f, 1.0f);
	if (Progress >= 1.0f)
	{
		return 0.0f;
	}

	// Same cubic decay as the burst: position and scale settle together, which is what sells the
	// punch. A sine that outlives the scale change looks like a rendering glitch.
	const float Remaining = 1.0f - Progress;
	const float Phase = ElapsedTime * CriticalShakeFrequency * 2.0f * PI;
	return FMath::Sin(Phase) * CriticalShakeAmplitude * (Remaining * Remaining * Remaining);
}

float UMALDamageNumberWidget::GetEffectiveLifetime() const
{
	const float Base = FMath::Max(Lifetime, 0.05f);
	return bShowAsCritical ? Base * CriticalLifetimeScale : Base;
}
