// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Attributes/MALPrimaryStat.h"
#include "Components/ActorComponent.h"
#include "Data/MALSkillTreeDefinition.h"
#include "GameplayTagContainer.h"
#include "MALProgressionComponent.generated.h"

class UMALClassDefinition;
class UMALProgressionAttributeSet;
struct FMALDamageResolvedEvent;

/** Fired on the authority, once per AddExperience call that crossed at least one level. */
DECLARE_MULTICAST_DELEGATE_OneParam(FMALLevelChangedSignature, int32);

/** Fired on the authority whenever the unspent point total changes. */
DECLARE_MULTICAST_DELEGATE_OneParam(FMALAttributePointsChangedSignature, int32);

/** Fired on the authority whenever the unlocked skill set changes, or a load replaces it. */
DECLARE_MULTICAST_DELEGATE(FMALSkillsChangedSignature);

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
	 * Server only. Bank Count skill points without awarding experience.
	 *
	 * Same seam as GrantAttributePoints: levelling is the normal source, this is for rewards
	 * that are not kills and for exercising the spend path from the console.
	 */
	UFUNCTION(BlueprintCallable, Category = "MAL|Skill")
	void GrantSkillPoints(int32 Count);

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

	/** The whole allocation table, indexed by EMALPrimaryStat. Read by the save subsystem. */
	const TArray<int32>& GetAllocatedPointsArray() const { return AllocatedPoints; }

	/** Points earned and not yet spent. Zero while the progression set is absent. */
	UFUNCTION(BlueprintPure, Category = "MAL|Skill")
	int32 GetSkillPoints() const;

	/** The skill tree this player's class defines, or null when the class has none. */
	const UMALSkillTreeDefinition* GetSkillTree() const;

	/**
	 * Where the node stands right now, and why.
	 *
	 * Available means the node is reachable -- level and prerequisites are met -- and stays
	 * Available when only the points are missing, with OutReason saying so. The menu needs that
	 * distinction: a node the player cannot reach yet is greyed for good, one they merely cannot
	 * afford yet is still worth showing as the next thing to buy. Unlocked returns an empty
	 * reason, and so does a node that can be bought this instant.
	 */
	EMALSkillNodeState EvaluateSkillState(FGameplayTag SkillTag, FText& OutReason) const;

	/** Replace the allocation table. Server only, for loading a save. */
	void SetAllocatedPoints(const TArray<int32>& InPoints);

	/**
	 * Recompute ExperienceToNextLevel from the class curve.
	 *
	 * The HUD divides by this, and a load writes a level without going through AddExperience,
	 * so the denominator would otherwise keep whatever the previous level left behind.
	 */
	void RefreshExperienceToNextLevel();

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

	// --- Skills ---

	/**
	 * Nodes the player has bought, as skill tags.
	 *
	 * Read-only on purpose: the set is only written by UnlockSkill and SetUnlockedSkills, the two
	 * paths that also charge the points, re-grant the abilities and broadcast. A writable array
	 * would let a caller own a skill the server never charged for.
	 */
	const TArray<FGameplayTag>& GetUnlockedSkills() const { return UnlockedSkills; }

	/** Fired on the authority whenever the set changes, so the menu can redraw without polling. */
	FMALSkillsChangedSignature OnSkillsChanged;

	UFUNCTION(BlueprintPure, Category = "MAL|Skill")
	bool IsSkillUnlocked(FGameplayTag SkillTag) const;

	/**
	 * Why the node cannot be bought right now, if it cannot. Empty OutReason means it can.
	 *
	 * Split from UnlockSkill so the panel can grey a node and say which requirement is missing
	 * before the click, rather than leaving the player to guess after a silent refusal.
	 */
	bool CanUnlockSkill(FGameplayTag SkillTag, FText& OutReason) const;

	/**
	 * Server only. Charge the node's cost, remember the tag, and grant the ability on the pawn.
	 *
	 * Returns false when the node is missing, already owned, or a requirement fails; the caller
	 * is expected to have asked CanUnlockSkill first and to log the reason.
	 */
	UFUNCTION(BlueprintCallable, Category = "MAL|Skill")
	bool UnlockSkill(FGameplayTag SkillTag);

	/**
	 * Replace the unlocked set wholesale. Server only, for loading a save.
	 *
	 * Goes through the same broadcast and re-grant as a normal unlock, so a caller never has to
	 * remember to finish the job itself.
	 */
	void SetUnlockedSkills(const TArray<FGameplayTag>& InSkills);

	/** Re-read the tree and push every unlocked node onto the pawn's ability system. */
	void RefreshGrantedAbilities();

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_UnlockedSkills();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * Cue played on the machine that owns this player when a level is gained.
	 *
	 * Defaults to GameplayCue.Character.LevelUp. Point it at another tag to swap the feedback,
	 * or clear it to silence level-ups without touching code.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Progression|Cue")
	FGameplayTag LevelUpCueTag;

	/**
	 * Plays LevelUpCueTag on this machine.
	 *
	 * A multi-cast rather than a plain call because ExecuteGameplayCue runs only where it is
	 * invoked: the authority awards the experience and every client draws the feedback. One
	 * award can cross several levels, so the cue fires once, for the level the player ends on.
	 */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayLevelUpCue(int32 NewLevel);

private:
	/** Narrow the world-wide kill bus down to this player's own kills. */
	void HandleDamageResolved(const FMALDamageResolvedEvent& Event);

	/** Owning player's class definition, read through the pawn. Null while the pawn is absent. */
	const UMALClassDefinition* GetClassDefinition() const;

	UMALProgressionAttributeSet* GetProgressionSet() const;

	/**
	 * Best-effort write of the whole profile.
	 *
	 * Called from the three places the profile actually changes -- levelling, spending an
	 * attribute point, unlocking a skill -- rather than from a timer: those are rare events, so
	 * a synchronous write costs nothing and never persists a half-applied state.
	 */
	void RequestAutoSave();

	/** Experience needed to leave Level, from the class definition's threshold curve. */
	float EvaluateExperienceToNextLevel(int32 Level) const;

	/**
	 * Points spent per stat, indexed by EMALPrimaryStat.
	 *
	 * Kept here rather than on the attribute set: only the authority needs the split, and the
	 * primaries cannot be asked, since equipment and effects write the same values.
	 */
	TArray<int32> AllocatedPoints;

	/**
	 * Nodes the player has bought, as skill tags.
	 *
	 * Replicated rather than derived from the ASC's granted abilities: a granted spec says an
	 * ability is *active*, not that it was *earned*, and the starting kit is granted the same
	 * way. The menu needs the difference to grey out what is still out there.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_UnlockedSkills)
	TArray<FGameplayTag> UnlockedSkills;
};