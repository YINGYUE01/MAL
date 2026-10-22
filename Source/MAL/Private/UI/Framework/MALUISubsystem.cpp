// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Framework/MALUISubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "UI/Widgets/MALHUDWidget.h"
#include "UI/Widgets/MALMenuWidget.h"

void UMALUISubsystem::CreateHUD(TSubclassOf<UMALHUDWidget> HUDClass)
{
	if (HUDWidget || !HUDClass)
	{
		return;
	}

	APlayerController* PlayerController = GetPlayerController();
	if (!PlayerController)
	{
		return;
	}

	UMALHUDWidget* NewHUD = CreateWidget<UMALHUDWidget>(PlayerController, HUDClass);
	if (!NewHUD)
	{
		return;
	}

	HUDWidget = NewHUD;

	// Show before initializing: adding to the viewport runs the widget's construct path, which
	// is what materializes the compiled view bindings the view model is pushed into.
	NewHUD->Show(0);
	NewHUD->InitializeForPlayer(PlayerController);
}

UMALMenuWidget* UMALUISubsystem::PushMenu(TSubclassOf<UMALMenuWidget> MenuClass)
{
	APlayerController* PlayerController = GetPlayerController();
	if (!PlayerController || !MenuClass)
	{
		return nullptr;
	}

	UMALMenuWidget* Menu = CreateWidget<UMALMenuWidget>(PlayerController, MenuClass);
	if (!Menu)
	{
		return nullptr;
	}

	// Capture the game's own pause state once, before any menu modifies it, so closing the last
	// menu restores whatever the game had rather than always unpausing.
	if (MenuStack.Num() == 0)
	{
		bPausedBeforeMenu = UGameplayStatics::IsGamePaused(GetWorld());
	}

	MenuStack.Add(Menu);
	Menu->ActivateMenu();

	ApplyInputMode();
	UpdatePauseState();

	return Menu;
}

void UMALUISubsystem::PopMenu()
{
	if (MenuStack.Num() == 0)
	{
		return;
	}

	TObjectPtr<UMALMenuWidget> Menu = MenuStack.Pop();
	if (Menu)
	{
		Menu->DeactivateMenu();
	}

	ApplyInputMode();
	UpdatePauseState();
}

UMALMenuWidget* UMALUISubsystem::GetTopMenu() const
{
	return MenuStack.Num() > 0 ? MenuStack.Last().Get() : nullptr;
}

void UMALUISubsystem::ApplyInputMode()
{
	APlayerController* PlayerController = GetPlayerController();
	if (!PlayerController)
	{
		return;
	}

	if (MenuStack.Num() > 0)
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		PlayerController->SetInputMode(InputMode);
		PlayerController->bShowMouseCursor = true;
	}
	else
	{
		PlayerController->SetInputMode(FInputModeGameOnly());
		PlayerController->bShowMouseCursor = false;
	}
}

void UMALUISubsystem::UpdatePauseState()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const UMALMenuWidget* TopMenu = GetTopMenu();
	const bool bShouldPause = (MenuStack.Num() == 0) ? bPausedBeforeMenu : (TopMenu && TopMenu->bPausesGame);

	UGameplayStatics::SetGamePaused(World, bShouldPause);
}

APlayerController* UMALUISubsystem::GetPlayerController() const
{
	if (const ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		return LocalPlayer->GetPlayerController(GetWorld());
	}

	return nullptr;
}

void UMALUISubsystem::Deinitialize()
{
	// A LocalPlayer can be torn down without its widgets ever receiving NativeDestruct, so close
	// the stack explicitly instead of relying on widget lifetime.
	for (const TObjectPtr<UMALMenuWidget>& Menu : MenuStack)
	{
		if (Menu)
		{
			Menu->DeactivateMenu();
		}
	}
	MenuStack.Reset();

	if (HUDWidget)
	{
		HUDWidget->RemoveFromParent();
		HUDWidget = nullptr;
	}

	Super::Deinitialize();
}
