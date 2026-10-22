// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
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
class ACharacter;
class UAnimInstance;
class UAnimMontage;

UCLASS()
class MAL_API AMALPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AMALPlayerController();

	/**
	 * Server -> owning client: confirmed hit feedback.
	 * Starts the camera shake and a short local hit-stop (animation + movement freeze).
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

	/**
	 * Server side: deliver a resolved hit to every client that should draw a number for it.
	 *
	 * Two clients can care about the same hit - the victim's owner ("I took damage") and the
	 * attacker's owner ("I hit something") - so both are notified and deduplicated. Called from
	 * UMALAttributeSet::PostGameplayEffectExecute, which only runs on the authority.
	 */
	static void RouteDamageNumber(AActor* Victim, AActor* Instigator, const FVector& WorldLocation,
		float Amount, bool bCritical, bool bMagic, bool bKillingBlow);

	/** Open the pause menu, or close it when one is already open. */
	UFUNCTION(BlueprintCallable, Category = "MAL|UI")
	void TogglePauseMenu();

	/**
	 * Pop the topmost menu. Callable from menu buttons: a paused world stops ticking input, so the
	 * close path has to come from the UI itself rather than a gameplay input action.
	 */
	UFUNCTION(BlueprintCallable, Category = "MAL|UI")
	void CloseTopMenu();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PostProcessInput(const float DeltaTime, const bool bGamePaused) override;

	/** Freeze the locally controlled character briefly; overlapping hits extend the timer. */
	void BeginHitStop(float Duration);

	/** Restore montage play rate and character time dilation. */
	void EndHitStop();

	
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

	/**
	 * Widget spawned once per floating damage number.
	 * Leave empty to disable damage numbers entirely.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Damage")
	TSubclassOf<UMALDamageNumberWidget> DamageNumberWidgetClass;

private:
	FTimerHandle HitStopTimer;
	bool bHitStopActive = false;

	TWeakObjectPtr<ACharacter> HitStopCharacter;
	TWeakObjectPtr<UAnimInstance> HitStopAnimInstance;
	TWeakObjectPtr<UAnimMontage> HitStopMontage;

	float HitStopRestoreRate = 1.0f;
	float HitStopRestoreTimeDilation = 1.0f;
};
