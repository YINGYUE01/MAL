// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Attributes/MALPrimaryStat.h"
#include "UI/ViewModel/MALUIViewModelBase.h"
#include "MALAttributeRowViewModel.generated.h"

/**
 * UMALAttributeRowViewModel
 *
 * Drives one row of the attribute menu: the label on the left and the number on the right.
 *
 * One instance per row is the whole point. Every row in the menu is the same widget blueprint,
 * so a source shared between them would print the same stat in all of them; the menu therefore
 * creates an instance per row and hands each row its own.
 *
 * The label is read once from the attribute's own display name, which falls back to the
 * property name, so a stat that wants different wording renames the attribute rather than
 * growing a translation table here.
 */
UCLASS(BlueprintType)
class MAL_API UMALAttributeRowViewModel : public UMALUIViewModelBase
{
	GENERATED_BODY()

public:
	/**
	 * Point the row at one stat. Call before the view model is handed to a widget: it fixes
	 * which attribute the row follows and is what the label is derived from.
	 */
	void Initialize(EMALPrimaryStat InStat);

	/** The stat this row follows; the stat it spends a point on. */
	UFUNCTION(BlueprintPure, Category = "MAL|UI|Attribute")
	EMALPrimaryStat GetPrimaryStat() const { return PrimaryStat; }

	/** The attribute this row follows; invalid before Initialize has run. */
	UFUNCTION(BlueprintPure, Category = "MAL|UI|Attribute")
	FGameplayAttribute GetAttribute() const { return Attribute; }

	/**
	 * Spend one banked point on this row's stat.
	 *
	 * The row's button binds straight here. A row blueprint is one widget shared by every stat,
	 * so a click wired in the graph would have no way to say which stat it belongs to; the per
	 * row view model is the object that already knows.
	 *
	 * The write is server side, so this only raises AddPointRequested and lets the menu route
	 * it. Kept as a callback rather than a lookup here because a view model has no widget and
	 * therefore no owning player: the ability system cannot name a controller on a client,
	 * where the PlayerState does not replicate its owner.
	 */
	UFUNCTION(BlueprintCallable, Category = "MAL|UI|Attribute")
	void RequestAddPoint();

	/** Set by the menu before the row is shown; null leaves the button an inert label. */
	TFunction<void(EMALPrimaryStat)> AddPointRequested;

	/** Label shown at the left of the row. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Attribute")
	FText AttributeName;

	/** Current value shown at the right of the row, rounded to a whole number. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Attribute")
	int32 DisplayValue = 0;

protected:
	//~ Begin UMALUIViewModelBase
	virtual void OnAbilitySystemReady(UAbilitySystemComponent* InAbilitySystemComponent) override;
	//~ End UMALUIViewModelBase

private:
	/** What RequestAddPoint spends on, fixed by Initialize. */
	EMALPrimaryStat PrimaryStat = EMALPrimaryStat::Vigor;

	/**
	 * Kept as a plain member rather than a UPROPERTY: nothing here is garbage collected, and
	 * FGameplayAttribute only carries an editor-only path to the property.
	 */
	FGameplayAttribute Attribute;
};
