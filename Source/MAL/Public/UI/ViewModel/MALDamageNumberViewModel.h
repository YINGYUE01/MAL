// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "MALDamageNumberViewModel.generated.h"

/**
 * UMALDamageNumberViewModel
 *
 * Data for one floating damage number.
 *
 * Unlike UMALStatViewModel this view model owns no AbilitySystemComponent subscription: by the
 * time a number exists the server has already resolved the final post-mitigation value, so the
 * view model is written once and then only drives presentation. One instance is recycled with
 * its widget, which keeps the whole thing allocation-free during combat.
 *
 * bCritical / bMagic / bKillingBlow are kept separate so a widget can drive colour, scale and
 * sound from bindings instead of duplicating the rules in blueprint.
 */
UCLASS(BlueprintType)
class MAL_API UMALDamageNumberViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** Raw damage, useful for effects that scale with the size of the hit. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Damage")
	float Amount = 0.0f;

	/**
	 * Pre-formatted number so the widget binds a string directly. A float bound to a TextBlock
	 * would need a conversion node in every widget that shows damage.
	 */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Damage")
	FText DisplayText;

	/** True when the damage execution rolled a critical hit. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Damage")
	bool bCritical = false;

	/** True when the hit went through the magic channel rather than the physical one. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Damage")
	bool bMagic = false;

	/** True when this hit took the target from above zero health to zero. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MAL|UI|Damage")
	bool bKillingBlow = false;

	/** Overwrite every field for a new hit. Called when the widget is taken out of the pool. */
	void SetDamage(float InAmount, bool bInCritical, bool bInMagic, bool bInKillingBlow);
};
