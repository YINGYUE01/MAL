// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/MALPlayerController.h"
#include "Combat/MALHitFeedbackComponent.h"
#include "Combat/MALWeaponComponent.h"
#include "Combat/MALWeaponDefinition.h"
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
#include "UI/Widgets/MALAttributeMenuWidget.h"
#include "UI/Widgets/MALSkillMenuWidget.h"
#include "UI/ViewModel/MALAbilityBarViewModel.h"
#include "Engine/World.h"
#include "TimerManager.h"

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

void AMALPlayerController::ToggleAttributeMenu()
{
	UE_LOG(LogTemp, Log, TEXT("MALInput: ToggleAttributeMenu class=%s"), AttributeMenuClass ? *AttributeMenuClass->GetName() : TEXT("None"));

	if (!IsLocalController())
	{
		return;
	}

	UMALUISubsystem* UISubsystem = ULocalPlayer::GetSubsystem<UMALUISubsystem>(GetLocalPlayer());
	if (!UISubsystem)
	{
		return;
	}

	// Only this panel's own key closes it. Popping whatever happens to be on top would make the
	// attribute key a second, less predictable way to dismiss the pause menu.
	if (const UMALMenuWidget* TopMenu = UISubsystem->GetTopMenu())
	{
		if (TopMenu->IsA<UMALAttributeMenuWidget>())
		{
			UISubsystem->PopMenu();
			return;
		}
	}

	if (AttributeMenuClass)
	{
		UMALMenuWidget* Pushed = UISubsystem->PushMenu(AttributeMenuClass);
		UE_LOG(LogTemp, Log, TEXT("MALInput: pushed %s"), *GetNameSafe(Pushed));
	}
}

