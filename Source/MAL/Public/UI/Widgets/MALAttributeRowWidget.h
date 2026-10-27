// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/MALUserWidget.h"
#include "MALAttributeRowWidget.generated.h"

class UButton;
class UTextBlock;
class UMALAttributeRowViewModel;

/**
 * UMALAttributeRowWidget
 *
 * One line of the attribute panel: the label on the left, the formatted reading on the right
 * and, on the spendable page only, the button that puts a point into the stat.
 *
 * The row owns the two text properties a blueprint binds to. They used to live on
 * UMALUserWidget, which meant every widget in the project carried an attribute-panel API and
 * looked up two text blocks by name on construct. Keeping them here is what makes the panel a
 * panel: the rest of the UI never sees them.
 *
 * The panel creates rows from RowWidgetClass, so the row blueprint has to declare a settable
 * MALAttributeRowViewModel source in its View Binding panel. When it does not, the panel still
 * seeds the label and value once, which is what keeps a half-configured row readable instead
 * of blank.
 */
UCLASS(Abstract)
class MAL_API UMALAttributeRowWidget : public UMALUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * Point the row at one stat and hand it its view model.
	 *
	 * Returns false when the blueprint declares no settable source of that type: the text is
	 * still seeded, only the live binding stays dead.
	 */
	bool SetRowViewModel(UMALAttributeRowViewModel* InViewModel);

	/** The view model behind this row, or null before the panel has built it. */
	UFUNCTION(BlueprintPure, Category = "MAL|UI|Attribute")
	UMALAttributeRowViewModel* GetRowViewModel() const { return RowViewModel; }

	/**
	 * Grey the add button out while the bank is empty.
	 *
	 * The spend itself already refuses a point that is not there; this is so the row says so
	 * before it is clicked rather than after.
	 */
	void SetAddPointEnabled(bool bEnabled);

	/**
	 * The value this row shows, already formatted for display.
	 *
	 * One level deep on purpose. A view binding destination is a single property on a single
	 * widget, so a row that wanted to print its value could not bind through
	 * AttributePoints.Text: that path is two levels deep and the last hop, Text, belongs to
	 * TextBlock rather than to the row's generated class. The binding is then dropped and the
	 * row shows nothing. The row binds here instead and the setter mirrors the text into its
	 * number block.
	 *
	 * The BlueprintSetter is load-bearing twice over: a view binding that targets a property
	 * with one is compiled into a call to that setter, and a blueprint that assigns the value
	 * by hand gets the same mirroring for free.
	 *
	 * Formatted upstream rather than as a number: the panel prints whole stats, percentages
	 * and multipliers from the same row widget, and only the caller knows which one it holds.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "MAL|UI|MVVM", meta = (BlueprintSetter = "SetDisplayValue"))
	FText DisplayValue;

	/** Writes DisplayValue and republishes it into the row's number text block. */
	UFUNCTION(BlueprintCallable, Category = "MAL|UI|MVVM")
	void SetDisplayValue(const FText& InValue);

	/**
	 * The label this row shows, in its native form.
	 *
	 * Same one-level rule as DisplayValue and for the same reason: AttributeName.Text is two
	 * hops deep, so a binding cannot target it. The row binds here instead and the setter
	 * mirrors the text into its name block.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "MAL|UI|MVVM", meta = (BlueprintSetter = "SetDisplayName"))
	FText DisplayName;

	/** Writes DisplayName and republishes it into the row's name text block. */
	UFUNCTION(BlueprintCallable, Category = "MAL|UI|MVVM")
	void SetDisplayName(const FText& InName);

protected:
	/**
	 * Row parts, all optional.
	 *
	 * A minimal row blueprint that only prints a stat still compiles, and the setters below
	 * skip whatever the blueprint left out instead of demanding a fixed widget tree.
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> AttributeName;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> AttributePoints;

	/** Present on the spendable page only; a derived row collapses it. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> AddPointButton;

	//~ Begin UUserWidget
	virtual void NativeConstruct() override;
	//~ End UUserWidget

private:
	/** Routed through the row so the button never has to know which stat it belongs to. */
	UFUNCTION()
	void HandleAddPointClicked();

	/** Show the add button exactly on the rows that can spend a point. */
	void RefreshAddPointVisibility();

	UPROPERTY(Transient)
	TObjectPtr<UMALAttributeRowViewModel> RowViewModel;
};
