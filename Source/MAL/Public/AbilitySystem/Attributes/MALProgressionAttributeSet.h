// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystem/Attributes/MALAttributeMacros.h"
#include "MALProgressionAttributeSet.generated.h"

/**
 * UMALProgressionAttributeSet
 *
 * Player progression: the character level and the experience banked towards the next one.
 *
 * A set of its own rather than three more entries in UMALAttributeSet. Enemies carry that set too
 * and have no progression to replicate, and keeping this separate leaves the vitals set's
 * replication list and PreAttributeChange switch without a branch that only ever applies to
 * players.
 *
 * Instanced as a default subobject of AMALPlayerState, next to the ASC, which adopts every
 * attribute set found on its owner. The level is an int in spirit and a float in storage, because
 * attributes are; every reader rounds it.
 */
UCLASS()
class MAL_API UMALProgressionAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UMALProgressionAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	// ========== Progression ==========

	/** Current character level. Never below 1. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Level, Category = "Progression")
	FGameplayAttributeData Level;
	ATTRIBUTE_ACCESSORS(UMALProgressionAttributeSet, Level)

	UFUNCTION()
	virtual void OnRep_Level(const FGameplayAttributeData& OldLevel);

	/** Experience banked inside the current level, already net of what earlier levels consumed. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Experience, Category = "Progression")
	FGameplayAttributeData Experience;
	ATTRIBUTE_ACCESSORS(UMALProgressionAttributeSet, Experience)

	UFUNCTION()
	virtual void OnRep_Experience(const FGameplayAttributeData& OldExperience);

	/** Experience needed to leave the current level: the denominator of the HUD bar. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ExperienceToNextLevel, Category = "Progression")
	FGameplayAttributeData ExperienceToNextLevel;
	ATTRIBUTE_ACCESSORS(UMALProgressionAttributeSet, ExperienceToNextLevel)

	UFUNCTION()
	virtual void OnRep_ExperienceToNextLevel(const FGameplayAttributeData& OldExperienceToNextLevel);

	/**
	 * Attribute points earned by levelling and not yet spent.
	 *
	 * An attribute rather than a counter on the progression component: the HUD draws it, and the
	 * replication and change notification this set already provides come with it. What has been
	 * spent is remembered by the component, since the primaries are plain base values that
	 * equipment and effects also write.
	 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_AttributePoints, Category = "Progression")
	FGameplayAttributeData AttributePoints;
	ATTRIBUTE_ACCESSORS(UMALProgressionAttributeSet, AttributePoints)

	UFUNCTION()
	virtual void OnRep_AttributePoints(const FGameplayAttributeData& OldAttributePoints);
};