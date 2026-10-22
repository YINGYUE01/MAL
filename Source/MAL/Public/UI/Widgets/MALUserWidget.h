// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MALUserWidget.generated.h"

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

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MAL|UI")
	bool bIsVisible = false;
};
