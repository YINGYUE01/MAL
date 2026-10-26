// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Widgets/MALAttributeMenuWidget.h"

#include "AbilitySystem/Attributes/MALProgressionAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Player/MALPlayerController.h"
#include "UI/ViewModel/MALAttributeRowViewModel.h"

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
	// depends on the two stat arrays, which do not change at runtime, so one build is enough; a
	// build that found no container is retried on the next construct.
	if (!bRowsBuilt)
	{
		bRowsBuilt = BuildRows();
	}

	ResolvePointsLeftText();
	BindPageSwitcher();
	ApplyStatsPage();

	// Repainted on every add-to-viewport: re-adding rebuilds the child widgets, and a row whose
	// view binding re-evaluates against the widget's own view model instance would come back
	// blank. What the view models already hold is the current reading, so pushing it out again
	// costs one text write per row and saves the row from showing nothing.
	PublishRowValues();
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

	// SetAbilitySystemComponent primes each view model with the attribute's current value.
	// Push that out now so a row whose bindings are still missing is not stuck at zero.
	PublishRowValues();

	BindPointAvailability(InAbilitySystemComponent);
}

UPanelWidget* UMALAttributeMenuWidget::FindRowContainer(FName ContainerName) const
{
	return Cast<UPanelWidget>(GetWidgetFromName(ContainerName));
}

UWidgetSwitcher* UMALAttributeMenuWidget::FindStatsSwitcher() const
{
	return Cast<UWidgetSwitcher>(GetWidgetFromName(StatsSwitcherName));
}

void UMALAttributeMenuWidget::ApplyStatsPage()
{
	if (UWidgetSwitcher* Switcher = FindStatsSwitcher())
	{
		Switcher->SetActiveWidgetIndex(bShowingSecondaryStats ? 1 : 0);
	}

	// Followed here rather than bound in the blueprint: the title has to change exactly when the
	// switcher does, and both are named on this class already.
	if (UTextBlock* Title = Cast<UTextBlock>(GetWidgetFromName(PageTitleTextName)))
	{
		Title->SetText(bShowingSecondaryStats ? SecondaryPageTitle : PrimaryPageTitle);
	}
}

void UMALAttributeMenuWidget::BindPageSwitcher()
{
	// AddDynamic is unique underneath, so binding again on every construct is harmless.
	if (UButton* PreviousButton = Cast<UButton>(GetWidgetFromName(PreviousPageButtonName)))
	{
		PreviousButton->OnClicked.AddDynamic(this, &UMALAttributeMenuWidget::ShowPrimaryStats);
	}

	if (UButton* NextButton = Cast<UButton>(GetWidgetFromName(NextPageButtonName)))
	{
		NextButton->OnClicked.AddDynamic(this, &UMALAttributeMenuWidget::ShowSecondaryStats);
	}
}

bool UMALAttributeMenuWidget::BuildRows()
{
	UPanelWidget* PrimaryContainer = FindRowContainer(RowContainerName);
	UPanelWidget* SecondaryContainer = FindRowContainer(SecondaryRowContainerName);

	if (!PrimaryContainer && !SecondaryContainer)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UMALAttributeMenuWidget: %s has neither '%s' nor '%s'. The menu stays unconfigured."),
			*GetNameSafe(this), *RowContainerName.ToString(), *SecondaryRowContainerName.ToString());
		return false;
	}

	// Counted instead of logged per row: a row blueprint without a source of this type is a
	// blueprint to finish, not nineteen separate problems.
	int32 RejectedRows = 0;
	if (PrimaryContainer)
	{
		RejectedRows += BuildPrimaryRows(PrimaryContainer);
	}
	if (SecondaryContainer)
	{
		RejectedRows += BuildSecondaryRows(SecondaryContainer);
	}

	if (RejectedRows > 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UMALAttributeMenuWidget: %d rows did not accept a row view model. Declare a settable %s source in the row blueprint to get live values there."),
			RejectedRows, *UMALAttributeRowViewModel::StaticClass()->GetName());
	}

	return true;
}

void UMALAttributeMenuWidget::ResolvePointsLeftText()
{
	PointsLeftText = Cast<UTextBlock>(GetWidgetFromName(PointsLeftTextName));
}

void UMALAttributeMenuWidget::RefreshPointsLeft(float Points)
{
	// Rounded rather than truncated: a reading of 0.999 is still a point the player can spend.
	if (PointsLeftText)
	{
		PointsLeftText->SetText(FText::AsNumber(FMath::RoundToInt(Points)));
	}
}

