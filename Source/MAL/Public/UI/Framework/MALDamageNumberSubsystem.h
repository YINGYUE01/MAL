// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "MALDamageNumberSubsystem.generated.h"

class UMALDamageNumberWidget;
class APlayerController;

/**
 * UMALDamageNumberSubsystem
 *
 * Per-local-player owner of the floating damage numbers.
 *
 * Its only job is lifetime: take a widget from the pool, hand it one hit, take it back when the
 * animation ends. The numbers themselves are transient and can be several per second, so widgets
 * are recycled instead of recreated - a fresh CreateWidget per hit would wake the garbage
 * collector in the middle of combat.
 *
 * Damage numbers are driven from the server (UMALCombatEventsSubsystem routes them through
 * AMALPlayerController::ClientShowDamageNumber), so a dedicated server never reaches this class:
 * without a LocalPlayer the subsystem is never instantiated.
 */
UCLASS()
class MAL_API UMALDamageNumberSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	/** Widget class spawned for each number. Set once by the player controller on begin play. */
	void SetDamageNumberWidgetClass(TSubclassOf<UMALDamageNumberWidget> InWidgetClass);

	/** Show one floating number at a world location. */
	UFUNCTION(BlueprintCallable, Category = "MAL|UI|Damage")
	void ShowDamageNumber(float Amount, bool bCritical, bool bMagic, bool bKillingBlow, FVector WorldLocation);

	/** Return a finished widget to the pool. Bound to each widget's OnPlaybackFinished. */
	void RecycleWidget(UMALDamageNumberWidget* Widget);

protected:
	virtual void Deinitialize() override;

private:
	APlayerController* GetPlayerController() const;

	/** Take a widget from the pool, or create one when the pool is empty. */
	UMALDamageNumberWidget* AcquireWidget(APlayerController* PlayerController);

	UPROPERTY()
	TSubclassOf<UMALDamageNumberWidget> DamageNumberWidgetClass;

	/** Widgets that finished playing, kept warm for the next hit. */
	UPROPERTY()
	TArray<TObjectPtr<UMALDamageNumberWidget>> WidgetPool;

	/**
	 * Upper bound on retained widgets. A burst of simultaneous hits can push past this, and the
	 * excess is simply garbage collected rather than kept alive forever.
	 */
	int32 MaxPooledWidgets = 24;
};
