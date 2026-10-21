// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
//#include "InputActionEvents.h"
#include "EnhancedInputComponent.h"
#include "GameplayTagContainer.h"
#include "MALPlayerController.generated.h"

class UMALInputConfig;
class UInputMappingContext;

UCLASS()
class MAL_API AMALPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AMALPlayerController();

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

	
};
