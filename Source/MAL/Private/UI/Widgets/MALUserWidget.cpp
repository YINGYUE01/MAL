// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Widgets/MALUserWidget.h"

#include "MVVMSubsystem.h"
#include "MVVMViewModelBase.h"
#include "View/MVVMView.h"

void UMALUserWidget::Show(int32 ZOrder)
{
	if (IsInViewport())
	{
		return;
	}

	AddToViewport(ZOrder);
	bIsVisible = true;
}

void UMALUserWidget::Hide()
{
	if (!IsInViewport())
	{
		return;
	}

	RemoveFromParent();
	bIsVisible = false;
}

bool UMALUserWidget::IsShown() const
{
	return IsInViewport();
}

UMVVMView* UMALUserWidget::GetMVVMView() const
{
	return UMVVMSubsystem::GetViewFromUserWidget(this);
}

bool UMALUserWidget::SetViewModelByName(FName ViewModelName, UMVVMViewModelBase* ViewModel)
{
	UMVVMView* View = GetMVVMView();
	if (!View)
	{
		// Not an error: a widget that only displays static content has no view model source.
		UE_LOG(LogTemp, Verbose,
			TEXT("UMALUserWidget: %s has no MVVM view. Declare a view model source in its View Binding panel."),
			*GetNameSafe(this));
		return false;
	}

	if (!ViewModel || ViewModelName.IsNone())
	{
		return false;
	}

	TScriptInterface<INotifyFieldValueChanged> ViewModelInterface;
	ViewModelInterface.SetObject(ViewModel);
	ViewModelInterface.SetInterface(Cast<INotifyFieldValueChanged>(ViewModel));

	const bool bApplied = View->SetViewModel(ViewModelName, ViewModelInterface);
	if (!bApplied)
	{
		// Rejected when the source name does not exist or is not marked "Allow Setting".
		UE_LOG(LogTemp, Warning,
			TEXT("UMALUserWidget: %s could not bind view model '%s' (%s). Check the source name and its Allow Setting flag."),
			*GetNameSafe(this), *ViewModelName.ToString(), *GetNameSafe(ViewModel));
	}

	return bApplied;
}

bool UMALUserWidget::SetViewModelByClass(UMVVMViewModelBase* ViewModel)
{
	UMVVMView* View = GetMVVMView();
	if (!View)
	{
		UE_LOG(LogTemp, Verbose,
			TEXT("UMALUserWidget: %s has no MVVM view. Declare a view model source in its View Binding panel."),
			*GetNameSafe(this));
		return false;
	}

	if (!ViewModel)
	{
		return false;
	}

	TScriptInterface<INotifyFieldValueChanged> ViewModelInterface;
	ViewModelInterface.SetObject(ViewModel);
	ViewModelInterface.SetInterface(Cast<INotifyFieldValueChanged>(ViewModel));

	const bool bApplied = View->SetViewModelByClass(ViewModelInterface);
	if (!bApplied)
	{
		// Happens when the source is not settable (it creates its own instance) or when the
		// widget declares no source of this type at all.
		UE_LOG(LogTemp, Warning,
			TEXT("UMALUserWidget: %s has no settable view model source matching %s. Check that the View Binding source is of that type and has Allow Setting enabled."),
			*GetNameSafe(this), *GetNameSafe(ViewModel));
	}

	return bApplied;
}
