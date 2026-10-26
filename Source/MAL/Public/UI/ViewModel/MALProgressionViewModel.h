// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/ViewModel/MALUIViewModelBase.h"
#include "MALProgressionViewModel.generated.h"

/**
 * UMALProgressionViewModel
 *
 * Presentation copy of the character level, the experience banked towards the next one, and the
 * attribute points that are still waiting to be spent.
 *
 * A view model of its own rather than more properties on UMALStatViewModel: progression comes
 * from a set of its own (UMALProgressionAttributeSet) that only players carry, and the vitals
 * view model is also pointed at enemies for nameplates, which have no level at all. The
 * primaries are read from the vitals set here because they are what the points are spent on,
 * and a stat panel wants them sitting next to the point count.
 *
 * The bar ratio is computed here, so the HUD can bind a ProgressBar percent straight to
 * ExperiencePercent without dividing in UMG.
 */
UCLASS(BlueprintType)
class MAL_API UMALProgressionViewModel : public UMALUIViewModelBase
{
	GENERATED_BODY()

public:
	/** Rounded current level, ready to drop into a Text block. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Progression")
	int32 DisplayLevel = 1;

	/** Experience banked inside the current level. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Progression")
	float Experience = 0.0f;

	/** Experience needed to leave the current level; the denominator of the bar. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Progression")
	float ExperienceToNextLevel = 0.0f;

	/** Pre-clamped 0..1 ratio; bind a ProgressBar percent here instead of dividing in UMG. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Progression")
	float ExperiencePercent = 0.0f;

	/** Attribute points earned and not yet spent. A prompt can bind its visibility here. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Progression")
	int32 AttributePoints = 0;

	// The primary stats the points go into, rounded for a stat panel to print as-is. They are
	// write-through copies: a point spent on the authority arrives here through replication.

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Progression")
	int32 Vigor = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Progression")
	int32 Strength = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Progression")
	int32 Dexterity = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Progression")
	int32 Endurance = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Progression")
	int32 Spirit = 0;

protected:
	//~ Begin UMALUIViewModelBase
	virtual void OnAbilitySystemReady(UAbilitySystemComponent* InAbilitySystemComponent) override;
	virtual void OnAbilitySystemReleased() override;
	//~ End UMALUIViewModelBase

private:
	void RefreshExperiencePercent();
};