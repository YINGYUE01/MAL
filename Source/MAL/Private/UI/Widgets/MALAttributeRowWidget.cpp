// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Widgets/MALAttributeRowWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "UI/ViewModel/MALAttributeRowViewModel.h"

void UMALAttributeRowWidget::SetDisplayValue(const FText& InValue)
{
	DisplayValue = InValue;

	// A binding can fire while the widget tree is still being realised, so a miss here is
	// expected rather than an error; NativeConstruct republishes once the tree exists.
	if (AttributePoints)
	{
		AttributePoints->SetText(InValue);
	}
}

void UMALAttributeRowWidget::SetDisplayName(const FText& InName)
{
	DisplayName = InName;

	// Same as the value path: a miss while the tree is still being realised is expected rather
	// than an error, and NativeConstruct republishes once the blocks exist.
	if (AttributeName)
	{
		AttributeName->SetText(InName);
	}
}

bool UMALAttributeRowWidget::SetRowViewModel(UMALAttributeRowViewModel* InViewModel)
{
	RowViewModel = InViewModel;

	if (!RowViewModel)
	{
		RefreshAddPointVisibility();
		return false;
	}

	// Seeded before the binding exists. A row blueprint whose source is not settable keeps
	// these two strings for its whole life, which is still better than the design-time text
	// every row shares.
	SetDisplayName(RowViewModel->AttributeName);
	SetDisplayValue(RowViewModel->DisplayValue);

	RefreshAddPointVisibility();

	// The one live path: the binding below writes DisplayName and DisplayValue through their
	// setters, and the setters own the text blocks. Nothing else repaints the row.
	return SetViewModelByClass(RowViewModel);
}

void UMALAttributeRowWidget::SetAddPointEnabled(bool bEnabled)
{
	if (AddPointButton)
	{
		AddPointButton->SetIsEnabled(bEnabled);
	}
}

void UMALAttributeRowWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Bound here rather than from the panel: the button belongs to this row, and the row
	// already knows which stat it spends on through its view model. AddUniqueDynamic keeps a
	// second construct from stacking handlers.
	if (AddPointButton)
	{
		AddPointButton->OnClicked.AddUniqueDynamic(this, &UMALAttributeRowWidget::HandleAddPointClicked);
	}

	RefreshAddPointVisibility();

	// The panel seeds a row before it is added to the viewport, and a row added later is
	// constructed after its view model was handed over; republishing here covers both orders.
	SetDisplayName(DisplayName);
	SetDisplayValue(DisplayValue);
}

void UMALAttributeRowWidget::RefreshAddPointVisibility()
{
	if (!AddPointButton)
	{
		return;
	}

	const bool bCanSpend = RowViewModel && RowViewModel->CanSpendPoints();
	AddPointButton->SetVisibility(bCanSpend ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UMALAttributeRowWidget::HandleAddPointClicked()
{
	if (RowViewModel)
	{
		RowViewModel->RequestAddPoint();
	}
}
