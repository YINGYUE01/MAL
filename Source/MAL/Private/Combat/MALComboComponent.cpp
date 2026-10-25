// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/MALComboComponent.h"

#include "Engine/World.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MALComboComponent)

UMALComboComponent::UMALComboComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// Pure state machine driven by the ability's input and the montage notifies; nothing
	// here needs to replicate (the ability's montage jump already does).
	SetIsReplicatedByDefault(false);
}

void UMALComboComponent::BeginCombo(const TArray<FName>& InSections, float InInputBufferWindow, float InRecoveryDelay)
{
	Sections = InSections;
	InputBufferWindow = FMath::Max(0.0f, InInputBufferWindow);
	RecoveryDelay = FMath::Max(0.0f, InRecoveryDelay);

	ClearRecoveryTimer();

	CurrentStep = 0;
	bWindowOpen = false;
	bInputBuffered = false;
	BufferedAt = 0.0;
}

void UMALComboComponent::EndCombo()
{
	ClearRecoveryTimer();

	CurrentStep = 0;
	bWindowOpen = false;
	bInputBuffered = false;
	BufferedAt = 0.0;
}

void UMALComboComponent::PressAttack()
{
	if (bWindowOpen)
	{
		Advance();
		return;
	}

	// Pressed outside the window: buffer it briefly and consume when the next window opens.
	bInputBuffered = true;
	BufferedAt = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

void UMALComboComponent::OpenWindow()
{
	bWindowOpen = true;

	// A new window means a section still owns the ability; a release armed by the previous
	// section must not end it mid-swing.
	ClearRecoveryTimer();

	// If the player pressed attack before the window opened, chain immediately.
	TryConsumeBufferedInput();
}

void UMALComboComponent::CloseWindow()
{
	if (!bWindowOpen)
	{
		// Advance() already closed the window. The trailing NotifyEnd of the section we jumped
		// away from still fires afterwards; arming a release here would cut the new section
		// short before its hit window even opens.
		return;
	}

	bWindowOpen = false;

	// Past this point the chain can no longer be extended, so the rest of the montage is pure
	// recovery. Arm the release here rather than on montage end, so the ability lets go of movement
	// and the next attack on the frame the player stops being able to act.
	ScheduleRecovery();
}

void UMALComboComponent::ScheduleRecovery()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Re-arm cleanly: one montage can open and close several hit windows.
	ClearRecoveryTimer();

	if (RecoveryDelay <= 0.0f)
	{
		HandleRecoveryExpired();
		return;
	}

	World->GetTimerManager().SetTimer(RecoveryTimer, this, &UMALComboComponent::HandleRecoveryExpired, RecoveryDelay, false);
}

void UMALComboComponent::CancelRecovery()
{
	ClearRecoveryTimer();
}

void UMALComboComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearRecoveryTimer();

	Super::EndPlay(EndPlayReason);
}

bool UMALComboComponent::Advance()
{
	// Prefer the montage's step over the local counter. Both sides predict independently, so a
	// section jump coming from the other side can arrive after this copy already counted past it;
	// chaining from the stale index would then jump the montage backwards.
	const int32 MontageStep = MontageStepQuery.IsBound() ? MontageStepQuery.Execute() : 0;
	const int32 BaseStep = FMath::Max(CurrentStep, MontageStep);

	// No further section -> stay on the current one (it flows into Recovery and ends).
	if (!Sections.IsValidIndex(BaseStep + 1))
	{
		bInputBuffered = false;
		BufferedAt = 0.0;
		return false;
	}

	const int32 NewStep = BaseStep + 1;
	const FName Section = Sections[NewStep];

	CurrentStep = NewStep;
	bInputBuffered = false;
	bWindowOpen = false; // closed until the next section's window notify reopens it
	BufferedAt = 0.0;

	// A new section invalidates the release armed by the previous one.
	ClearRecoveryTimer();

	OnComboAdvanced.Broadcast(CurrentStep, Section);
	return true;
}

void UMALComboComponent::TryConsumeBufferedInput()
{
	if (!bInputBuffered)
	{
		return;
	}

	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : BufferedAt;
	if (Now - BufferedAt > static_cast<double>(InputBufferWindow))
	{
		// Stale press: don't let an input from several seconds ago auto-chain.
		bInputBuffered = false;
		return;
	}

	bInputBuffered = false;
	Advance();
}

void UMALComboComponent::ClearRecoveryTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RecoveryTimer);
	}
}

void UMALComboComponent::HandleRecoveryExpired()
{
	OnRecoveryExpired.Broadcast();
}
