// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Widgets/MALUserWidget.h"

#include "Components/TextBlock.h"
#include "MVVMSubsystem.h"
#include "MVVMViewModelBase.h"
#include "View/MVVMView.h"

namespace
{
	// Where a row keeps its number. A name rather than a bound property: the block belongs to
	// the row blueprint, so this class cannot declare it as a BindWidget without every other
	// widget in the project inheriting a requirement it does not meet.
	const FName DisplayValueTextName(TEXT("AttributePoints"));

	/** Where a row keeps its label, under the same contract as DisplayValueTextName. */
	const FName DisplayNameTextName(TEXT("AttributeName"));
}

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

FText UMALUserWidget::Conv_IntToText(int32 Value) const
{
	// AsNumber rather than AsCultureInvariantString: a stat panel is player-facing text, and a
	// culture that groups thousands should group them here too.
	return FText::AsNumber(Value);
}

FText UMALUserWidget::Conv_FloatToText(float Value, int32 DecimalPlaces) const
{
	FNumberFormattingOptions Options;
	Options.MinimumFractionalDigits = FMath::Max(DecimalPlaces, 0);
	Options.MaximumFractionalDigits = FMath::Max(DecimalPlaces, 0);
	return FText::AsNumber(Value, &Options);
}

void UMALUserWidget::SetDisplayValue(int32 InValue)
{
	DisplayValue = InValue;

	// A binding can fire while the widget tree is still being realised, so a miss here is
	// expected rather than an error; NativeConstruct republishes once the tree exists.
	if (UTextBlock* ValueText = Cast<UTextBlock>(GetWidgetFromName(DisplayValueTextName)))
	{
		ValueText->SetText(FText::AsNumber(InValue));
	}
}

void UMALUserWidget::SetDisplayName(const FText& InName)
{
	DisplayName = InName;

	// Same as the value path: a miss while the tree is still being realised is expected rather
	// than an error, and NativeConstruct republishes once the blocks exist.
	if (UTextBlock* NameText = Cast<UTextBlock>(GetWidgetFromName(DisplayNameTextName)))
	{
		NameText->SetText(InName);
	}
}

void UMALUserWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Bindings may have been evaluated before the tree was built; the text blocks therefore may
	// hold their design-time text until this point.
	SetDisplayValue(DisplayValue);
	SetDisplayName(DisplayName);
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
