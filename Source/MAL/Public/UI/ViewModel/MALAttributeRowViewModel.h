// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Attributes/MALPrimaryStat.h"
#include "AbilitySystem/Attributes/MALSecondaryStat.h"
#include "UI/ViewModel/MALUIViewModelBase.h"
#include "MALAttributeRowViewModel.generated.h"

/**
 * UMALAttributeRowViewModel
 *
 * Drives one row of the attribute menu: the label on the left and the value on the right.
 *
 * One instance per row is the whole point. Every row in the menu is the same widget blueprint,
 * so a source shared between them would print the same stat in all of them; the menu therefore
 * creates an instance per row and hands each row its own.
 *
 * Two kinds of row share this class:
 *  - Primary: the five stats a point can be spent on, with an add button at the right.
 *  - Secondary: the stats derived from the primaries, printed but not spent on.
 * InitializeSecondary leaves the spend path closed, so a derived row is display only no matter
 * what its blueprint calls.
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
	/** Point the row at one spendable stat. Call before the view model is handed to a widget. */
	void InitializePrimary(EMALPrimaryStat InStat);

	/**
	 * Point the row at one derived stat. Call before the view model is handed to a widget.
	 *
	 * The value still follows live: the secondary attribute effect rewrites it whenever a point
	 * lands on a primary, and the same attribute listener picks that up.
	 */
	void InitializeSecondary(EMALSecondaryStat InStat);

	/** The stat this row spends points on; only meaningful after InitializePrimary. */
	UFUNCTION(BlueprintPure, Category = "MAL|UI|Attribute")
	EMALPrimaryStat GetPrimaryStat() const { return PrimaryStat; }

	/** The attribute this row follows; invalid before an Initialize call has run. */
	UFUNCTION(BlueprintPure, Category = "MAL|UI|Attribute")
	FGameplayAttribute GetAttribute() const { return Attribute; }

	/** False on a derived row, which has no add button to press. */
	UFUNCTION(BlueprintPure, Category = "MAL|UI|Attribute")
	bool CanSpendPoints() const { return bCanSpendPoints; }

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

	/**
	 * Current reading shown at the right of the row, formatted for its stat.
	 *
	 * Text rather than a number because the secondaries do not share a unit: CritChance reads
	 * as 8.0%, CritDamage as 170% and AttackSpeed as 1.10. Formatting lives in
	 * MALFormatStatValue so this row and any other readout agree on it.
	 */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Attribute")
	FText DisplayValue;

protected:
	//~ Begin UMALUIViewModelBase
	virtual void OnAbilitySystemReady(UAbilitySystemComponent* InAbilitySystemComponent) override;
	//~ End UMALUIViewModelBase

private:
	/** Shared tail of both Initialize overloads: fixes the attribute and how it prints. */
	void InitializeAttribute(const FGameplayAttribute& InAttribute, EMALStatFormat InFormat);

	/** Format one reading into DisplayValue. */
	void ApplyValue(float Value);

	/** What RequestAddPoint spends on, fixed by InitializePrimary. Count past the primaries. */
	EMALPrimaryStat PrimaryStat = EMALPrimaryStat::Count;

	/** How ApplyValue prints this row's reading, fixed by the Initialize call. */
	EMALStatFormat Format = EMALStatFormat::Integer;

	/** Only a primary row can take a point; a derived row refuses RequestAddPoint outright. */
	bool bCanSpendPoints = false;

	/**
	 * Kept as a plain member rather than a UPROPERTY: nothing here is garbage collected, and
	 * FGameplayAttribute only carries an editor-only path to the property.
	 */
	FGameplayAttribute Attribute;
};