void AMALPlayerController::ToggleSkillMenu()
{
	UE_LOG(LogTemp, Log, TEXT("MALInput: ToggleSkillMenu class=%s"), SkillMenuClass ? *SkillMenuClass->GetName() : TEXT("None"));

	if (!IsLocalController())
	{
		return;
	}

	UMALUISubsystem* UISubsystem = ULocalPlayer::GetSubsystem<UMALUISubsystem>(GetLocalPlayer());
	if (!UISubsystem)
	{
		return;
	}

	// Same rule as the attribute key: only this panel's own key closes it. Matched against the
	// C++ type rather than the configured class, so swapping the blueprint cannot leave an open
	// panel that this key no longer recognises.
	if (const UMALMenuWidget* TopMenu = UISubsystem->GetTopMenu())
	{
		if (TopMenu->IsA<UMALSkillMenuWidget>())
		{
			UISubsystem->PopMenu();
			return;
		}
	}

	if (SkillMenuClass)
	{
		UMALMenuWidget* Pushed = UISubsystem->PushMenu(SkillMenuClass);
		UE_LOG(LogTemp, Log, TEXT("MALInput: pushed %s"), *GetNameSafe(Pushed));
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

			for (const FMALInputActionMapping& Mapping : InputConfig->AbilityInputActions)
			{
				UE_LOG(LogTemp, Log, TEXT("MALInput: bound %s -> %s"), *Mapping.InputTag.ToString(), *GetNameSafe(Mapping.Action));
			}
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

	// Move and Look repeat every frame; only the discrete actions are worth a line each.
	if (InputTag != GameplayTags->Input_Move && InputTag != GameplayTags->Input_Look)
	{
		UE_LOG(LogTemp, Log, TEXT("MALInput: tag=%s event=%s"), *InputTag.ToString(),
			*StaticEnum<ETriggerEvent>()->GetNameStringByValue(static_cast<int64>(EventType)));
	}

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

	// A menu on screen owns the player's attention: walking or looking underneath it would drag
	// the camera around behind the panel. The game keeps running while it is open, so this has
	// to be filtered here rather than by pausing the world -- a paused world also stops input,
	// and the key that opened the panel would never arrive to close it again.
	const UMALUISubsystem* UISubsystem = ULocalPlayer::GetSubsystem<UMALUISubsystem>(GetLocalPlayer());
	const bool bMenuOpen = UISubsystem && UISubsystem->IsMenuOpen();

	if (InputTag == GameplayTags->Input_Move)
	{
		if (bMovementLocked || bMenuOpen)
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
		if (bMenuOpen)
		{
			return;
		}

		const FVector2D LookInput = Value.Get<FVector2D>();
		AddYawInput(LookInput.X);
		AddPitchInput(LookInput.Y);
	}
	else if (InputTag == GameplayTags->Input_Jump)
	{
		if (bMovementLocked || bMenuOpen)
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
	else if (InputTag == GameplayTags->Input_AttributeMenu)
	{
		// Started only: triggering on the repeat rate would flicker the panel while the key is held.
		if (EventType == ETriggerEvent::Started)
		{
			ToggleAttributeMenu();
		}
	}
	else if (InputTag == GameplayTags->Input_SkillMenu)
	{
		// Started only, for the same reason as the attribute key above.
		if (EventType == ETriggerEvent::Started)
		{
			ToggleSkillMenu();
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

void AMALPlayerController::ServerUnlockSkill_Implementation(FGameplayTag SkillTag)
{
	AMALPlayerState* MALPS = GetPlayerState<AMALPlayerState>();
	UMALProgressionComponent* Progression = MALPS ? MALPS->GetProgressionComponent() : nullptr;
	if (!Progression)
	{
		return;
	}

	FText Reason;
	if (!Progression->CanUnlockSkill(SkillTag, Reason))
	{
		UE_LOG(LogTemp, Warning, TEXT("ServerUnlockSkill: %s refused: %s"),
			*SkillTag.ToString(), *Reason.ToString());
		return;
	}

	if (!Progression->UnlockSkill(SkillTag))
	{
		UE_LOG(LogTemp, Warning, TEXT("ServerUnlockSkill: %s failed after passing its checks."), *SkillTag.ToString());
	}
}

void AMALPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// The server side of possession. A client gets here through AcknowledgePossession instead.
	BindToWeaponComponent();
}

void AMALPlayerController::AcknowledgePossession(APawn* InPawn)
{
	Super::AcknowledgePossession(InPawn);

	// The client side of possession, and the first moment this machine has a pawn to watch.
	BindToWeaponComponent();
}

void AMALPlayerController::BindToWeaponComponent()
{
	UnbindWeaponComponent();

	APawn* ControlledPawn = GetPawn();
	UMALWeaponComponent* WeaponComponent =
		ControlledPawn ? ControlledPawn->FindComponentByClass<UMALWeaponComponent>() : nullptr;
	if (!WeaponComponent)
	{
		return;
	}

	BoundWeaponComponent = WeaponComponent;
	WeaponChangedDelegateHandle = WeaponComponent->OnEquippedWeaponChanged.AddUObject(
		this, &AMALPlayerController::HandleEquippedWeaponChanged);
}

void AMALPlayerController::UnbindWeaponComponent()
{
	if (UMALWeaponComponent* WeaponComponent = BoundWeaponComponent.Get())
	{
		WeaponComponent->OnEquippedWeaponChanged.Remove(WeaponChangedDelegateHandle);
	}

	BoundWeaponComponent.Reset();
	WeaponChangedDelegateHandle.Reset();
}

void AMALPlayerController::HandleEquippedWeaponChanged()
{
	RefreshAbilityBar();

	// Again next frame. A client learns about a swap from OnRep_EquippedWeapon, while the specs
	// the swap granted arrive in a replication bunch of their own; when the property wins the
	// race, the first refresh still sees the previous kit.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(this, &AMALPlayerController::RefreshAbilityBar);
	}
}

void AMALPlayerController::RefreshAbilityBar()
{
	UMALUISubsystem* UISubsystem = ULocalPlayer::GetSubsystem<UMALUISubsystem>(GetLocalPlayer());
	UMALHUDWidget* HUD = UISubsystem ? UISubsystem->GetHUD() : nullptr;
	UMALAbilityBarViewModel* AbilityBar = HUD ? HUD->GetAbilityBarViewModel() : nullptr;
	if (AbilityBar)
	{
		AbilityBar->RefreshSlots();
	}
}

void AMALPlayerController::ServerEquipWeapon_Implementation(UMALWeaponDefinition* Weapon)
{
	APawn* ControlledPawn = GetPawn();
	UMALWeaponComponent* WeaponComponent =
		ControlledPawn ? ControlledPawn->FindComponentByClass<UMALWeaponComponent>() : nullptr;
	if (!WeaponComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("ServerEquipWeapon: %s has no weapon component."), *GetNameSafe(ControlledPawn));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("ServerEquipWeapon: %s equips %s."),
		*GetNameSafe(ControlledPawn), Weapon ? *Weapon->GetName() : TEXT("nothing"));

	WeaponComponent->EquipWeapon(Weapon);
}

void AMALPlayerController::MALEQUIP(const FString& WeaponAssetPath)
{
	if (WeaponAssetPath.IsEmpty())
	{
		ServerEquipWeapon(nullptr);
		return;
	}

	UMALWeaponDefinition* Weapon = LoadObject<UMALWeaponDefinition>(nullptr, *WeaponAssetPath);
	if (!Weapon)
	{
		UE_LOG(LogTemp, Warning, TEXT("MALEQUIP: no weapon definition at '%s'."), *WeaponAssetPath);
		return;
	}

	ServerEquipWeapon(Weapon);
}
