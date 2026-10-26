// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/MALPlayerController.h"
#include "Combat/MALHitFeedbackComponent.h"
#include "Input/MALInputConfig.h"
#include "Input/MALInputComponent.h"
#include "InputAction.h"
#include "EnhancedInputSubsystems.h"
#include "Tags/MALGameplayTags.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "Components/MALAbilitySystemComponent.h"
#include "Components/MALProgressionComponent.h"
#include "AbilitySystem/Attributes/MALPrimaryStat.h"
#include "Player/MALPlayerState.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/LocalPlayer.h"
#include "UI/Framework/MALDamageNumberSubsystem.h"
#include "UI/Framework/MALUISubsystem.h"

AMALPlayerController::AMALPlayerController()
{
	// Hit-stop is controller-owned presentation: one per player, independent of the pawn
	// so a respawn does not orphan an in-flight freeze.
	HitFeedbackComponent = CreateDefaultSubobject<UMALHitFeedbackComponent>(TEXT("HitFeedbackComponent"));
}

void AMALPlayerController::ClientOnHitLanded_Implementation(float HitStopDuration, TSubclassOf<UCameraShakeBase> ShakeClass)
{
	if (ShakeClass && PlayerCameraManager)
	{
		PlayerCameraManager->StartCameraShake(ShakeClass, 1.0f);
	}

	if (HitFeedbackComponent)
	{
		HitFeedbackComponent->BeginHitStop(HitStopDuration);
	}
}

void AMALPlayerController::ClientShowDamageNumber_Implementation(float Amount, bool bCritical, bool bMagic, bool bKillingBlow, FVector WorldLocation)
{
	if (UMALDamageNumberSubsystem* DamageNumberSubsystem = ULocalPlayer::GetSubsystem<UMALDamageNumberSubsystem>(GetLocalPlayer()))
	{
		DamageNumberSubsystem->ShowDamageNumber(Amount, bCritical, bMagic, bKillingBlow, WorldLocation);
	}
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

	// Only a local player has a HUD: remote clients render their own and dedicated servers have
	// no viewport at all.
	if (IsLocalController())
	{
		if (UMALUISubsystem* UISubsystem = ULocalPlayer::GetSubsystem<UMALUISubsystem>(GetLocalPlayer()))
		{
			UISubsystem->CreateHUD(HUDWidgetClass);
		}

		if (UMALDamageNumberSubsystem* DamageNumberSubsystem = ULocalPlayer::GetSubsystem<UMALDamageNumberSubsystem>(GetLocalPlayer()))
		{
			DamageNumberSubsystem->SetDamageNumberWidgetClass(DamageNumberWidgetClass);
		}
	}
}

void AMALPlayerController::TogglePauseMenu()
{
	if (!IsLocalController())
	{
		return;
	}

	UMALUISubsystem* UISubsystem = ULocalPlayer::GetSubsystem<UMALUISubsystem>(GetLocalPlayer());
	if (!UISubsystem)
	{
		return;
	}

	if (UISubsystem->IsMenuOpen())
	{
		UISubsystem->PopMenu();
	}
	else if (PauseMenuClass)
	{
		UISubsystem->PushMenu(PauseMenuClass);
	}
}

void AMALPlayerController::CloseTopMenu()
{
	if (!IsLocalController())
	{
		return;
	}

	if (UMALUISubsystem* UISubsystem = ULocalPlayer::GetSubsystem<UMALUISubsystem>(GetLocalPlayer()))
	{
		UISubsystem->PopMenu();
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

	// While an ability that commits the character to an animation (attack, cast) is active,
	// lock Move/Jump. Without it the character keeps sliding under its own velocity while the
	// montage plays in place.
	const bool bMovementLocked = MALASC
		&& (MALASC->HasMatchingGameplayTag(GameplayTags->Status_Attacking)
			|| MALASC->HasMatchingGameplayTag(GameplayTags->Status_Casting));

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

void AMALPlayerController::MALAddAttributePoints(int32 Count)
{
	if (AMALPlayerState* MALPS = GetPlayerState<AMALPlayerState>())
	{
		if (UMALProgressionComponent* Progression = MALPS->GetProgressionComponent())
		{
			Progression->GrantAttributePoints(Count);
			UE_LOG(LogTemp, Log, TEXT("MALAddAttributePoints: %d banked, %d unspent."), Count, Progression->GetAttributePoints());
		}
	}
}

void AMALPlayerController::MALSpendAttributePoint(const FString& StatName, int32 Count)
{
	// String-to-enum by hand rather than through UEnum: the editor-only lookup is not there in a
	// packaged build, and five names are not worth a table.
	EMALPrimaryStat Stat = EMALPrimaryStat::Vigor;
	if (StatName.Equals(TEXT("Vigor"), ESearchCase::IgnoreCase))
	{
		Stat = EMALPrimaryStat::Vigor;
	}
	else if (StatName.Equals(TEXT("Strength"), ESearchCase::IgnoreCase))
	{
		Stat = EMALPrimaryStat::Strength;
	}
	else if (StatName.Equals(TEXT("Dexterity"), ESearchCase::IgnoreCase))
	{
		Stat = EMALPrimaryStat::Dexterity;
	}
	else if (StatName.Equals(TEXT("Endurance"), ESearchCase::IgnoreCase))
	{
		Stat = EMALPrimaryStat::Endurance;
	}
	else if (StatName.Equals(TEXT("Spirit"), ESearchCase::IgnoreCase))
	{
		Stat = EMALPrimaryStat::Spirit;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("MALSpendAttributePoint: unknown stat '%s'. Use Vigor, Strength, Dexterity, Endurance or Spirit."), *StatName);
		return;
	}

	// Same entry point the attribute panel uses, so the console exercises the path the UI takes
	// rather than a parallel one that could drift from it.
	ServerSpendAttributePoint(Stat, Count);
}

void AMALPlayerController::ServerSpendAttributePoint_Implementation(EMALPrimaryStat Stat, int32 Count)
{
	AMALPlayerState* MALPS = GetPlayerState<AMALPlayerState>();
	UMALProgressionComponent* Progression = MALPS ? MALPS->GetProgressionComponent() : nullptr;
	if (!Progression)
	{
		return;
	}

	if (!Progression->SpendAttributePoints(Stat, Count))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("ServerSpendAttributePoint: %d point(s) on stat %d refused (not enough unspent, or at the class cap)."),
			Count, static_cast<int32>(Stat));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("ServerSpendAttributePoint: %d -> stat %d, %d unspent left."),
		Count, static_cast<int32>(Stat), Progression->GetAttributePoints());
}
