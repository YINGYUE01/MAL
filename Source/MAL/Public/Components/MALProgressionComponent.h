// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Attributes/MALPrimaryStat.h"
#include "Components/ActorComponent.h"
#include "MALProgressionComponent.generated.h"

class UMALClassDefinition;
class UMALProgressionAttributeSet;
struct FMALDamageResolvedEvent;

/** Fired on the authority, once per AddExperience call that crossed at least one level. */
DECLARE_MULTICAST_DELEGATE_OneParam(FMALLevelChangedSignature, int32);

/** Fired on the authority whenever the unspent point total changes. */
DECLARE_MULTICAST_DELEGATE_OneParam(FMALAttributePointsChangedSignature, int32);

/**
 * UMALProgressionComponent
 *
 * Awards experience on the authority, owns the level-up decision, and turns a level into
 * attribute points the player spends on the primary stats.
 *
 * Lives on AMALPlayerState rather than on the pawn: progression survives a respawn, and the
 * PlayerState already owns the ASC and the attribute set this writes into.
 *
 * Experience arrives over the resolved-combat bus rather than through the victim's death path. The
 * victim already knows what it is worth (AMALCharacterBase::ExperienceReward) and the bus already
 * carries the killer, so neither the combat layer nor the character layer has to know that
 * progression exists. The price of that decoupling is that every player's component sees every
 * kill in the world and has to filter for its own.
 */
UCLASS()
class MAL_API UMALProgressionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMALProgressionComponent();

	/**
	 * Server only. Add experience and resolve every level it crosses.
	 *
	 * One award can cross more than one level. The loop doing it is bounded from both ends: by the
	 * class definition's maximum level, and by a threshold that has to be positive to be
	 * subtracted.
	 */
	UFUNCTION(BlueprintCallable, Category = "MAL|Progression")
	void AddExperience(int32 Amount);

	/**
	 * Server only. Bank Count points without awarding experience.
	 *
	 * Levelling is the normal source; this is the seam for rewards that are not kills, and for
	 * exercising the spend path from the console.
	 */
	UFUNCTION(BlueprintCallable, Category = "MAL|Progression")
	void GrantAttributePoints(int32 Count);

	/**
	 * Server only. Put Count of the unspent points into one primary stat.
	 *
	 * The stat is written through the attribute set, so the derived secondaries and the HUD
	 * follow without anything here touching them. Returns false when the points are not there,
	 * the stat is already at the class cap, or this runs off the authority.
	 */
	UFUNCTION(BlueprintCallable, Category = "MAL|Progression")
	bool SpendAttributePoints(EMALPrimaryStat Stat, int32 Count = 1);

	/** Points earned and not yet spent. Zero while the progression set is absent. */
	UFUNCTION(BlueprintPure, Category = "MAL|Progression")
	int32 GetAttributePoints() const;

	/** Points already put into Stat. Zero for an out-of-range stat. */
	UFUNCTION(BlueprintPure, Category = "MAL|Progression")
	int32 GetAllocatedPoints(EMALPrimaryStat Stat) const;

	/**
	 * Put every already-spent point back on top of the primaries.
	 *
	 * Called by the pawn right after the default primary effect has written the baseline values,
	 * which overwrite whatever the player had spent. Safe to call when nothing was spent.
	 */
	void ReapplyAttributePoints();

	/**
	 * Fired on the authority after a level-up.
	 *
	 * Nothing consumes it yet. The HUD learns about the level through replicated attributes, so
	 * this is the seam for things that are not attribute-shaped: level-up audio, a gameplay cue,
	 * an unlock check.
	 */
	FMALLevelChangedSignature OnLevelChanged;

	/**
	 * Fired on the authority after points are earned or spent.
	 *
	 * The HUD reads the replicated attribute instead; this is the seam for things that are not
	 * attribute-shaped, such as an "unspent points" prompt.
	 */
	FMALAttributePointsChangedSignature OnAttributePointsChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Narrow the world-wide kill bus down to this player's own kills. */
	void HandleDamageResolved(const FMALDamageResolvedEvent& Event);

	/** Owning player's class definition, read through the pawn. Null while the pawn is absent. */
	const UMALClassDefinition* GetClassDefinition() const;

	UMALProgressionAttributeSet* GetProgressionSet() const;

	/** Experience needed to leave Level, from the class definition's threshold curve. */
	float EvaluateExperienceToNextLevel(int32 Level) const;

	/**
	 * Points spent per stat, indexed by EMALPrimaryStat.
	 *
	 * Kept here rather than on the attribute set: only the authority needs the split, and the
	 * primaries cannot be asked, since equipment and effects write the same values.
	 */
	TArray<int32> AllocatedPoints;
};