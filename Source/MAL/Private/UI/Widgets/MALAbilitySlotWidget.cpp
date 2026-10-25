// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Widgets/MALAbilitySlotWidget.h"

#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "UI/Framework/MALUISubsystem.h"
#include "UI/ViewModel/MALAbilityBarViewModel.h"
#include "UI/ViewModel/MALAbilitySlotViewModel.h"
#include "UI/Widgets/MALHUDWidget.h"

void UMALAbilitySlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	TryResolveSlotViewModel();
}

void UMALAbilitySlotWidget::NativeDestruct()
{
	StopResolveRetryTimer();
	ReleaseSlotViewModel();

	Super::NativeDestruct();
}

void UMALAbilitySlotWidget::TryResolveSlotViewModel()
{
	UMALAbilityBarViewModel* BarViewModel = nullptr;

	if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
	{
		if (UMALUISubsystem* UISubsystem = LocalPlayer->GetSubsystem<UMALUISubsystem>())
		{
			if (UMALHUDWidget* HUD = UISubsystem->GetHUD())
			{
				BarViewModel = HUD->GetAbilityBarViewModel();
			}
		}
	}

	if (!BarViewModel || !BarViewModel->Slots.IsValidIndex(SlotIndex))
	{
		// The HUD builds its bar during InitializeForPlayer, which can land after this widget is
		// constructed. Poll instead of binding once and staying permanently empty.
		if (!ResolveRetryTimer.IsValid())
		{
			UWorld* World = GetWorld();
			if (World)
			{
				World->GetTimerManager().SetTimer(
					ResolveRetryTimer,
					this,
					&UMALAbilitySlotWidget::TryResolveSlotViewModel,
					FMath::Max(ResolveRetryInterval, 0.02f),
					true);
			}
		}

		return;
	}

	StopResolveRetryTimer();

	UMALAbilitySlotViewModel* Resolved = BarViewModel->Slots[SlotIndex].Get();
	if (Resolved == SlotViewModel)
	{
		return;
	}

	ReleaseSlotViewModel();

	SlotViewModel = Resolved;
	if (SlotViewModel)
	{
		SlotViewModel->OnSlotRefreshed.AddUObject(this, &UMALAbilitySlotWidget::RefreshFromViewModel);
		RefreshFromViewModel();
	}

	// Push it into MVVM as well, so a view binding authored in the slot blueprint keeps firing.
	SetViewModelByClass(SlotViewModel.Get());
}

void UMALAbilitySlotWidget::StopResolveRetryTimer()
{
	if (ResolveRetryTimer.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(ResolveRetryTimer);
		}
		ResolveRetryTimer.Invalidate();
	}
}

void UMALAbilitySlotWidget::ReleaseSlotViewModel()
{
	if (SlotViewModel)
	{
		SlotViewModel->OnSlotRefreshed.RemoveAll(this);
	}
	SlotViewModel = nullptr;
}

void UMALAbilitySlotWidget::RefreshFromViewModel()
{
	if (!SlotViewModel)
	{
		return;
	}

	if (NameText)
	{
		// What the HUD authored for this key wins over whatever the ability calls itself.
		NameText->SetText(DisplayNameOverride.IsEmpty() ? SlotViewModel->DisplayName : DisplayNameOverride);
	}

	if (FrameImage)
	{
		// An ungranted key reads as recessed rather than lit, so it does not look pressable.
		FrameImage->SetColorAndOpacity(FLinearColor(0.03f, 0.03f, 0.03f, SlotViewModel->bHasAbility ? 0.9f : 0.55f));
	}

	if (IconImage)
	{
		// The override is checked first so a HUD can skin a key on its own; the ability icon is
		// the fallback, which keeps a finished asset wired without any per-slot setup.
		UTexture2D* DesiredIcon = IconOverride ? IconOverride.Get() : SlotViewModel->Icon.Get();

		if (DesiredIcon && LastIcon.Get() != DesiredIcon)
		{
			// Uploading the brush is not free and this runs on the cooldown tick, so only touch
			// it when the icon actually changed.
			IconImage->SetBrushFromTexture(DesiredIcon, false);
			LastIcon = DesiredIcon;
		}

		if (DesiredIcon)
		{
			IconImage->SetColorAndOpacity(IconTint);
		}
		else
		{
			// No artwork anywhere: a flat placeholder still reads as a key rather than a hole.
			const float Placeholder = SlotViewModel->bHasAbility ? 0.55f : 0.15f;
			IconImage->SetColorAndOpacity(FLinearColor(Placeholder, Placeholder, Placeholder, 1.0f));
		}

		// Dimmed while the cost cannot be paid, so the bar explains a refused press before it happens.
		IconImage->SetRenderOpacity(SlotViewModel->bCanAfford ? 1.0f : 0.45f);
	}

	if (CooldownOverlay)
	{
		// The sweep is a material parameter, so a ready slot fades the overlay out instead of
		// hiding it: toggling visibility would restart the material every cooldown.
		CooldownOverlay->SetRenderOpacity(SlotViewModel->bIsOnCooldown ? 1.0f : 0.0f);

		if (UMaterialInstanceDynamic* CooldownMaterial = CooldownOverlay->GetDynamicMaterial())
		{
			CooldownMaterial->SetScalarParameterValue(FName(TEXT("Progress")), SlotViewModel->CooldownPercent);
		}
	}

	if (CooldownProgress)
	{
		// Faded out while ready: an empty trough under every key would read as a permanent border.
		CooldownProgress->SetRenderOpacity(SlotViewModel->bIsOnCooldown ? 1.0f : 0.0f);

		// Same 1.0 -> 0.0 ratio as the sweep, so the bar and the wedge can never drift apart.
		CooldownProgress->SetPercent(SlotViewModel->CooldownPercent);
	}

	if (CooldownText)
	{
		// Tenths instead of whole seconds: a 1.2s cooldown now steps 1.2, 1.1, 1.0 instead of
		// holding on a rounded number for most of its duration.
		FText Label;
		if (SlotViewModel->bIsOnCooldown && SlotViewModel->CooldownRemaining > 0.0f)
		{
			static const FNumberFormattingOptions CountdownFormat = []
			{
				FNumberFormattingOptions Options;
				Options.MinimumFractionalDigits = 1;
				Options.MaximumFractionalDigits = 1;
				// Rounds up, so the readout never sits on 0.0 while the ability is still cooling.
				Options.RoundingMode = ERoundingMode::ToPositiveInfinity;
				return Options;
			}();

			Label = FText::AsNumber(SlotViewModel->CooldownRemaining, &CountdownFormat);
		}

		// This runs every frame but the digits only change ten times a second, so skip the FText
		// rebuild unless the rendered string actually differs.
		if (!Label.EqualTo(LastCooldownLabel))
		{
			LastCooldownLabel = Label;
			CooldownText->SetText(Label);
		}
	}

	// Last, so a slot blueprint overrides these defaults instead of being overwritten by them.
	OnSlotUpdated();
}

void UMALAbilitySlotWidget::SetSlotIcon(UTexture2D* InIcon)
{
	IconOverride = InIcon;
	RefreshFromViewModel();
}

void UMALAbilitySlotWidget::SetSlotDisplayName(const FText& InName)
{
	DisplayNameOverride = InName;
	RefreshFromViewModel();
}

UImage* UMALAbilitySlotWidget::GetIconImage() const
{
	return IconImage;
}

UMALAbilitySlotViewModel* UMALAbilitySlotWidget::GetSlotViewModel() const
{
	return SlotViewModel;
}
