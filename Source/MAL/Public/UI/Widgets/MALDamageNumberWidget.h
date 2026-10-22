// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/MALDamageNumberTypes.h"
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

	/**
	 * True while the number being played came from a critical hit.
	 *
	 * Exposed so OnDamageNumberPlayed can add its own flourishes (crit sting, HUD particles)
	 * without having to re-derive the flag from the view model.
	 */
	UFUNCTION(BlueprintPure, Category = "MAL|UI|Damage")
	bool IsCriticalHit() const { return bShowAsCritical; }

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

	/**
	 * Colour per hit type. Authored on the view so re-skinning the numbers is a blueprint edit;
	 * the view model turns the hit's flags into one of these entries.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Damage|Color")
	FMALDamageNumberColors Colors;

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

	/**
	 * Resting size of a critical number relative to a normal one. Above 1 so a crit stays
	 * recognisable after the burst has settled, not just during the first few frames.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Damage|Critical", meta = (ClampMin = "1.0"))
	float CriticalRestScale = 1.35f;

	/** Size a critical bursts to on its first frame, before settling to CriticalRestScale. */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Damage|Critical", meta = (ClampMin = "1.0"))
	float CriticalPopScale = 1.95f;

	/**
	 * Seconds the burst takes to settle. Short on purpose: a slow zoom reads as a UI bug, while a
	 * three or four frame snap reads as impact.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Damage|Critical", meta = (ClampMin = "0.01"))
	float CriticalPopDuration = 0.16f;

	/** Horizontal shake amplitude in viewport units. Decays to zero over CriticalPopDuration. */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Damage|Critical", meta = (ClampMin = "0.0"))
	float CriticalShakeAmplitude = 6.0f;

	/** Shake cycles per second. High enough to read as a punch, not as a wobble. */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Damage|Critical", meta = (ClampMin = "0.0"))
	float CriticalShakeFrequency = 18.0f;

	/**
	 * Scatter multiplier for criticals. A crit usually lands right after the fast normal hits that
	 * preceded it, so it is pushed further aside instead of piling into the same spot.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Damage|Critical", meta = (ClampMin = "1.0"))
	float CriticalScatterScale = 1.9f;

	/** Rise speed multiplier, so a crit launches harder before settling on the same curve. */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Damage|Critical", meta = (ClampMin = "1.0"))
	float CriticalRiseScale = 1.4f;

	/** Lifetime multiplier, so the loudest number also lingers a beat longer. */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Damage|Critical", meta = (ClampMin = "1.0"))
	float CriticalLifetimeScale = 1.2f;

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

	/** Distance scale with the critical rest scale and the burst curve folded in. */
	float ComputeRenderScale(float CameraDistance) const;

	/** 1 for a normal hit; bursts to CriticalPopScale and snaps back for a critical. */
	float ComputeCriticalPopScale() const;

	/** Horizontal shake in viewport units; zero once the burst window elapsed. */
	float ComputeCriticalShake() const;

	/** Lifetime with the critical multiplier applied. */
	float GetEffectiveLifetime() const;

	UPROPERTY()
	TObjectPtr<UMALDamageNumberViewModel> ViewModel;

	FVector WorldLocation = FVector::ZeroVector;

	float ElapsedTime = 0.0f;
	float LateralOffset = 0.0f;

	bool bPlaying = false;

	/** Set per play from the hit's crit flag; drives every critical-only flourish below. */
	bool bShowAsCritical = false;

	/** Tracks the last visibility we applied so a steady number does not invalidate every frame. */
	bool bProjectedOnScreen = true;
};
