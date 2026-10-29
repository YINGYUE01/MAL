// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "UI/Framework/MALRetryPoll.h"
#include "MALWorldWidgetComponent.generated.h"

class UMALStatViewModel;
class UAbilitySystemComponent;

/**
 * UMALWorldWidgetComponent
 *
 * World-space widget that shows an actor's vitals: enemy nameplates today, world markers later.
 *
 * It owns its own view model. UMALStatViewModel is ASC-agnostic, so the same class that drives
 * the player HUD drives this component by pointing it at the owning actor's ASC instead. Nothing
 * in the widget layer knows whether it is showing a player or an enemy.
 *
 * Set WidgetClass to a UMALUserWidget blueprint whose View Binding panel declares a
 * MALStatViewModel source. On a dedicated server the engine skips widget creation entirely, so
 * the binding work silently does nothing there.
 */
UCLASS(ClassGroup = (MAL), meta = (BlueprintSpawnableComponent))
class MAL_API UMALWorldWidgetComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	UMALWorldWidgetComponent();

	UFUNCTION(BlueprintPure, Category = "MAL|UI")
	UMALStatViewModel* GetStatViewModel() const { return StatViewModel; }

	/** Bind to a specific ASC instead of the owning actor's (pets, summons, spectator targets). */
	UFUNCTION(BlueprintCallable, Category = "MAL|UI")
	void SetAbilitySystemComponent(UAbilitySystemComponent* InAbilitySystemComponent);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void InitWidget() override;

	/** Optional source name; empty matches the source by type. */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI")
	FName StatViewModelName = NAME_None;

	/** Retry interval while the owning actor's ASC is not available yet. */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI", meta = (ClampMin = "0.02"))
	float AbilitySystemBindRetryInterval = 0.1f;

	/**
	 * Fast retries to spend before falling back to AbilitySystemBindSlowRetryInterval.
	 * At the default 0.1s interval this covers the first three seconds after BeginPlay.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI", meta = (ClampMin = "1"))
	int32 AbilitySystemBindFastRetryCount = 30;

	/**
	 * Retry interval once the fast window has elapsed.
	 *
	 * Never stops: pooled enemies and summons can have their ASC granted long after the
	 * component starts. It only slows down, so a nameplate waiting on nothing costs ~1 Hz
	 * instead of 10 Hz.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI", meta = (ClampMin = "0.1"))
	float AbilitySystemBindSlowRetryInterval = 1.0f;

private:
	/** Bind to the owning actor's ASC, retrying until it exists. */
	void ResolveOwnerAbilitySystem();

	/** Hand the view model to the instantiated widget. Safe to call more than once. */
	void PushViewModelToWidget();

	UPROPERTY(Transient)
	TObjectPtr<UMALStatViewModel> StatViewModel;

	/** Poll that waits for the owning actor's ASC to appear. */
	FMALRetryPoll BindRetryPoll;
};
