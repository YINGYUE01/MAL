// Copyright Epic Games, Inc. All Rights Reserved.

#include "Input/MALInputComponent.h"
#include "Input/MALInputConfig.h"

void UMALInputComponent::BindInputConfig(const UMALInputConfig* InputConfig)
{
	if (!InputConfig)
	{
		return;
	}

	for (const FMALInputActionMapping& Mapping : InputConfig->AbilityInputActions)
	{
		if (!Mapping.Action)
		{
			continue;
		}

		BindAction(Mapping.Action, ETriggerEvent::Started, this, &UMALInputComponent::InputPressed, Mapping.InputTag);
		BindAction(Mapping.Action, ETriggerEvent::Triggered, this, &UMALInputComponent::InputTriggered, Mapping.InputTag);
		BindAction(Mapping.Action, ETriggerEvent::Completed, this, &UMALInputComponent::InputReleased, Mapping.InputTag);
	}
	
}

void UMALInputComponent::InputPressed(const FInputActionValue& Value, FGameplayTag InputTag)
{
	OnInputActionEvent.Broadcast(InputTag, Value, ETriggerEvent::Started);
}

void UMALInputComponent::InputTriggered(const FInputActionValue& Value, FGameplayTag InputTag)
{
	OnInputActionEvent.Broadcast(InputTag, Value, ETriggerEvent::Triggered);
}

void UMALInputComponent::InputReleased(const FInputActionValue& Value, FGameplayTag InputTag)
{
	OnInputActionEvent.Broadcast(InputTag, Value, ETriggerEvent::Completed);
}
