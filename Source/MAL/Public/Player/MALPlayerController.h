// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
//#include "InputActionEvents.h"
#include "EnhancedInputComponent.h"
#include "GameplayTagContainer.h"
#include "Camera/CameraShakeBase.h"
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

private:
	FTimerHandle HitStopTimer;
	bool bHitStopActive = false;

	TWeakObjectPtr<ACharacter> HitStopCharacter;
	TWeakObjectPtr<UAnimInstance> HitStopAnimInstance;
	TWeakObjectPtr<UAnimMontage> HitStopMontage;

	float HitStopRestoreRate = 1.0f;
	float HitStopRestoreTimeDilation = 1.0f;
};
