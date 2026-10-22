// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Widgets/MALMenuWidget.h"

void UMALMenuWidget::ActivateMenu()
{
	if (IsShown())
	{
		return;
	}

	Show();
	OnMenuActivated();
}

void UMALMenuWidget::DeactivateMenu()
{
	if (!IsShown())
	{
		return;
	}

	OnMenuDeactivated();
	Hide();
}
