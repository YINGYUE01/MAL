// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "MALComboComponent.generated.h"

/** The chain moved to a new step; the owning ability performs the montage jump. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FMALComboAdvancedSignature, int32 /*Step*/, FName /*Section*/);

/** RecoveryDelay elapsed without a new step; the owning ability should end itself. */
DECLARE_MULTICAST_DELEGATE(FMALComboRecoverySignature);

/** Asked for the step the montage is currently playing; see MontageStepQuery. */
DECLARE_DELEGATE_RetVal(int32, FMALComboStepQuery);

/**
 * UMALComboComponent
 *
 * Combo state machine for a melee attack: buffers an early press, advances when the montage's
 * combo window is open and arms the recovery release once the window is gone.
 *
 * Lives on AMALCharacterBase so the ability does not carry per-swing state across
 * activations. It deliberately does not touch the montage: advancing broadcasts the section
 * to jump to, and the ability applies it through MontageJumpToSection (which owns the
 * replication from clients). That keeps the component free of any GAS dependency.
 */
UCLASS(ClassGroup = (MAL), meta = (BlueprintSpawnableComponent))
class MAL_API UMALComboComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMALComboComponent();

	/**
	 * Start a fresh combo. The ability hands in its own tuning so the component stays
	 * agnostic of which ability is driving it.
	 */
	void BeginCombo(const TArray<FName>& InSections, float InInputBufferWindow, float InRecoveryDelay);

	/** End the combo (ability ended or cancelled); clears timers and state. */
	void EndCombo();

	/** Attack pressed while the ability is active: advance now, or buffer briefly. */
	void PressAttack();

	/** Montage notify: the window in which the next press chains is now open. */
	void OpenWindow();

	/** Montage notify: chaining is no longer possible. */
	void CloseWindow();

	/** Hit window closed: restart the recovery countdown with the configured delay. */
	void ScheduleRecovery();

	/** A new section started: a pending recovery release is no longer valid. */
	void CancelRecovery();

	UFUNCTION(BlueprintPure, Category = "MAL|Combo")
	int32 GetStep() const { return CurrentStep; }

	UFUNCTION(BlueprintPure, Category = "MAL|Combo")
	bool IsWindowOpen() const { return bWindowOpen; }

	/**
	 * Optional. Bound by the owning ability to the step derived from the montage's live section.
	 *
	 * The local counter is only a cache: this component and the server each advance their own,
	 * and a section jump that arrives from the other side can leave this copy behind. Chaining
	 * from a stale index would pick a section the montage already played, so Advance() prefers
	 * the montage's answer when one is available.
	 */
	FMALComboStepQuery MontageStepQuery;

	/** Fired when the chain advanced; the ability performs the montage jump. */
	FMALComboAdvancedSignature OnComboAdvanced;

	/** Fired after RecoveryDelay without a new step; the ability ends itself. */
	FMALComboRecoverySignature OnRecoveryExpired;

protected:
	//~ Begin UActorComponent
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent

private:
	/** Advance to the next section if one exists. */
	bool Advance();

	/** Consume a buffered press if the window is open and the press is still fresh. */
	void TryConsumeBufferedInput();

	void ClearRecoveryTimer();

	void HandleRecoveryExpired();

	/** Current step index (0-based into Sections). A cache; MontageStepQuery is the truth. */
	int32 CurrentStep = 0;

	/** True while the montage's combo window notify state is active. */
	bool bWindowOpen = false;

	/** True if attack was pressed before the window opened; consumed when it opens. */
	bool bInputBuffered = false;

	/** World time of the buffered press, used to expire stale inputs. */
	double BufferedAt = 0.0;

	float InputBufferWindow = 0.25f;

	float RecoveryDelay = 0.25f;

	/** Section names in order (e.g. Combo1..Combo4). */
	TArray<FName> Sections;

	FTimerHandle RecoveryTimer;
};
