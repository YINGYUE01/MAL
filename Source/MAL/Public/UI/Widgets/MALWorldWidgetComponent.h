// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
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

private:
	/** Bind to the owning actor's ASC, retrying until it exists. */
	void ResolveOwnerAbilitySystem();

	/** Hand the view model to the instantiated widget. Safe to call more than once. */
	void PushViewModelToWidget();

	void StopRetryTimer();

	UPROPERTY(Transient)
	TObjectPtr<UMALStatViewModel> StatViewModel;

	FTimerHandle BindRetryTimer;
};
