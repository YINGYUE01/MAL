// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/MALUserWidget.h"
#include "MALMenuWidget.generated.h"

/**
 * UMALMenuWidget
 *
 * Base class for modal menus (pause, settings, inventory).
 *
 * Deliberately thin: the UI subsystem owns the stack, the input mode and the pause state, so a
 * menu only reacts to being shown or hidden. Keeping that responsibility outside the widget is
 * what lets the same menu be pushed from gameplay, a cutscene or the main menu.
 */
UCLASS(Abstract)
class MAL_API UMALMenuWidget : public UMALUserWidget
{
	GENERATED_BODY()

public:
	/** Called by the UI subsystem right after the menu is pushed onto the stack. */
	void ActivateMenu();

	/** Called right before the menu is popped off the stack. */
	void DeactivateMenu();

	/** Pause the game while this menu is on top. The subsystem restores the previous state on pop. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|UI")
	bool bPausesGame = false;

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "MAL|UI", meta = (DisplayName = "On Menu Activated"))
	void OnMenuActivated();

	UFUNCTION(BlueprintImplementableEvent, Category = "MAL|UI", meta = (DisplayName = "On Menu Deactivated"))
	void OnMenuDeactivated();
};
