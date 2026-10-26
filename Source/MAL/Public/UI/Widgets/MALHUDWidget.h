// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"
#include "UI/Widgets/MALUserWidget.h"
#include "MALHUDWidget.generated.h"

class UAbilitySystemComponent;
class UMALAbilityBarViewModel;
class UMALProgressionViewModel;
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

	UFUNCTION(BlueprintPure, Category = "MAL|UI")
	UMALAbilityBarViewModel* GetAbilityBarViewModel() const { return AbilityBarViewModel; }

	UFUNCTION(BlueprintPure, Category = "MAL|UI")
	UMALProgressionViewModel* GetProgressionViewModel() const { return ProgressionViewModel; }

protected:
	/**
	 * Optional source name to target precisely. Leave empty (the default) to match the source by
	 * type instead, which is what you want unless the widget declares several sources of the same
	 * type. A name typed in the View Binding panel is easy to get wrong and fails as a silent
	 * runtime no-op.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|MVVM")
	FName StatViewModelName = NAME_None;

	/** Same contract as StatViewModelName, for the ability bar view model. */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|MVVM")
	FName AbilityBarViewModelName = NAME_None;

	/** Same contract as StatViewModelName, for the progression view model. */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|MVVM")
	FName ProgressionViewModelName = NAME_None;

	/**
	 * Input tags drawn on the ability bar, in screen order.
	 *
	 * Leave empty to get the default bar (Attack, Buff, Defend, Dash) without having to touch the
	 * blueprint; the fallback lives in InitializeForPlayer because the tags are native and are
	 * not safe to read while the widget's class default object is being constructed.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Ability")
	TArray<FGameplayTag> AbilitySlotTags;

	/** Retry interval while the PlayerState (and with it the ASC) is still replicating in. */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI", meta = (ClampMin = "0.02"))
	float AbilitySystemBindRetryInterval = 0.1f;

	/**
	 * Fast retries to spend before falling back to AbilitySystemBindSlowRetryInterval.
	 * At the default 0.1s interval this covers the first three seconds after creation.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI", meta = (ClampMin = "1"))
	int32 AbilitySystemBindFastRetryCount = 30;

	/**
	 * Retry interval once the fast window has elapsed.
	 *
	 * Polling never stops on purpose: the HUD outlives possession changes and respawns, so a
	 * later PlayerState still has to be picked up. It only slows down, because past the first
	 * few seconds an ASC that is still missing is not merely late.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI", meta = (ClampMin = "0.1"))
	float AbilitySystemBindSlowRetryInterval = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|UI")
	TObjectPtr<UMALStatViewModel> StatViewModel;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|UI")
	TObjectPtr<UMALAbilityBarViewModel> AbilityBarViewModel;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|UI")
	TObjectPtr<UMALProgressionViewModel> ProgressionViewModel;

	virtual void NativeDestruct() override;

private:
	void TryBindAbilitySystem();
	void StopRetryTimer();
	void ArmRetryTimer(float Interval);

	/**
	 * Hand the progression view model to nested widgets that cannot inherit it.
	 *
	 * A nested widget's view is built by Show(), before this HUD has a source to inherit from,
	 * so a child that expects the progression view model through its parent context ends up
	 * bound to nothing. Publishing directly bypasses that ordering.
	 */
	void PublishProgressionToNestedWidgets();

	/**
	 * Forward the ability system to widgets nested in this HUD's own tree.
	 *
	 * Such a widget constructs before the PlayerState (and with it the ASC) has replicated
	 * in, so it cannot resolve the ASC itself without duplicating this HUD's retry loop.
	 * Called from TryBindAbilitySystem, which is also where a re-possession rebinds it.
	 */
	void PublishAbilitySystemToNestedWidgets(UAbilitySystemComponent* InAbilitySystemComponent);

	TWeakObjectPtr<APlayerController> OwningPlayerController;
	FTimerHandle BindRetryTimer;
	int32 BindRetryCount = 0;
};
