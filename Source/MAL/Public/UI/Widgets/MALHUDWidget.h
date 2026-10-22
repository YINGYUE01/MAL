// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "UI/Widgets/MALUserWidget.h"
#include "MALHUDWidget.generated.h"

class UMALStatViewModel;
class APlayerController;

/**
 * UMALHUDWidget
 *
 * Persistent in-game HUD: vitals today, ability bar and minimap later.
 *
 * Owns its view model and pushes it into the widget blueprint's bindings, so the blueprint only
 * declares layout and bindings, never gameplay lookups. The ASC is resolved through the
 * PlayerState, which replicates after the HUD is created on a client, so binding is retried on
 * a short timer until it lands.
 */
UCLASS(Abstract)
class MAL_API UMALHUDWidget : public UMALUserWidget
{
	GENERATED_BODY()

public:
	/** Called by the UI subsystem once the widget is in the viewport. */
	void InitializeForPlayer(APlayerController* InPlayerController);

	UFUNCTION(BlueprintPure, Category = "MAL|UI")
	UMALStatViewModel* GetStatViewModel() const { return StatViewModel; }

protected:
	/**
	 * Optional source name to target precisely. Leave empty (the default) to match the source by
	 * type instead, which is what you want unless the widget declares several sources of the same
	 * type. A name typed in the View Binding panel is easy to get wrong and fails as a silent
	 * runtime no-op.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|MVVM")
	FName StatViewModelName = NAME_None;

	/** Retry interval while the PlayerState (and with it the ASC) is still replicating in. */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI", meta = (ClampMin = "0.02"))
	float AbilitySystemBindRetryInterval = 0.1f;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|UI")
	TObjectPtr<UMALStatViewModel> StatViewModel;

	virtual void NativeDestruct() override;

private:
	void TryBindAbilitySystem();
	void StopRetryTimer();

	TWeakObjectPtr<APlayerController> OwningPlayerController;
	FTimerHandle BindRetryTimer;
};
