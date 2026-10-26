// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Attributes/MALPrimaryStat.h"
#include "UI/Widgets/MALUserWidget.h"
#include "MALAttributeMenuWidget.generated.h"

class UButton;
class UMALAttributeRowViewModel;
class UPanelWidget;
struct FOnAttributeChangeData;

/**
 * UMALAttributeMenuWidget
 *
 * The stat list: one row per entry in RowStats, top to bottom.
 *
 * Owns one row view model per row. The rows all share a single widget blueprint, so a shared
 * source would print the same stat in every one of them; giving each row its own instance is
 * what lets one blueprint draw five different stats.
 *
 * The rows themselves are found through the container named in RowContainerName, which
 * defaults to the scroll box this project's menu blueprint stacks them in.
 */
UCLASS(Abstract)
class MAL_API UMALAttributeMenuWidget : public UMALUserWidget
{
	GENERATED_BODY()

public:
	/** Row view model at Index, or null past the last configured stat. */
	UFUNCTION(BlueprintPure, Category = "MAL|UI|Attribute")
	UMALAttributeRowViewModel* GetRowViewModel(int32 Index) const;

protected:
	/**
	 * Stats drawn top to bottom. Leave empty to get the five primaries, which is what an
	 * unconfigured blueprint wants.
	 *
	 * A list shorter than the blueprint's row count is normal while a panel is being trimmed:
	 * the extra rows are collapsed instead of left printing a label with no stat behind it.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Attribute")
	TArray<EMALPrimaryStat> RowStats;

	/** Widget that stacks the rows. Rename here if the blueprint's container is not ScrollBox_0. */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Attribute")
	FName RowContainerName = TEXT("ScrollBox_0");

	/**
	 * Button inside a row that puts one point into that row's stat.
	 * Rename here if the row blueprint's add button is not Button_56.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "MAL|UI|Attribute")
	FName RowAddPointButtonName = TEXT("Button_56");

	//~ Begin UMALUserWidget
	virtual void NativeConstruct() override;
	virtual void NativeAbilitySystemReady(UAbilitySystemComponent* InAbilitySystemComponent) override;
	//~ End UMALUserWidget

private:
	/** Build the row view models and hand each row its own. Safe to call once. */
	void BuildRows();

	/**
	 * Write every row's current label and value straight into the widget.
	 *
	 * The rows are also meant to be bound in the blueprint, but a blueprint that has not
	 * declared a row source yet would otherwise stay at its design-time text, and the label is
	 * known here even before an ability system exists.
	 */
	void PublishRowValues();

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

	UPanelWidget* FindRowContainer() const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMALAttributeRowViewModel>> RowViewModels;

	/** Add button per row, in the same order as RowViewModels. Entries stay null when a row has none. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> RowAddPointButtons;
};
