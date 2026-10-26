// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Attributes/MALPrimaryStat.h"
#include "AbilitySystem/Attributes/MALSecondaryStat.h"
#include "UI/Widgets/MALMenuWidget.h"
#include "MALAttributeMenuWidget.generated.h"

class UButton;
class UMALAttributeRowViewModel;
class UPanelWidget;
class UTextBlock;
class UWidgetSwitcher;
struct FOnAttributeChangeData;

/**
 * UMALAttributeMenuWidget
 *
 * The stat list, as two pages of one switcher: the spendable stats from RowStats on the first
 * page, the derived ones from SecondaryRowStats on the second. Only one page is on screen at a
 * time; ShowPrimaryStats and ShowSecondaryStats, or the arrow buttons, flip between them.
 *
 * The page a row belongs to is decided by the container it sits in: RowContainerName for the
 * spendable page, SecondaryRowContainerName for the derived one. Within the spendable page the
 * row blueprint is expected to carry an add button; that button is what gets wired for spending,
 * and the derived row blueprint is the same layout without it, so the panel never has to know
 * either class name.
 *
 * Owns one row view model per row. The rows all share a single widget blueprint within each
 * page, so a shared source would print the same stat in every one of them; giving each row its
 * own instance is what lets one blueprint draw five different primaries and another draw the
 * fourteen derived stats.
 *
 * Both pages are found through StatsSwitcherName, then through the two container names inside it.
 *
 * A menu rather than a plain widget: the UI subsystem owns the stack, so this panel is opened
 * and closed through PushMenu and PopMenu and never needs to know how it got on screen. It must
 * not pause the game -- a paused world stops ticking enhanced input, and the very key that
 * opened the panel would then never arrive to close it again.
 */
UCLASS(Abstract)
class MAL_API UMALAttributeMenuWidget : public UMALMenuWidget
{
	GENERATED_BODY()

public:
	/** View model at Index of the spendable page, indexing that page's container. */
	UFUNCTION(BlueprintPure, Category = "MAL|UI|Attribute")
	UMALAttributeRowViewModel* GetPrimaryRowViewModel(int32 Index) const;

	/** Same for the derived page. */
	UFUNCTION(BlueprintPure, Category = "MAL|UI|Attribute")
	UMALAttributeRowViewModel* GetSecondaryRowViewModel(int32 Index) const;

	/** True while the derived page is the one on screen. */
	UFUNCTION(BlueprintPure, Category = "MAL|UI|Attribute")
	bool IsShowingSecondaryStats() const { return bShowingSecondaryStats; }

	/** Page to the spendable stats. Harmless while already there. */
	UFUNCTION(BlueprintCallable, Category = "MAL|UI|Attribute")
	void ShowPrimaryStats();

	/** Page to the derived stats. */
	UFUNCTION(BlueprintCallable, Category = "MAL|UI|Attribute")
	void ShowSecondaryStats();

	/** Page to the other one, for a single button that cycles. */
	UFUNCTION(BlueprintCallable, Category = "MAL|UI|Attribute")
	void ShowNextStatsPage();

protected:
	/**
	 * Spendable stats drawn top to bottom. Leave empty to get the five primaries, which is what
	 * an unconfigured blueprint wants.
	 *
	 * A list shorter than the blueprint's spendable row count is normal while a panel is being
	 * trimmed: the extra rows are collapsed instead of left printing a label with no stat
	 * behind it.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Attribute")
	TArray<EMALPrimaryStat> RowStats;

	/**
	 * Derived stats drawn below the spendable ones, in this order. Leave empty to get every
	 * entry of EMALSecondaryStat, the same way RowStats falls back to its whole enum.
	 *
	 * DamageBonus is in the list even though no primary derives it: it is driven by buff effects
	 * instead, and it is equally not a place a point goes, so it belongs with the readouts.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Attribute")
	TArray<EMALSecondaryStat> SecondaryRowStats;

	/** Scroll box holding the spendable page's rows, the first page of StatsSwitcherName. */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Attribute")
	FName RowContainerName = TEXT("ScrollBox_0");

	/** Scroll box holding the derived page's rows, the second page of StatsSwitcherName. */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Attribute")
	FName SecondaryRowContainerName = TEXT("ScrollBox_Secondary");

