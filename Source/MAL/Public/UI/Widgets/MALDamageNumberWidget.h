// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/MALUserWidget.h"
#include "MALDamageNumberWidget.generated.h"

class UMALDamageNumberViewModel;

/**
 * UMALDamageNumberWidget
 *
 * One floating damage number, positioned by projecting a world location onto the viewport.
 *
 * Screen space rather than a world-space widget component on purpose: a number is a UI overlay,
 * it should not be occluded by geometry or inherited the actor's scale, and an overlay can be
 * pooled and recycled by UMALDamageNumberSubsystem without spawning actors.
 *
 * The widget drives the motion that every game wants out of the box (rise, fade, perspective
 * scale). Anything project-specific goes in OnDamageNumberPlayed, so a blueprint never has to
 * reimplement the lifetime bookkeeping.
 *
 * Create a blueprint child (e.g. WBP_DamageNumber) whose View Binding panel declares a
 * MALDamageNumberViewModel source, then assign it to the player controller's
 * DamageNumberWidgetClass.
 */
UCLASS(Abstract)
class MAL_API UMALDamageNumberWidget : public UMALUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * Show one hit. Called by the subsystem when the widget leaves the pool; the widget owns
	 * nothing here, it only reads the values it is handed.
	 */
	void PlayDamageNumber(float InAmount, bool bInCritical, bool bInMagic, bool bInKillingBlow, const FVector& InWorldLocation);

	UFUNCTION(BlueprintPure, Category = "MAL|UI|Damage")
	UMALDamageNumberViewModel* GetDamageNumberViewModel() const { return ViewModel; }

	/** Broadcast once the lifetime elapsed. The subsystem uses it to recycle the widget. */
	FSimpleDelegate OnPlaybackFinished;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/**
	 * Optional source name to target precisely. Leave empty (the default) to match the source by
	 * type, which is what you want unless the widget declares several sources of the same type.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|MVVM")
	FName ViewModelName = NAME_None;

	/** Total seconds the number stays on screen before it is recycled. */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Damage", meta = (ClampMin = "0.05"))
	float Lifetime = 0.85f;

	/** Seconds the number stays fully opaque before it starts fading out. */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Damage", meta = (ClampMin = "0.0"))
	float HoldTime = 0.2f;

	/** Upward drift in world units per second; it slows down as the number ages. */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Damage")
	float RiseSpeed = 90.0f;

	/**
	 * Random horizontal jitter in viewport units so simultaneous hits on one target stay legible.
	 * Deliberately small: this is shake around the impact, not a placement offset.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Damage", meta = (ClampMin = "0.0"))
	float HorizontalScatter = 8.0f;

	/**
	 * Size assumed when Slate has not measured the widget yet. A blueprint whose root is a
	 * CanvasPanel can report a desired size of zero, which would pin the number by its top-left
	 * corner instead of centring it on the hit.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Damage")
	FVector2D FallbackWidgetSize = FVector2D(90.0f, 40.0f);

	/** Camera distance at which the number renders at its authored size. */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Damage", meta = (ClampMin = "1.0"))
	float ReferenceDistance = 900.0f;

	/** Perspective scale limits, so a distant number stays readable and a close one stays sane. */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Damage", meta = (ClampMin = "0.1"))
	float MinDistanceScale = 0.55f;

	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Damage", meta = (ClampMin = "0.1"))
	float MaxDistanceScale = 1.6f;

	/** Must sit above the HUD and the health bars to never be covered. */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Damage")
	int32 ZOrder = 20;

	/** Hook for extra flourishes; rise, fade and distance scaling are already handled in C++. */
	UFUNCTION(BlueprintImplementableEvent, Category = "MAL|UI|Damage", meta = (DisplayName = "On Damage Number Played"))
	void OnDamageNumberPlayed();

private:
	/**
	 * Add the widget on its own player layer with a point anchor and centred alignment.
	 * The default viewport slot anchors to the full screen, which would stretch the blueprint's
	 * root over everything to the bottom-right of the hit.
	 */
	void AddToPlayerScreenCentered();

	/** Project WorldLocation onto the viewport and place the widget there. */
	void UpdateScreenPosition();

	/** Measured content size, falling back to the authored size before the first layout pass. */
	FVector2D MeasureWidgetSize() const;

	/** Push the recycled view model back into the source the blueprint declared. */
	void PushViewModel();

	float ComputeDistanceScale(float CameraDistance) const;

	UPROPERTY()
	TObjectPtr<UMALDamageNumberViewModel> ViewModel;

	FVector WorldLocation = FVector::ZeroVector;

	float ElapsedTime = 0.0f;
	float LateralOffset = 0.0f;

	bool bPlaying = false;

	/** Tracks the last visibility we applied so a steady number does not invalidate every frame. */
	bool bProjectedOnScreen = true;
};
