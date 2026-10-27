// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Widgets/MALAttributeMenuWidget.h"

#include "AbilitySystem/Attributes/MALProgressionAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Player/MALPlayerController.h"
#include "UI/ViewModel/MALAttributeRowViewModel.h"
#include "UI/Widgets/MALAttributeRowWidget.h"

namespace
{
	/** Every spendable stat, in enum order, used when the blueprint leaves RowStats empty. */
	void AppendAllPrimaryRowStats(TArray<EMALPrimaryStat>& OutStats)
	{
		for (int32 Index = 0; Index < static_cast<int32>(EMALPrimaryStat::Count); ++Index)
		{
			OutStats.Add(static_cast<EMALPrimaryStat>(Index));
		}
	}

	/** Same fallback for the derived list: an empty SecondaryRowStats means show them all. */
	void AppendAllSecondaryRowStats(TArray<EMALSecondaryStat>& OutStats)
	{
		for (int32 Index = 0; Index < static_cast<int32>(EMALSecondaryStat::Count); ++Index)
		{
			OutStats.Add(static_cast<EMALSecondaryStat>(Index));
		}
	}
}

UMALAttributeMenuWidget::UMALAttributeMenuWidget()
{
	// Spelled out rather than left empty: the title is the only thing telling the two pages
	// apart once the switcher has flipped, and a blank one would read as a broken panel.
	PrimaryPageTitle = NSLOCTEXT("MALAttributeMenu", "PrimaryPageTitle", "Primary Attributes");
	SecondaryPageTitle = NSLOCTEXT("MALAttributeMenu", "SecondaryPageTitle", "Secondary Attributes");
}

UMALAttributeRowViewModel* UMALAttributeMenuWidget::GetPrimaryRowViewModel(int32 Index) const
{
	return PrimaryRowViewModels.IsValidIndex(Index) ? PrimaryRowViewModels[Index] : nullptr;
}

UMALAttributeRowViewModel* UMALAttributeMenuWidget::GetSecondaryRowViewModel(int32 Index) const
{
	return SecondaryRowViewModels.IsValidIndex(Index) ? SecondaryRowViewModels[Index] : nullptr;
}

void UMALAttributeMenuWidget::ShowPrimaryStats()
{
	bShowingSecondaryStats = false;
	ApplyStatsPage();
}

void UMALAttributeMenuWidget::ShowSecondaryStats()
{
	bShowingSecondaryStats = true;
	ApplyStatsPage();
}

void UMALAttributeMenuWidget::ShowNextStatsPage()
{
	bShowingSecondaryStats = !bShowingSecondaryStats;
	ApplyStatsPage();
}

void UMALAttributeMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Construct runs again every time the widget is re-added to the viewport. The row set only
	// depends on the two stat arrays and RowWidgetClass, which do not change at runtime, so one
	// build is enough; a build that found the blueprint unconfigured is retried next time.
	if (!bRowsBuilt)
	{
		bRowsBuilt = BuildRows();
	}

	// Also re-run on every construct: the handlers are idempotent and the page index may have
	// been changed from a blueprint before the widget came back on screen.
	BindPageSwitcher();
	ApplyStatsPage();
}

void UMALAttributeMenuWidget::NativeAbilitySystemReady(UAbilitySystemComponent* InAbilitySystemComponent)
{
	Super::NativeAbilitySystemReady(InAbilitySystemComponent);

	// The menu constructs while the HUD is still waiting for the PlayerState to replicate in,
	// so rows built then have no listeners yet. This is where they get them, and where a
	// re-possession rebinds them.
	for (UMALAttributeRowViewModel* RowViewModel : PrimaryRowViewModels)
	{
		if (RowViewModel)
		{
			RowViewModel->SetAbilitySystemComponent(InAbilitySystemComponent);
		}
	}

	// The derived page is built at the same time and listens the same way: its attributes are
	// rewritten by the secondary effect whenever a point lands on a primary.
	for (UMALAttributeRowViewModel* RowViewModel : SecondaryRowViewModels)
	{
		if (RowViewModel)
		{
			RowViewModel->SetAbilitySystemComponent(InAbilitySystemComponent);
		}
	}

	BindPointAvailability(InAbilitySystemComponent);
}

void UMALAttributeMenuWidget::RefreshPointsLeft(float Points)
{
	// Rounded rather than truncated: a reading of 0.999 is still a point the player can spend.
	if (PointsLeftText)
	{
		PointsLeftText->SetText(FText::AsNumber(FMath::RoundToInt(Points)));
	}
}

void UMALAttributeMenuWidget::ApplyStatsPage()
{
	if (StatsSwitcher)
	{
		StatsSwitcher->SetActiveWidgetIndex(bShowingSecondaryStats ? 1 : 0);
	}

	// Followed here rather than bound in the blueprint: the title has to change exactly when the
	// switcher does, and the page index lives on this class.
	if (PageTitleText)
	{
		PageTitleText->SetText(bShowingSecondaryStats ? SecondaryPageTitle : PrimaryPageTitle);
	}
}

void UMALAttributeMenuWidget::BindPageSwitcher()
{
	// AddUnique semantics are the point: Construct runs on every add-to-viewport, and a plain
	// AddDynamic would stack one more handler per visit.
	if (PreviousPageButton)
	{
		PreviousPageButton->OnClicked.AddUniqueDynamic(this, &UMALAttributeMenuWidget::ShowPrimaryStats);
	}

	if (NextPageButton)
	{
		NextPageButton->OnClicked.AddUniqueDynamic(this, &UMALAttributeMenuWidget::ShowSecondaryStats);
	}
}