	/**
	 * Switcher whose two pages are the containers above. Left None, both pages are still built
	 * and only the first is ever put on screen.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Attribute")
	FName StatsSwitcherName = TEXT("WidgetSwitcher_0");

	/** Button that jumps to the spendable page. A missing one is left unwired. */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Attribute")
	FName PreviousPageButtonName = TEXT("Button_PrevTab");

	/** Button that jumps to the derived page. A missing one is left unwired. */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Attribute")
	FName NextPageButtonName = TEXT("Button_NextTab");

	/** Title block rewritten on every page change. Left None, the title never changes. */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Attribute")
	FName PageTitleTextName = TEXT("TextBlock_73");

	/** Text block showing the points still available to spend. Left None, the readout stays blank. */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Attribute")
	FName PointsLeftTextName = TEXT("TextBlock_PointsLeft");

	/** Title shown on the spendable page. */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Attribute")
	FText PrimaryPageTitle;

	/** Title shown on the derived page. */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Attribute")
	FText SecondaryPageTitle;

	/**
	 * Button inside a row that puts one point into that row's stat.
	 * Rename here if the row blueprint's add button is not Button_56.
	 *
	 * Only the spendable page wires this: a row on the derived page carrying a button is left
	 * inert, because there is no stat behind it to put a point into.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Attribute")
	FName RowAddPointButtonName = TEXT("Button_56");

	//~ Begin UMALUserWidget
	virtual void NativeConstruct() override;
	virtual void NativeAbilitySystemReady(UAbilitySystemComponent* InAbilitySystemComponent) override;
	//~ End UMALUserWidget

	UMALAttributeMenuWidget();

private:
	/** Find the points readout once the rows exist; a missing one only costs the number. */
	void ResolvePointsLeftText();

	/** Write the bank into the panel. Called on every change, not just on open. */
	void RefreshPointsLeft(float Points);

	/** Fill both pages. Returns false when neither container exists. */
	bool BuildRows();

	/** Fill the spendable page. Returns how many rows refused their view model. */
	int32 BuildPrimaryRows(UPanelWidget* Container);

	/** Fill the derived page. Returns how many rows refused their view model. */
	int32 BuildSecondaryRows(UPanelWidget* Container);

	/**
	 * Hand one row its view model and paint it.
	 *
	 * Returns false when the row blueprint declares no source of this type: the label and value
	 * are still painted, only the blueprint binding stays dead.
	 */
	bool InitializeRow(UMALUserWidget* Row, UMALAttributeRowViewModel* RowViewModel);

	/** Wire the two page buttons, if the blueprint has them. */
	void BindPageSwitcher();

	/** Point the switcher and the title at the current page. */
	void ApplyStatsPage();

	/**
	 * Write every row's current label and value straight into the widget.
	 *
	 * The rows are also meant to be bound in the blueprint, but a blueprint that has not
	 * declared a row source yet would otherwise stay at its design-time text, and the label is
	 * known here even before an ability system exists.
	 */
	void PublishRowValues();

	void PublishRowsInContainer(UPanelWidget* Container,
		const TArray<TObjectPtr<UMALAttributeRowViewModel>>& RowViewModels);

	/**
	 * Grey out the add buttons while the bank is empty.
	 *
	 * The spend itself already refuses a point that is not there; this is so the panel says so
	 * before it is clicked rather than after.
	 */
	void BindPointAvailability(UAbilitySystemComponent* InAbilitySystemComponent);

	/** Native delegate handler, not a UFUNCTION: nothing here is driven by reflection. */
	void HandleAttributePointsChanged(const FOnAttributeChangeData& ChangeData);

	void SetAddPointEnabled(bool bEnabled);

	UPanelWidget* FindRowContainer(FName ContainerName) const;
	UWidgetSwitcher* FindStatsSwitcher() const;

	/** One view model per child of the spendable page's container. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMALAttributeRowViewModel>> PrimaryRowViewModels;

	/** One view model per child of the derived page's container. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMALAttributeRowViewModel>> SecondaryRowViewModels;

	/** Add buttons, one per spendable row, in the order those rows appear. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> RowAddPointButtons;

	/** Block that prints the points left; resolved from PointsLeftTextName. */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PointsLeftText;

	/** Set once the rows exist. Construct runs again on every add-to-viewport. */
	bool bRowsBuilt = false;

	/** Which page the switcher is on. */
	bool bShowingSecondaryStats = false;
};
