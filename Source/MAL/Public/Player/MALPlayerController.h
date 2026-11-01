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
class UMALLockOnComponent;
class UMALWeaponComponent;
class UMALWeaponDefinition;

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
	 * Open the skill tree menu, or close it when it is the one on top.
	 *
	 * Same contract as ToggleAttributeMenu: this key only ever dismisses its own panel, so
	 * pressing it while another menu is up opens this one on top of it instead.
	 */
	UFUNCTION(BlueprintCallable, Category = "MAL|UI")
	void ToggleSkillMenu();

	/**
	 * Pop the topmost menu. Callable from menu buttons: a paused world stops ticking input, so the
	 * close path has to come from the UI itself rather than a gameplay input action.
	 */
	UFUNCTION(BlueprintCallable, Category = "MAL|UI")
	void CloseTopMenu();

	/**
	 * Debug: toggle lock-on and log the result, so the toggle is testable without a key binding.
	 *
	 * DelaySeconds defers the toggle, which is what a headless run needs: -ExecCmds fires on the
	 * first frame, before the spawner has put anything hostile in the level.
	 */
	UFUNCTION(Exec)
	void MALLOCK(float DelaySeconds = 0.0f);

	/** Debug: fire the dash input once, so the dodge is testable without a key binding. */
	UFUNCTION(Exec)
	void MALDODGE();

	/**
	 * Debug: spawn an actor class in front of the player, facing them.
	 *
	 * Exists because the level's spawner is a blueprint with a hand-authored class list: this is
	 * what lets a new enemy be exercised before it is added to that list.
	 */
	UFUNCTION(Exec)
	void MALSPAWN(const FString& ClassPath, float Distance = 500.0f);

	/** Debug: print player health and the collision responses involved in enemy hits. */
	UFUNCTION(Exec)
	void MALCOMBATINFO(float DelaySeconds = 0.0f);

	/** Target the camera is currently locked onto, or null. */
	UFUNCTION(BlueprintPure, Category = "MAL|LockOn")
	AActor* GetLockedTarget() const;

	/** The controller-owned lock-on driver; the HUD binds its marker to it. */
	UFUNCTION(BlueprintPure, Category = "MAL|LockOn")
	UMALLockOnComponent* GetLockOnComponent() const { return LockOnComponent; }

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
	 * Debug: equip a weapon by asset path, or unequip with an empty path.
	 *
	 * There is no pickup or inventory yet, so this is what makes the weapon ability layer
	 * reachable from a PIE console. Goes through the same server RPC a real equip would.
	 */
	UFUNCTION(Exec)
	void MALEQUIP(const FString& WeaponAssetPath);

	/**
	 * Debug: log where the player's view comes from and what the pawn's camera rig looks like.
	 *
	 * Prints the view point plus every owned component with its creation method and, for scene
	 * components, the attachment parent -- enough to tell a working native rig from a stray
	 * blueprint camera without opening the editor.
	 */
	UFUNCTION(Exec)
	void MALCAM();

	/**
	 * Client -> server: put Count banked points into a primary stat.
	 *
	 * The attribute panel lives on the client but the primaries are only writable on the
	 * authority, so a row's button has to cross the wire. Reliable: a dropped click would leave
	 * the row showing a value the server never agreed to.
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "MAL|Progression")
	void ServerSpendAttributePoint(EMALPrimaryStat Stat, int32 Count);

	/**
	 * Client -> server: buy one node of the skill tree.
	 *
	 * Reliable for the same reason the point spend is: a dropped click would leave the panel
	 * showing a skill the server never granted and never charged for.
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "MAL|Skill")
	void ServerUnlockSkill(FGameplayTag SkillTag);

	/**
	 * Client -> server: equip Weapon, or unequip when it is null.
	 *
	 * EquipWeapon refuses to run anywhere but the authority, and the decision to swap belongs to
	 * the player, so a weapon picked in a client-side UI has to cross the wire. Reliable for the
	 * same reason the other two are: a dropped swap leaves the client drawing a weapon the server
	 * never gave it, along with abilities to match.
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "MAL|Combat")
	void ServerEquipWeapon(UMALWeaponDefinition* Weapon);

	/**
	 * Client -> server: step Direction places through the loadout.
	 *
	 * Only the direction crosses the wire, not the resulting weapon: the authority owns the
	 * equipped slot, and a client picking the next one from its own replicated copy would pick
	 * the same slot twice while two swaps are in flight. Reliable for the same reason the other
	 * equip RPC is.
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "MAL|Combat")
	void ServerCycleWeapon(int32 Direction);

protected:
	//~ Begin APlayerController
	virtual void OnPossess(APawn* InPawn) override;
	virtual void AcknowledgePossession(APawn* InPawn) override;
	//~ End APlayerController

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

	/** Menu pushed by ToggleSkillMenu. Leave empty to disable the skill tree panel. */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI")
	TSubclassOf<UMALMenuWidget> SkillMenuClass;

	/**
	 * Widget spawned once per floating damage number.
	 * Leave empty to disable damage numbers entirely.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Damage")
	TSubclassOf<UMALDamageNumberWidget> DamageNumberWidgetClass;

private:
	/**
	 * Shared body of the menu keys: closes the panel when it is the one on top, otherwise pushes
	 * MenuClass. PanelType is the C++ class matched against the open panel rather than the
	 * configured blueprint, so swapping the asset cannot leave a panel this key no longer closes.
	 */
	void ToggleMenuOfType(TSubclassOf<UMALMenuWidget> MenuClass, UClass* PanelType);

	/** Watch this pawn's weapon component for equips; drops whatever was watched before. */
	void BindToWeaponComponent();

	/** Detach from the component last handed to BindToWeaponComponent. */
	void UnbindWeaponComponent();

	/** Re-resolve the ability bar from the specs the character holds right now. */
	void RefreshAbilityBar();

	/** Weapon component callback: a swap landed, so the bar's contents may have changed. */
	void HandleEquippedWeaponChanged();

	/** The component BindToWeaponComponent is listening to. Weak: the pawn can die under us. */
	TWeakObjectPtr<UMALWeaponComponent> BoundWeaponComponent;

	/** Pending delayed MALLOCK toggle; a handle cannot be a temporary for FTimerManager. */
	FTimerHandle DebugLockTimer;

	/** Pending delayed MALCOMBATINFO dump. */
	FTimerHandle DebugInfoTimer;

	FDelegateHandle WeaponChangedDelegateHandle;

	/** Local hit-stop driver for confirmed hits (camera shake stays on the controller). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MAL|Combat", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMALHitFeedbackComponent> HitFeedbackComponent;

	/** Camera-facing lock-on target; lives on the controller so a respawn keeps the rig. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MAL|LockOn", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMALLockOnComponent> LockOnComponent;
};
