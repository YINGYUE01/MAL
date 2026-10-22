// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "MALUISubsystem.generated.h"

class UMALHUDWidget;
class UMALMenuWidget;
class APlayerController;

/**
 * UMALUISubsystem
 *
 * Per-local-player UI owner. Everything that outlives a single widget lives here: the
 * persistent HUD, the modal menu stack, and the input/pause state that follows from it.
 *
 * This is deliberately separate from the view models. View models answer "what do I show",
 * the subsystem answers "what is open, who has focus, is the game running". Keeping those two
 * apart is what stops the UI layer from turning into one god object.
 *
 * A LocalPlayerSubsystem only exists where there is a LocalPlayer, so dedicated servers never
 * instantiate it.
 */
UCLASS()
class MAL_API UMALUISubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	/** Create the persistent HUD for this local player. No-op when one already exists. */
	void CreateHUD(TSubclassOf<UMALHUDWidget> HUDClass);

	UFUNCTION(BlueprintPure, Category = "MAL|UI")
	UMALHUDWidget* GetHUD() const { return HUDWidget; }

	/** Push a modal menu, switch to UI input and pause if the menu asks for it. */
	UFUNCTION(BlueprintCallable, Category = "MAL|UI")
	UMALMenuWidget* PushMenu(TSubclassOf<UMALMenuWidget> MenuClass);

	/** Pop the top menu. Gameplay input returns when the stack empties. */
	UFUNCTION(BlueprintCallable, Category = "MAL|UI")
	void PopMenu();

	UFUNCTION(BlueprintPure, Category = "MAL|UI")
	UMALMenuWidget* GetTopMenu() const;

	UFUNCTION(BlueprintPure, Category = "MAL|UI")
	bool IsMenuOpen() const { return MenuStack.Num() > 0; }

protected:
	virtual void Deinitialize() override;

private:
	APlayerController* GetPlayerController() const;

	/** Apply the input mode implied by the current menu stack depth. */
	void ApplyInputMode();

	/** Restore or apply pause based on the top menu's bPausesGame. */
	void UpdatePauseState();

	UPROPERTY()
	TObjectPtr<UMALHUDWidget> HUDWidget;

	UPROPERTY()
	TArray<TObjectPtr<UMALMenuWidget>> MenuStack;

	/** Pause state captured before the first menu opened; restored when the stack empties. */
	bool bPausedBeforeMenu = false;
};
