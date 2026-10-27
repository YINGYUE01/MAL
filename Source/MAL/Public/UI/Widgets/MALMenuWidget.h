// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/MALUserWidget.h"
#include "MALMenuWidget.generated.h"

class UWidgetAnimation;

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

	/**
	 * Where the panel sits horizontally on screen: 0 is the left edge, 0.5 the centre, 1 the right
	 * edge. Vertical is always centred, which is what keeps a side panel clear of the HUD's
	 * top-left icons.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|UI")
	float HorizontalAnchor = 0.f;

	/** Gap between the panel and the screen edge it is anchored to, in slate units. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|UI")
	float ScreenEdgeMargin = 48.f;

	/**
	 * UMG animation played when the menu opens. Assign one from the widget blueprint's class
	 * defaults; left unset, the built-in fade and slide runs instead.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|UI|Transition")
	TObjectPtr<UWidgetAnimation> OpenAnimation;

	/**
	 * UMG animation played when the menu closes. Same fallback as OpenAnimation, and the menu
	 * is only removed from the viewport once it ends either way.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|UI|Transition")
	TObjectPtr<UWidgetAnimation> CloseAnimation;

	/** Seconds the built-in transition takes. Ignored once the animations above are assigned. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|UI|Transition")
	float TransitionTime = 0.18f;

	/** Slate units the panel travels during the built-in transition. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|UI|Transition")
	float TransitionSlideDistance = 48.f;

protected:
	//~ Begin UUserWidget
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	//~ End UUserWidget

	UFUNCTION(BlueprintImplementableEvent, Category = "MAL|UI", meta = (DisplayName = "On Menu Activated"))
	void OnMenuActivated();

	UFUNCTION(BlueprintImplementableEvent, Category = "MAL|UI", meta = (DisplayName = "On Menu Deactivated"))
	void OnMenuDeactivated();

private:
	/** Drives RenderOpacity and RenderTranslation from TransitionAlpha. */
	void ApplyTransition();

	/** Removes the menu once the close animation has played out. */
	UFUNCTION()
	void HandleCloseAnimationFinished();

	/** 1 is fully on screen, 0 fully gone. Read only by the built-in transition. */
	float TransitionAlpha = 1.f;

	/** Where TransitionAlpha is heading. */
	float TargetAlpha = 1.f;

	/** Set while an exit is playing, so the menu outlives it rather than vanishing mid-way. */
	bool bClosing = false;

	/** Held as a member so the finish callback is bound once per widget, not once per close. */
	FWidgetAnimationDynamicEvent CloseFinishedEvent;

	/** True once CloseFinishedEvent is bound to CloseAnimation. */
	bool bCloseFinishedBound = false;
};
