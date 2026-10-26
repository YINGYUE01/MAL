// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Attributes/MALPrimaryStat.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
//#include "InputActionEvents.h"
#include "EnhancedInputComponent.h"
#include "GameplayTagContainer.h"
#include "Camera/CameraShakeBase.h"
// TSubclassOf<T> dereferences T::StaticClass(), so the widget types cannot stay forward declared.
#include "UI/Widgets/MALDamageNumberWidget.h"
#include "UI/Widgets/MALHUDWidget.h"
#include "UI/Widgets/MALMenuWidget.h"
#include "MALPlayerController.generated.h"

class UMALInputConfig;
class UInputMappingContext;
class UMALHitFeedbackComponent;

UCLASS()
class MAL_API AMALPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AMALPlayerController();

	/**
	 * Server -> owning client: confirmed hit feedback.
	 * Starts the camera shake and a short local hit-stop.
	 *
	 * A listen server host skips the hit-stop: its local world is the authoritative one, so any
	 * freeze there would also stall what the server replicates and other clients would see the
	 * attacker hitch. The shake still plays because it is presentation only.
	 */
	UFUNCTION(Client, Reliable)
	void ClientOnHitLanded(float HitStopDuration, TSubclassOf<UCameraShakeBase> ShakeClass);

	/**
	 * Server -> owning client: draw one floating damage number.
	 *
	 * Unreliable on purpose. A number is presentation only and back-to-back hits produce a stream
	 * of them, so losing one is invisible while a backed-up reliable channel would stall input.
	 */
	UFUNCTION(Client, Unreliable)
	void ClientShowDamageNumber(float Amount, bool bCritical, bool bMagic, bool bKillingBlow, FVector WorldLocation);

	/** Open the pause menu, or close it when one is already open. */
	UFUNCTION(BlueprintCallable, Category = "MAL|UI")
	void TogglePauseMenu();

	/**
	 * Open the attribute menu, or close it when it is the one on top.
	 *
	 * Deliberately not "close whatever menu is open": pressing the attribute key while the
	 * pause menu is up should not dismiss the pause menu.
	 */
	UFUNCTION(BlueprintCallable, Category = "MAL|UI")
	void ToggleAttributeMenu();

	/**
	 * Pop the topmost menu. Callable from menu buttons: a paused world stops ticking input, so the
	 * close path has to come from the UI itself rather than a gameplay input action.
	 */
	UFUNCTION(BlueprintCallable, Category = "MAL|UI")
	void CloseTopMenu();

	/**
	 * Debug: bank attribute points on the local player.
	 *
	 * The panel that spends them is a UI task of its own, so this keeps the C++ side exercisable
	 * from the console until it exists.
	 */
	UFUNCTION(Exec)
	void MALAddAttributePoints(int32 Count);

	/** Debug: spend banked points on a primary stat: Vigor, Strength, Dexterity, Endurance or Spirit. */
	UFUNCTION(Exec)
	void MALSpendAttributePoint(const FString& StatName, int32 Count = 1);

	/**
	 * Client -> server: put Count banked points into a primary stat.
	 *
	 * The attribute panel lives on the client but the primaries are only writable on the
	 * authority, so a row's button has to cross the wire. Reliable: a dropped click would leave
	 * the row showing a value the server never agreed to.
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "MAL|Progression")
	void ServerSpendAttributePoint(EMALPrimaryStat Stat, int32 Count);

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PostProcessInput(const float DeltaTime, const bool bGamePaused) override;

	UFUNCTION()
	void HandleInputActionEvent(FGameplayTag InputTag, const FInputActionValue& Value, ETriggerEvent EventType);

	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Input")
	TObjectPtr<UMALInputConfig> InputConfig;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,  Category = "Input")
	TObjectPtr<UInputMappingContext>DefaultMappingContext;

	/** Persistent HUD spawned for the local player when gameplay begins. */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI")
	TSubclassOf<UMALHUDWidget> HUDWidgetClass;

	/** Menu pushed by TogglePauseMenu. Leave empty to disable the pause menu. */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI")
	TSubclassOf<UMALMenuWidget> PauseMenuClass;

	/** Menu pushed by ToggleAttributeMenu. Leave empty to disable the attribute panel. */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI")
	TSubclassOf<UMALMenuWidget> AttributeMenuClass;

	/**
	 * Widget spawned once per floating damage number.
	 * Leave empty to disable damage numbers entirely.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Damage")
	TSubclassOf<UMALDamageNumberWidget> DamageNumberWidgetClass;

private:
	/** Local hit-stop driver for confirmed hits (camera shake stays on the controller). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MAL|Combat", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMALHitFeedbackComponent> HitFeedbackComponent;
};
