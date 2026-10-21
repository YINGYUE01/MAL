// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/MALPlayerController.h"
#include "Input/MALInputConfig.h"
#include "Input/MALInputComponent.h"
#include "InputAction.h"
#include "EnhancedInputSubsystems.h"
#include "Tags/MALGameplayTags.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "Components/MALAbilitySystemComponent.h"
#include "Player/MALPlayerState.h"

AMALPlayerController::AMALPlayerController()
{
}

void AMALPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Register MappingContext after Enhanced Input subsystem is fully initialized
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (DefaultMappingContext)
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("AMALPlayerController: DefaultMappingContext is not set. No input will be received."));
		}
	}
}

void AMALPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// Bind InputConfig and hook up delegate
	if (UMALInputComponent* MALInput = Cast<UMALInputComponent>(InputComponent))
	{
		if (InputConfig)
		{
			MALInput->BindInputConfig(InputConfig);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("AMALPlayerController: InputConfig is not set. Ability input will not work."));
		}

		MALInput->OnInputActionEvent.AddDynamic(TObjectPtr<AMALPlayerController>(this), &AMALPlayerController::HandleInputActionEvent);
	}
	else
	{
		UE_LOG(LogTemp, Error,
			TEXT("AMALPlayerController: InputComponent is not a UMALInputComponent. Check DefaultInputComponentClass in DefaultInput.ini. Ability input will not work."));
	}
}

void AMALPlayerController::HandleInputActionEvent(FGameplayTag InputTag, const FInputActionValue& Value, ETriggerEvent EventType)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	UMALGameplayTags* GameplayTags = UMALGameplayTags::Get();

	// Cache the ASC once; used for both movement lock and ability input routing.
	UMALAbilitySystemComponent* MALASC = nullptr;
	if (const AMALPlayerState* MALPS = GetPlayerState<AMALPlayerState>())
	{
		MALASC = Cast<UMALAbilitySystemComponent>(MALPS->GetAbilitySystemComponent());
	}

	// While an ability owning Status.Attacking is active, lock Move/Jump so the character
	// commits to the action (the root motion lunge drives movement, preventing foot-sliding).
	const bool bMovementLocked = MALASC && MALASC->HasMatchingGameplayTag(GameplayTags->Status_Attacking);

	if (InputTag == GameplayTags->Input_Move)
	{
		if (bMovementLocked)
		{
			return;
		}

		if (APawn* CurrentPawn = GetPawn())
		{
			const FVector2D MoveInput = Value.Get<FVector2D>();

			const FRotator YawRotation(0, GetControlRotation().Yaw, 0);
			const FVector ForwardDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
			const FVector RightDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

			CurrentPawn->AddMovementInput(ForwardDir, MoveInput.Y);
			CurrentPawn->AddMovementInput(RightDir, MoveInput.X);
		}
	}
	else if (InputTag == GameplayTags->Input_Look)
	{
		const FVector2D LookInput = Value.Get<FVector2D>();
		AddYawInput(LookInput.X);
		AddPitchInput(LookInput.Y);
	}
	else if (InputTag == GameplayTags->Input_Jump)
	{
		if (bMovementLocked)
		{
			return;
		}

		if (ACharacter* MyCharacter = Cast<ACharacter>(GetPawn()))
		{
			if (EventType == ETriggerEvent::Started)
			{
				MyCharacter->Jump();
			}
			else if (EventType == ETriggerEvent::Completed)
			{
				MyCharacter->StopJumping();
			}
		}
	}
	else
	{
		// Buffer input for GAS abilities (Attack, Defend, Dash, etc.)
		if (MALASC)
		{
			if (EventType == ETriggerEvent::Started)
			{
				MALASC->AbilityInputTagPressed(InputTag, Value);
			}
			else if (EventType == ETriggerEvent::Completed)
			{
				MALASC->AbilityInputTagReleased(InputTag, Value);
			}
		}
	}
}

void AMALPlayerController::PostProcessInput(const float DeltaTime, const bool bGamePaused)
{
	if (AMALPlayerState* MALPS = GetPlayerState<AMALPlayerState>())
	{
		if (UMALAbilitySystemComponent* MALASC = Cast<UMALAbilitySystemComponent>(MALPS->GetAbilitySystemComponent()))
		{
			MALASC->ProcessAbilityInput(DeltaTime, bGamePaused);
		}
	}

	Super::PostProcessInput(DeltaTime, bGamePaused);
}
