// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Widgets/MALAttributeMenuWidget.h"

#include "AbilitySystem/Attributes/MALProgressionAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Player/MALPlayerController.h"
#include "UI/ViewModel/MALAttributeRowViewModel.h"

namespace
{
	/** Every spendable stat, in enum order, used when the blueprint leaves RowStats empty. */
	void AppendAllRowStats(TArray<EMALPrimaryStat>& OutStats)
	{
		for (int32 Index = 0; Index < static_cast<int32>(EMALPrimaryStat::Count); ++Index)
		{
			OutStats.Add(static_cast<EMALPrimaryStat>(Index));
		}
	}
}

UMALAttributeRowViewModel* UMALAttributeMenuWidget::GetRowViewModel(int32 Index) const
{
	return RowViewModels.IsValidIndex(Index) ? RowViewModels[Index] : nullptr;
}

void UMALAttributeMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Construct runs again every time the widget is re-added to the viewport. The row set only
	// depends on RowStats, which does not change at runtime, so one build is enough.
	if (RowViewModels.Num() == 0)
	{
		BuildRows();
	}
}

void UMALAttributeMenuWidget::NativeAbilitySystemReady(UAbilitySystemComponent* InAbilitySystemComponent)
{
	Super::NativeAbilitySystemReady(InAbilitySystemComponent);

	// The menu constructs while the HUD is still waiting for the PlayerState to replicate in,
	// so rows built then have no listeners yet. This is where they get them, and where a
	// re-possession rebinds them.
	for (UMALAttributeRowViewModel* RowViewModel : RowViewModels)
	{
		if (RowViewModel)
		{
			RowViewModel->SetAbilitySystemComponent(InAbilitySystemComponent);
		}
	}

	// SetAbilitySystemComponent primes each view model with the attribute's current value.
	// Push that out now so a row whose bindings are still missing is not stuck at zero.
	PublishRowValues();

	BindPointAvailability(InAbilitySystemComponent);
}

UPanelWidget* UMALAttributeMenuWidget::FindRowContainer() const
{
	return Cast<UPanelWidget>(GetWidgetFromName(RowContainerName));
}

void UMALAttributeMenuWidget::BuildRows()
{
	UPanelWidget* RowContainer = FindRowContainer();
	if (!RowContainer)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UMALAttributeMenuWidget: %s has no '%s' row container. The menu stays unconfigured."),
			*GetNameSafe(this), *RowContainerName.ToString());
		return;
	}

	TArray<EMALPrimaryStat> Stats = RowStats;
	if (Stats.Num() == 0)
	{
		AppendAllRowStats(Stats);
	}

	RowViewModels.SetNum(Stats.Num());
	RowAddPointButtons.SetNum(Stats.Num());

	// Counted instead of logged per row: a row blueprint without a source of this type is a
	// blueprint to finish, not eighteen separate problems.
	int32 RejectedRows = 0;

	const int32 RowCount = RowContainer->GetChildrenCount();
	for (int32 Index = 0; Index < RowCount; ++Index)
	{
		UMALUserWidget* Row = Cast<UMALUserWidget>(RowContainer->GetChildAt(Index));
		if (!Row)
		{
			continue;
		}

		// Keeping the extras out of the way rather than leaving a row on screen that follows no
		// stat: a panel is usually trimmed down before its stat list is filled out.
		if (!Stats.IsValidIndex(Index))
		{
			Row->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}

		Row->SetVisibility(ESlateVisibility::Visible);

		UMALAttributeRowViewModel* RowViewModel = NewObject<UMALAttributeRowViewModel>(this);
		RowViewModel->Initialize(Stats[Index]);
		RowViewModels[Index] = RowViewModel;

		// Not fatal when the row blueprint declares no source of this type yet: the setters
		// below still paint the label and value, only live updates are lost.
		if (!Row->SetViewModelByClass(RowViewModel))
		{
			++RejectedRows;
		}

		Row->SetDisplayName(RowViewModel->AttributeName);

		// Bound here rather than in the row blueprint: every row is one widget blueprint, so a
		// click wired in the graph would have no way to say which stat it belongs to. The per
		// row view model already carries that, and its handler is the UFUNCTION a dynamic
		// button delegate needs.
		//
		// Routed through the menu because this is the only object here with an owning player: a
		// client never sees its own controller through the PlayerState, whose owner does not
		// replicate, so a lookup from the view model would come back empty there.
		TWeakObjectPtr<UMALAttributeMenuWidget> WeakThis(this);
		RowViewModel->AddPointRequested = [WeakThis](EMALPrimaryStat Stat)
		{
			UMALAttributeMenuWidget* Menu = WeakThis.Get();
			if (!Menu)
			{
				return;
			}

			if (AMALPlayerController* PlayerController = Cast<AMALPlayerController>(Menu->GetOwningPlayer()))
			{
				PlayerController->ServerSpendAttributePoint(Stat, 1);
			}
		};

		UButton* AddPointButton = Cast<UButton>(Row->GetWidgetFromName(RowAddPointButtonName));
		RowAddPointButtons[Index] = AddPointButton;
		if (AddPointButton)
		{
			AddPointButton->OnClicked.AddDynamic(RowViewModel, &UMALAttributeRowViewModel::RequestAddPoint);
		}
	}

	if (RejectedRows > 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UMALAttributeMenuWidget: %d of %d rows did not accept a row view model. Declare a settable %s source in the row blueprint to get live values there."),
			RejectedRows, RowCount, *UMALAttributeRowViewModel::StaticClass()->GetName());
	}
}

void UMALAttributeMenuWidget::BindPointAvailability(UAbilitySystemComponent* InAbilitySystemComponent)
{
	if (!InAbilitySystemComponent)
	{
		return;
	}

	const FGameplayAttribute PointsAttribute = UMALProgressionAttributeSet::GetAttributePointsAttribute();

	// AddUObject rather than a lambda: the HUD outlives this menu, so the listener has to be
	// weakly bound to the widget. AddUnique semantics make a re-possession rebind harmless.
	InAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(PointsAttribute)
		.AddUObject(this, &UMALAttributeMenuWidget::HandleAttributePointsChanged);

	// The delegate only fires on change, so the opening state is read directly: a panel opened
	// with nothing banked would otherwise start out with live buttons.
	SetAddPointEnabled(InAbilitySystemComponent->GetNumericAttribute(PointsAttribute) >= 1.0f);
}

void UMALAttributeMenuWidget::HandleAttributePointsChanged(const FOnAttributeChangeData& ChangeData)
{
	SetAddPointEnabled(ChangeData.NewValue >= 1.0f);
}

void UMALAttributeMenuWidget::SetAddPointEnabled(bool bEnabled)
{
	for (UButton* AddPointButton : RowAddPointButtons)
	{
		if (AddPointButton)
		{
			AddPointButton->SetIsEnabled(bEnabled);
		}
	}
}

void UMALAttributeMenuWidget::PublishRowValues()
{
	UPanelWidget* RowContainer = FindRowContainer();
	if (!RowContainer)
	{
		return;
	}

	const int32 RowCount = FMath::Min(RowContainer->GetChildrenCount(), RowViewModels.Num());
	for (int32 Index = 0; Index < RowCount; ++Index)
	{
		UMALUserWidget* Row = Cast<UMALUserWidget>(RowContainer->GetChildAt(Index));
		const UMALAttributeRowViewModel* RowViewModel = RowViewModels[Index];
		if (!Row || !RowViewModel)
		{
			continue;
		}

		Row->SetDisplayName(RowViewModel->AttributeName);
		Row->SetDisplayValue(RowViewModel->DisplayValue);
	}
}
