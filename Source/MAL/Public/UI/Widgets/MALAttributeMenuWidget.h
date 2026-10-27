// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Attributes/MALPrimaryStat.h"
#include "AbilitySystem/Attributes/MALSecondaryStat.h"
#include "UI/Widgets/MALMenuWidget.h"
#include "MALAttributeMenuWidget.generated.h"

class UButton;
class UMALAttributeRowViewModel;
class UMALAttributeRowWidget;
class UScrollBox;
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
 * Rows are created at runtime from RowWidgetClass, one per stat, so the page a row belongs to
 * is decided by which container it was built into rather than by how many copies a blueprint
 * happens to contain. Adding a stat is a change to the arrays below; the blueprint stays the
 * same. The row blueprint carries an add button on the spendable page and collapses it on the
 * derived one, which is why a single row class draws both pages.
 *
 * Owns one row view model per row. The rows all share a single widget blueprint within each
 * page, so a shared source would print the same stat in every one of them; giving each row its
 * own instance is what lets one blueprint draw five different primaries and fourteen derived
 * stats.
 *
 * The parts are bound as optional widgets rather than looked up by name: a renamed widget is a
 * compile error in the blueprint instead of a silent no-op at runtime, and a panel that leaves
 * one out still runs.
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

	/**
	 * Row widget created once per stat.
	 *
	 * A single class draws both pages: its add button is shown only on rows that can spend, so
	 * the derived page needs no second blueprint. Null leaves both pages empty and logs which
	 * property to fill in.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Attribute")
	TSubclassOf<UMALAttributeRowWidget> RowWidgetClass;

	/** Gap under every row, in slate units. */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Attribute", meta = (ClampMin = "0.0"))
	float RowSpacing = 6.0f;

	/** Title shown on the spendable page. */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Attribute")
	FText PrimaryPageTitle;

	/** Title shown on the derived page. */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Attribute")
	FText SecondaryPageTitle;

	/** Scroll box holding the spendable page's rows, the first page of the switcher. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> PrimaryRowsBox;

	/** Scroll box holding the derived page's rows, the second page of the switcher. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> SecondaryRowsBox;

	/** Switcher whose two pages are the containers above. A missing one leaves page one up. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> StatsSwitcher;

	/** Button that jumps to the spendable page. A missing one is left unwired. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> PreviousPageButton;

	/** Button that jumps to the derived page. A missing one is left unwired. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> NextPageButton;

	/** Title block rewritten on every page change. A missing one only costs the title. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PageTitleText;

	/** Text block showing the points still available to spend. A missing one only costs the number. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PointsLeftText;

	//~ Begin UMALUserWidget
	virtual void NativeConstruct() override;
	virtual void NativeAbilitySystemReady(UAbilitySystemComponent* InAbilitySystemComponent) override;
	//~ End UMALUserWidget

	UMALAttributeMenuWidget();

private:
	/** Write the bank into the panel. Called on every change, not just on open. */
	void RefreshPointsLeft(float Points);

	/** Fill both pages. Returns false when the blueprint is not configured enough to try. */
	bool BuildRows();

	/**
	 * Fill one page: one row per stat, created from RowWidgetClass.
	 *
	 * The view model factory is what differs between the pages, so it is the only thing passed
	 * in; everything else here is the same for a spendable and a derived row.
	 *
	 * Returns how many rows refused their view model.
	 */
	int32 BuildPage(UScrollBox* Container,
		int32 StatCount,
		TFunctionRef<UMALAttributeRowViewModel*(int32 Index)> MakeRowViewModel,
		TArray<TObjectPtr<UMALAttributeRowViewModel>>& OutViewModels,
		bool bSpendable);

	/** Wire the two page buttons, if the blueprint has them. */
	void BindPageSwitcher();

	/** Point the switcher and the title at the current page. */
	void ApplyStatsPage();

	/** Listen for the bank and grey the add buttons while it is empty. */
	void BindPointAvailability(UAbilitySystemComponent* InAbilitySystemComponent);

	/** Native delegate handler, not a UFUNCTION: nothing here is driven by reflection. */
	void HandleAttributePointsChanged(const FOnAttributeChangeData& ChangeData);

	void SetAddPointEnabled(bool bEnabled);

	/** One view model per row of the spendable page, in draw order. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMALAttributeRowViewModel>> PrimaryRowViewModels;

	/** One view model per row of the derived page, in draw order. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMALAttributeRowViewModel>> SecondaryRowViewModels;

	/** Rows of the spendable page, so the bank can disable their buttons. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMALAttributeRowWidget>> SpendableRows;

	/** Set once the rows exist. Construct runs again on every add-to-viewport. */
	bool bRowsBuilt = false;

	/** Which page the switcher is on. */
	bool bShowingSecondaryStats = false;
};