bool UMALAttributeMenuWidget::BuildRows()
{
	if (!PrimaryRowsBox && !SecondaryRowsBox)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UMALAttributeMenuWidget: %s binds neither PrimaryRowsBox nor SecondaryRowsBox. The menu stays unconfigured."),
			*GetNameSafe(this));
		return false;
	}

	if (!RowWidgetClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UMALAttributeMenuWidget: %s has no RowWidgetClass. Assign the row widget blueprint to build either page."),
			*GetNameSafe(this));
		return false;
	}

	// Counted instead of logged per row: a row blueprint without a source of this type is a
	// blueprint to finish, not nineteen separate problems.
	int32 RejectedRows = 0;

	TArray<EMALPrimaryStat> PrimaryStats = RowStats;
	if (PrimaryStats.Num() == 0)
	{
		AppendAllPrimaryRowStats(PrimaryStats);
	}

	RejectedRows += BuildPage(PrimaryRowsBox, PrimaryStats.Num(),
		[this, &PrimaryStats](int32 Index)
		{
			UMALAttributeRowViewModel* RowViewModel = NewObject<UMALAttributeRowViewModel>(this);
			RowViewModel->InitializePrimary(PrimaryStats[Index]);

			// Routed through the menu because this is the only object here with an owning
			// player: a client never sees its own controller through the PlayerState, whose
			// owner does not replicate, so a lookup from the view model would come back empty
			// there.
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

			return RowViewModel;
		},
		PrimaryRowViewModels,
		/*bSpendable*/ true);

	TArray<EMALSecondaryStat> SecondaryStats = SecondaryRowStats;
	if (SecondaryStats.Num() == 0)
	{
		AppendAllSecondaryRowStats(SecondaryStats);
	}

	RejectedRows += BuildPage(SecondaryRowsBox, SecondaryStats.Num(),
		[this, &SecondaryStats](int32 Index)
		{
			UMALAttributeRowViewModel* RowViewModel = NewObject<UMALAttributeRowViewModel>(this);
			// A derived row ends here: no spend callback to give it, and the row collapses its
			// add button because the view model reports that it cannot spend.
			RowViewModel->InitializeSecondary(SecondaryStats[Index]);
			return RowViewModel;
		},
		SecondaryRowViewModels,
		/*bSpendable*/ false);

	if (RejectedRows > 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UMALAttributeMenuWidget: %d rows did not accept a row view model. Declare a settable %s source in %s to get live values there."),
			RejectedRows, *UMALAttributeRowViewModel::StaticClass()->GetName(), *GetNameSafe(RowWidgetClass));
	}

	return true;
}

int32 UMALAttributeMenuWidget::BuildPage(UScrollBox* Container,
	int32 StatCount,
	TFunctionRef<UMALAttributeRowViewModel*(int32 Index)> MakeRowViewModel,
	TArray<TObjectPtr<UMALAttributeRowViewModel>>& OutViewModels,
	bool bSpendable)
{
	OutViewModels.Reset();
	if (bSpendable)
	{
		SpendableRows.Reset();
	}

	if (!Container)
	{
		return 0;
	}

	// Rows are owned by the panel, not by the blueprint: whatever a page was authored with is
	// replaced by the stat list this class was configured with, so the two can never disagree
	// on how many rows there are.
	Container->ClearChildren();

	int32 RejectedRows = 0;
	for (int32 Index = 0; Index < StatCount; ++Index)
	{
		UMALAttributeRowViewModel* RowViewModel = MakeRowViewModel(Index);
		if (!RowViewModel)
		{
			continue;
		}

		UMALAttributeRowWidget* Row = CreateWidget<UMALAttributeRowWidget>(this, RowWidgetClass);
		if (!Row)
		{
			// BuildRows checks RowWidgetClass before this point; a failure here is a class that
			// is not a widget and would fail for every index, so there is nothing to retry.
			break;
		}

		Container->AddChild(Row);

		// The slot only exists once the child is in the panel. Fill horizontally so a row
		// stretches with the panel instead of sitting at its authored width.
		if (UScrollBoxSlot* RowSlot = Cast<UScrollBoxSlot>(Row->Slot))
		{
			RowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, RowSpacing));
			RowSlot->SetHorizontalAlignment(HAlign_Fill);
		}

		if (!Row->SetRowViewModel(RowViewModel))
		{
			++RejectedRows;
		}

		OutViewModels.Add(RowViewModel);

		if (bSpendable)
		{
			SpendableRows.Add(Row);
		}
	}

	return RejectedRows;
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
	const float PointsLeft = InAbilitySystemComponent->GetNumericAttribute(PointsAttribute);
	SetAddPointEnabled(PointsLeft >= 1.0f);
	RefreshPointsLeft(PointsLeft);
}

void UMALAttributeMenuWidget::HandleAttributePointsChanged(const FOnAttributeChangeData& ChangeData)
{
	SetAddPointEnabled(ChangeData.NewValue >= 1.0f);
	RefreshPointsLeft(ChangeData.NewValue);
}

void UMALAttributeMenuWidget::SetAddPointEnabled(bool bEnabled)
{
	for (UMALAttributeRowWidget* Row : SpendableRows)
	{
		if (Row)
		{
			Row->SetAddPointEnabled(bEnabled);
		}
	}
}
