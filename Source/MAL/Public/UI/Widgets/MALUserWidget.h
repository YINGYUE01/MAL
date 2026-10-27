// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MALUserWidget.generated.h"

class UAbilitySystemComponent;
class UMVVMView;
class UMVVMViewModelBase;

/**
 * UMALUserWidget
 *
 * Base class for every widget in the project.
 *
 * Besides show/hide it wraps the two MVVM entry points so gameplay code never has to include
 * the ModelViewViewModel headers directly:
 *  - GetMVVMView() exposes the view extension the widget blueprint compiled.
 *  - SetViewModelByName() pushes a view model into a named source.
 *
 * A widget stays display-only: it reads FieldNotify properties from its view models and never
 * touches the AbilitySystem. That is what keeps a widget reusable outside a GAS world.
 *
 * Deliberately free of any screen's own vocabulary. Anything a single panel prints -- a stat
 * label, a formatted value -- belongs on that panel's widget class, otherwise every widget in
 * the project inherits an API it never uses and pays for it on construct.
 */
UCLASS()
class MAL_API UMALUserWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "MAL|UI")
	void Show(int32 ZOrder = 0);

	UFUNCTION(BlueprintCallable, Category = "MAL|UI")
	void Hide();

	UFUNCTION(BlueprintCallable, Category = "MAL|UI")
	bool IsShown() const;

	/**
	 * The MVVM view extension created for this widget's generated class.
	 * Null when the widget blueprint declares no view model source.
	 */
	UFUNCTION(BlueprintCallable, Category = "MAL|UI|MVVM")
	UMVVMView* GetMVVMView() const;

	/**
	 * Push a view model into a named source declared in the widget's View Binding panel.
	 * The source must be marked "Allow Setting", otherwise the assignment is rejected and
	 * this returns false.
	 */
	UFUNCTION(BlueprintCallable, Category = "MAL|UI|MVVM")
	bool SetViewModelByName(FName ViewModelName, UMVVMViewModelBase* ViewModel);

	/**
	 * Push a view model into the first settable source whose type matches it.
	 * Prefer this over SetViewModelByName: the source name typed in the View Binding panel is
	 * easy to get wrong and the failure is a silent runtime no-op.
	 */
	UFUNCTION(BlueprintCallable, Category = "MAL|UI|MVVM")
	bool SetViewModelByClass(UMVVMViewModelBase* ViewModel);

	/**
	 * View binding conversion: an integer as display text.
	 *
	 * Declared on the base class so every widget blueprint inherits it into its own conversion
	 * function list and no row needs a private copy. Both modifiers are load-bearing: the view
	 * binding compiler only accepts a widget's own conversion functions when they are
	 * BlueprintPure and const, and only then does the inherited function show up in the list.
	 *
	 * Matches the engine's To Text (Integer) for plain values; kept here so the formatting a
	 * stat panel uses has one definition instead of one per widget.
	 */
	UFUNCTION(BlueprintPure, Category = "MAL|UI|Conversion")
	FText Conv_IntToText(int32 Value) const;

	/** View binding conversion: a float rounded to DecimalPlaces digits. */
	UFUNCTION(BlueprintPure, Category = "MAL|UI|Conversion")
	FText Conv_FloatToText(float Value, int32 DecimalPlaces = 0) const;

	/**
	 * Ability system handoff for widgets nested inside a HUD.
	 *
	 * A nested widget constructs while the owning HUD is still waiting for the PlayerState to
	 * replicate in, so it cannot resolve the ASC itself without duplicating the HUD's retry
	 * loop. The HUD calls this once binding succeeds, and again after a re-possession.
	 */
	virtual void NativeAbilitySystemReady(UAbilitySystemComponent* InAbilitySystemComponent) {}

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MAL|UI")
	bool bIsVisible = false;
};
