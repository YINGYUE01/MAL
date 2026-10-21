// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Widgets/MALUserWidget.h"

void UMALUserWidget::Show()
{
	if (!bIsVisible)
	{
		AddToViewport();
		bIsVisible = true;
	}
}

void UMALUserWidget::Hide()
{
	if (bIsVisible)
	{
		RemoveFromParent();
		bIsVisible = false;
	}
}

void UMALUserWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UMALUserWidget::NativeDestruct()
{
	Super::NativeDestruct();
}
