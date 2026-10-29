// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "MVVMViewModelBase.h"
#include "AttributeSet.h"
#include "GameplayEffectTypes.h"
#include "MALUIViewModelBase.generated.h"

class UAbilitySystemComponent;

/** One live attribute listener; kept together so unbinding can find the delegate again. */
struct FMALAttributeBinding
{
	FGameplayAttribute Attribute;
	FDelegateHandle Handle;
	TFunction<void(const FOnAttributeChangeData&)> Callback;
};

/**
 * UMALUIViewModelBase
 *
 * Shared base for every UI view model in the project.
 *
 * Responsibilities:
 *  - Owns the binding lifetime to an AbilitySystemComponent, including teardown when the ASC
 *    is replaced or destroyed (respawn, re-possession, local player leaving).
 *  - Gives subclasses one place to declare attribute listeners without juggling delegate
 *    handles or worrying about stale pointers.
 *
 * Subclasses override OnAbilitySystemReady to register the attributes they display and push
 * values out through UE_MVVM_SET_PROPERTY_VALUE. Nothing here assumes the ASC belongs to a
 * player, so the same view model drives an enemy nameplate widget.
 */
UCLASS(Abstract, Blueprintable)
class MAL_API UMALUIViewModelBase : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/**
	 * Point the view model at a new ASC. The previous binding is torn down first, so calling
	 * this repeatedly while the PlayerState replicates in is safe.
	 */
	void SetAbilitySystemComponent(UAbilitySystemComponent* InAbilitySystemComponent);

	UFUNCTION(BlueprintPure, Category = "MAL|UI")
	UAbilitySystemComponent* GetAbilitySystemComponent() const;

protected:
	/** Called after the new ASC is cached and before any attribute listener is added. */
	virtual void OnAbilitySystemReady(UAbilitySystemComponent* InAbilitySystemComponent) {}

	/** Called after every attribute listener was removed. */
	virtual void OnAbilitySystemReleased() {}

	/**
	 * Listen for changes on one attribute. The callback also fires once immediately with the
	 * current value, because GameplayEffects only broadcast on change: without the priming
	 * call a freshly created HUD would sit at zero until the first hit lands.
	 */
	void BindAttribute(const FGameplayAttribute& Attribute, TFunction<void(const FOnAttributeChangeData&)>&& OnChanged);

	/** Remove every listener registered through BindAttribute. */
	void UnbindAllAttributes();

	/**
	 * Drive TickFunction from the core ticker. A no-op while a ticker is already running, so a
	 * caller can start it from every path that needs one; TickFunction returns false to stop.
	 *
	 * Weak lambda internally: the core ticker outlives the UObject and has no way to learn it
	 * was collected.
	 */
	void StartTicker(TFunction<bool(float)>&& TickFunction);

	/** Stop the ticker started by StartTicker. Safe to call when none is running. */
	void StopTicker();

private:
	void HandleAttributeChanged(const FOnAttributeChangeData& ChangeData);

	TWeakObjectPtr<UAbilitySystemComponent> CachedAbilitySystemComponent;

	TArray<FMALAttributeBinding> AttributeBindings;

	FTSTicker::FDelegateHandle ViewModelTicker;
};
