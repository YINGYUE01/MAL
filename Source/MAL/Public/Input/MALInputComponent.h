// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnhancedInputComponent.h"
#include "GameplayTagContainer.h"
#include "MALInputComponent.generated.h"

class UMALInputConfig;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FMALInputActionEvent, FGameplayTag, InputTag, const FInputActionValue&, Value, ETriggerEvent, EventType);

/**
 * UMALInputComponent
 * 
 * Custom Enhanced Input Component that binds InputConfig mappings
 * with Pressed/Triggered/Released callbacks driven by GameplayTags.
 */
UCLASS()
class MAL_API UMALInputComponent : public UEnhancedInputComponent
{
	GENERATED_BODY()

public:
	/** Broadcast when any bound input action is triggered */
	UPROPERTY(BlueprintAssignable)
	FMALInputActionEvent OnInputActionEvent;

	/** Bind all actions from an InputConfig, registering Pressed/Triggered/Released for each */
	void BindInputConfig(const UMALInputConfig* InputConfig);

protected:
	/** Called when an action is first pressed (Started event) */
	void InputPressed(const FInputActionValue& Value, FGameplayTag InputTag);

	/** Called while an action is held (Triggered event) */
	void InputTriggered(const FInputActionValue& Value, FGameplayTag InputTag);

	/** Called when an action is released (Completed event) */
	void InputReleased(const FInputActionValue& Value, FGameplayTag InputTag);
};