int32 UMALAttributeMenuWidget::BuildPrimaryRows(UPanelWidget* Container)
{
	TArray<EMALPrimaryStat> Stats = RowStats;
	if (Stats.Num() == 0)
	{
		AppendAllPrimaryRowStats(Stats);
	}

	const int32 RowCount = Container->GetChildrenCount();
	PrimaryRowViewModels.Reset();
	PrimaryRowViewModels.SetNum(RowCount);

	int32 RejectedRows = 0;
	for (int32 Index = 0; Index < RowCount; ++Index)
	{
		UMALUserWidget* Row = Cast<UMALUserWidget>(Container->GetChildAt(Index));
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
		RowViewModel->InitializePrimary(Stats[Index]);
		PrimaryRowViewModels[Index] = RowViewModel;

		if (!InitializeRow(Row, RowViewModel))
		{
			++RejectedRows;
		}

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

		// Bound here rather than in the row blueprint: every row is one widget blueprint, so a
		// click wired in the graph would have no way to say which stat it belongs to. The per
		// row view model already carries that, and its handler is the UFUNCTION a dynamic
		// button delegate needs.
		if (UButton* AddPointButton = Cast<UButton>(Row->GetWidgetFromName(RowAddPointButtonName)))
		{
			AddPointButton->OnClicked.AddDynamic(RowViewModel, &UMALAttributeRowViewModel::RequestAddPoint);
			RowAddPointButtons.Add(AddPointButton);
		}
	}

	return RejectedRows;
}

int32 UMALAttributeMenuWidget::BuildSecondaryRows(UPanelWidget* Container)
{
	TArray<EMALSecondaryStat> Stats = SecondaryRowStats;
	if (Stats.Num() == 0)
	{
		AppendAllSecondaryRowStats(Stats);
	}

	const int32 RowCount = Container->GetChildrenCount();
	SecondaryRowViewModels.Reset();
	SecondaryRowViewModels.SetNum(RowCount);

	int32 RejectedRows = 0;
	for (int32 Index = 0; Index < RowCount; ++Index)
	{
		UMALUserWidget* Row = Cast<UMALUserWidget>(Container->GetChildAt(Index));
		if (!Row)
		{
			continue;
		}

		if (!Stats.IsValidIndex(Index))
		{
			Row->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}

		Row->SetVisibility(ESlateVisibility::Visible);

		UMALAttributeRowViewModel* RowViewModel = NewObject<UMALAttributeRowViewModel>(this);
		RowViewModel->InitializeSecondary(Stats[Index]);
		SecondaryRowViewModels[Index] = RowViewModel;

		// A derived row ends here: no button to wire, and no spend callback to give it.
		if (!InitializeRow(Row, RowViewModel))
		{
			++RejectedRows;
		}
	}

	return RejectedRows;
}

bool UMALAttributeMenuWidget::InitializeRow(UMALUserWidget* Row, UMALAttributeRowViewModel* RowViewModel)
{
	// Not fatal when the row blueprint declares no source of this type yet: the setters below
	// still paint the label and value, only the blueprint binding stays dead.
	const bool bAccepted = Row->SetViewModelByClass(RowViewModel);

	Row->SetDisplayName(RowViewModel->AttributeName);
	Row->SetDisplayValue(RowViewModel->DisplayValue);

	// The row blueprint declares its view model source as one the widget creates for itself,
	// and a source like that cannot be replaced at runtime, so the binding on the row keeps
	// reading the widget's own instance instead of this one. Painting the text block from
	// here is what keeps the row live; the binding stays for a blueprint that switches its
	// source to a manual one.
	TWeakObjectPtr<UMALUserWidget> WeakRow(Row);
	RowViewModel->ValueChanged = [WeakRow](const FText& Value)
	{
		if (UMALUserWidget* RowWidget = WeakRow.Get())
		{
			RowWidget->SetDisplayValue(Value);
		}
	};

	return bAccepted;
}

void UMALAttributeMenuWidget::PublishRowValues()
{
	PublishRowsInContainer(FindRowContainer(RowContainerName), PrimaryRowViewModels);
	PublishRowsInContainer(FindRowContainer(SecondaryRowContainerName), SecondaryRowViewModels);
}

void UMALAttributeMenuWidget::PublishRowsInContainer(UPanelWidget* Container,
	const TArray<TObjectPtr<UMALAttributeRowViewModel>>& RowViewModels)
{
	if (!Container)
	{
		return;
	}

	const int32 RowCount = FMath::Min(Container->GetChildrenCount(), RowViewModels.Num());
	for (int32 Index = 0; Index < RowCount; ++Index)
	{
		UMALUserWidget* Row = Cast<UMALUserWidget>(Container->GetChildAt(Index));
		const UMALAttributeRowViewModel* RowViewModel = RowViewModels[Index];
		if (!Row || !RowViewModel)
		{
			continue;
		}

		Row->SetDisplayName(RowViewModel->AttributeName);
		Row->SetDisplayValue(RowViewModel->DisplayValue);
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
	for (UButton* AddPointButton : RowAddPointButtons)
	{
		if (AddPointButton)
		{
			AddPointButton->SetIsEnabled(bEnabled);
		}
	}
}
