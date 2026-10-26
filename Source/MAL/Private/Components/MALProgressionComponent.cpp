// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MALProgressionComponent.h"

#include "AbilitySystem/Attributes/MALAttributeSet.h"
#include "AbilitySystem/Attributes/MALProgressionAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Character/MALCharacterBase.h"
#include "Combat/MALCombatEvents.h"
#include "Combat/MALCombatEventsSubsystem.h"
#include "Curves/RealCurve.h"
#include "Data/MALClassDefinition.h"
#include "Engine/CurveTable.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Player/MALPlayerState.h"

namespace
{
	/**
	 * Add Delta to the stat's base value.
	 *
	 * The base is what a spent point means, and reading it back through the ASC rather than off
	 * the set skips the modifiers: a buffed primary must not make the next point worth more.
	 */
	void AddToPrimaryStatBase(UAbilitySystemComponent* AbilitySystemComponent, EMALPrimaryStat Stat, float Delta)
	{
		if (!AbilitySystemComponent || Delta == 0.0f)
		{
			return;
		}

		const FGameplayAttribute Attribute = MALGetPrimaryAttribute(Stat);
		if (Attribute.IsValid())
		{
			const float Base = AbilitySystemComponent->GetNumericAttributeBase(Attribute);
			AbilitySystemComponent->SetNumericAttributeBase(Attribute, Base + Delta);
		}
	}
}

UMALProgressionComponent::UMALProgressionComponent()
{
	// Everything here is event-driven off a world-wide bus; there is nothing to advance per frame.
	PrimaryComponentTick.bCanEverTick = false;

	// Server-only bookkeeping: the attributes it writes replicate on their own, and no remote
	// machine has a use for the component's own state.
	SetIsReplicatedByDefault(false);

	// Sized up front so a spend never has to grow the array, and so the index is the stat.
	AllocatedPoints.SetNum(static_cast<int32>(EMALPrimaryStat::Count));
}

void UMALProgressionComponent::BeginPlay()
{
	Super::BeginPlay();

	// The bus only publishes where damage resolves, which is the authority, and this component
	// only ever writes on the authority. A client-side subscription would sit idle forever.
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (UMALCombatEventsSubsystem* CombatEvents = World->GetSubsystem<UMALCombatEventsSubsystem>())
		{
			CombatEvents->OnDamageResolved.AddUObject(this, &UMALProgressionComponent::HandleDamageResolved);
		}
	}
}

void UMALProgressionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// The subsystem outlives the PlayerState, so drop the listener instead of leaving a stale
	// entry pointing at a destroyed component.
	if (UWorld* World = GetWorld())
	{
		if (UMALCombatEventsSubsystem* CombatEvents = World->GetSubsystem<UMALCombatEventsSubsystem>())
		{
			CombatEvents->OnDamageResolved.RemoveAll(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void UMALProgressionComponent::AddExperience(int32 Amount)
{
	// Server only: the level and the banked experience replicate out, so a client writing them
	// would be overwritten by the next update.
	if (Amount <= 0 || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	UMALProgressionAttributeSet* ProgressionSet = GetProgressionSet();
	if (!ProgressionSet)
	{
		return;
	}

	const UMALClassDefinition* ClassDefinition = GetClassDefinition();
	const int32 MaxLevel = ClassDefinition ? FMath::Max(ClassDefinition->MaxCharacterLevel, 1) : 1;

	// At the cap the award is dropped rather than banked: experience that can never be spent
	// would only make the bar climb against a threshold nothing reads.
	int32 Level = FMath::Max(FMath::RoundToInt(ProgressionSet->GetLevel()), 1);
	if (Level >= MaxLevel)
	{
		return;
	}

	// Plain writes instead of a GameplayEffect: this is bookkeeping with no duration, no
	// modifiers and no magnitude to evaluate, and an effect asset per award would only hide
	// where the numbers go.
	float Experience = ProgressionSet->GetExperience() + static_cast<float>(Amount);
	int32 GainedLevels = 0;

	while (Level < MaxLevel)
	{
		const float Needed = EvaluateExperienceToNextLevel(Level);
		if (Needed <= 0.0f)
		{
			// No threshold authored for this level: stop rather than spin on a subtraction that
			// never makes progress.
			break;
		}

		if (Experience < Needed)
		{
			break;
		}

		Experience -= Needed;
		++Level;
		++GainedLevels;
	}

	ProgressionSet->SetLevel(static_cast<float>(Level));
	ProgressionSet->SetExperience(Experience);
	ProgressionSet->SetExperienceToNextLevel(EvaluateExperienceToNextLevel(Level));

	if (GainedLevels > 0)
	{
		// A level pays out points instead of writing the primaries: the player decides where
		// they land, and a stat only moves when SpendAttributePoints says so.
		const int32 PointsPerLevel = ClassDefinition ? FMath::Max(ClassDefinition->AttributePointsPerLevel, 0) : 0;
		GrantAttributePoints(GainedLevels * PointsPerLevel);

		OnLevelChanged.Broadcast(Level);
	}

	// One line per award: rare enough to leave at Log and the only trace of the numbers the
	// HUD is fed from.
	UE_LOG(LogTemp, Log, TEXT("UMALProgressionComponent: %s +%d XP -> level %d (%.0f/%.0f), %d unspent points"),
		*GetNameSafe(GetOwner()), Amount, Level, Experience, ProgressionSet->GetExperienceToNextLevel(),
		FMath::RoundToInt(ProgressionSet->GetAttributePoints()));
}

void UMALProgressionComponent::HandleDamageResolved(const FMALDamageResolvedEvent& Event)
{
	// Only the killing blow pays out; chip damage does not.
	if (!Event.bKillingBlow)
	{
		return;
	}

	// The bus is world-wide, so every player's component hears every kill. The killer is a pawn
	// and its PlayerState is the actor this component lives on, which is the filter.
	const APawn* KillerPawn = Cast<APawn>(Event.Instigator.Get());
	if (!KillerPawn || KillerPawn->GetPlayerState<AMALPlayerState>() != GetOwner<AMALPlayerState>())
	{
		return;
	}

	// The reward is authored on the victim. An enemy reaches the bus as its pawn; a player
	// victim arrives as the PlayerState that owns its attributes, so read through that pawn.
	const AMALCharacterBase* VictimCharacter = Cast<AMALCharacterBase>(Event.Victim.Get());
	if (!VictimCharacter)
	{
		const APlayerState* VictimPlayerState = Cast<APlayerState>(Event.Victim.Get());
		VictimCharacter = VictimPlayerState ? Cast<AMALCharacterBase>(VictimPlayerState->GetPawn()) : nullptr;
	}

	if (VictimCharacter)
	{
		// Read through the accessor: a placed instance keeps its own zero and would otherwise
		// outrank the reward authored on the victim's class default.
		AddExperience(VictimCharacter->GetExperienceReward());
	}
}

void UMALProgressionComponent::GrantAttributePoints(int32 Count)
{
	if (Count <= 0 || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	AMALPlayerState* OwningPlayerState = GetOwner<AMALPlayerState>();
	UMALProgressionAttributeSet* ProgressionSet = GetProgressionSet();
	UAbilitySystemComponent* AbilitySystemComponent = OwningPlayerState ? OwningPlayerState->GetAbilitySystemComponent() : nullptr;
	if (!ProgressionSet || !AbilitySystemComponent)
	{
		return;
	}

	const int32 Total = FMath::RoundToInt(ProgressionSet->GetAttributePoints()) + Count;

	// Written through the ability system rather than straight onto the set: the attribute panel's
	// add buttons listen for this attribute to know when the bank has anything in it, and a
	// direct SetAttributePoints write does not fire that broadcast on the authority.
	AbilitySystemComponent->SetNumericAttributeBase(
		UMALProgressionAttributeSet::GetAttributePointsAttribute(), static_cast<float>(Total));

	OnAttributePointsChanged.Broadcast(Total);
}

bool UMALProgressionComponent::SpendAttributePoints(EMALPrimaryStat Stat, int32 Count)
{
	// Server only: the primaries replicate out, so a client writing them would be overwritten by
	// the next update.
	if (Count <= 0 || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}

	const int32 Index = static_cast<int32>(Stat);
	if (!AllocatedPoints.IsValidIndex(Index))
	{
		return false;
	}

	AMALPlayerState* OwningPlayerState = GetOwner<AMALPlayerState>();
	UMALProgressionAttributeSet* ProgressionSet = GetProgressionSet();
	UAbilitySystemComponent* AbilitySystemComponent = OwningPlayerState ? OwningPlayerState->GetAbilitySystemComponent() : nullptr;
	if (!ProgressionSet || !AbilitySystemComponent)
	{
		return false;
	}

	const int32 Available = FMath::RoundToInt(ProgressionSet->GetAttributePoints());
	if (Available < Count)
	{
		return false;
	}

	// Cap is authored per class and off by default. Reject the whole spend rather than part of
	// it: a partial spend that returns true would leave the caller's count and the bar disagreeing.
	if (const UMALClassDefinition* ClassDefinition = GetClassDefinition())
	{
		const int32 Cap = ClassDefinition->MaxAttributePointsPerStat;
		if (Cap > 0 && AllocatedPoints[Index] + Count > Cap)
		{
			return false;
		}
	}

	AddToPrimaryStatBase(AbilitySystemComponent, Stat, static_cast<float>(Count));

	AllocatedPoints[Index] += Count;

	// Same reason as GrantAttributePoints: through the ability system, so the panel's buttons
	// learn about the new balance on the authority as well as on a client.
	AbilitySystemComponent->SetNumericAttributeBase(
		UMALProgressionAttributeSet::GetAttributePointsAttribute(),
		static_cast<float>(Available - Count));

	OnAttributePointsChanged.Broadcast(Available - Count);

	return true;
}

int32 UMALProgressionComponent::GetAttributePoints() const
{
	const UMALProgressionAttributeSet* ProgressionSet = GetProgressionSet();
	return ProgressionSet ? FMath::Max(FMath::RoundToInt(ProgressionSet->GetAttributePoints()), 0) : 0;
}

int32 UMALProgressionComponent::GetAllocatedPoints(EMALPrimaryStat Stat) const
{
	const int32 Index = static_cast<int32>(Stat);
	return AllocatedPoints.IsValidIndex(Index) ? AllocatedPoints[Index] : 0;
}

void UMALProgressionComponent::ReapplyAttributePoints()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	AMALPlayerState* OwningPlayerState = GetOwner<AMALPlayerState>();
	UAbilitySystemComponent* AbilitySystemComponent = OwningPlayerState ? OwningPlayerState->GetAbilitySystemComponent() : nullptr;
	if (!AbilitySystemComponent)
	{
		return;
	}

	// Called right after the default primary effect overwrote the base values with the level 1
	// baseline, so every point recorded here is missing and has to go back on top of it.
	for (int32 Index = 0; Index < AllocatedPoints.Num(); ++Index)
	{
		AddToPrimaryStatBase(AbilitySystemComponent, static_cast<EMALPrimaryStat>(Index), static_cast<float>(AllocatedPoints[Index]));
	}
}

const UMALClassDefinition* UMALProgressionComponent::GetClassDefinition() const
{
	const AMALPlayerState* OwningPlayerState = GetOwner<AMALPlayerState>();
	const AMALCharacterBase* Pawn = OwningPlayerState ? Cast<AMALCharacterBase>(OwningPlayerState->GetPawn()) : nullptr;
	return Pawn ? Pawn->GetClassDefinition() : nullptr;
}

UMALProgressionAttributeSet* UMALProgressionComponent::GetProgressionSet() const
{
	const AMALPlayerState* OwningPlayerState = GetOwner<AMALPlayerState>();
	return OwningPlayerState ? OwningPlayerState->GetProgressionAttributeSet() : nullptr;
}

float UMALProgressionComponent::EvaluateExperienceToNextLevel(int32 Level) const
{
	const UMALClassDefinition* ClassDefinition = GetClassDefinition();
	if (!ClassDefinition || !ClassDefinition->ExperienceToNextLevelTable || ClassDefinition->ExperienceToNextLevelRowName.IsNone())
	{
		return 0.0f;
	}

	const FRealCurve* Curve = ClassDefinition->ExperienceToNextLevelTable->FindCurve(
		ClassDefinition->ExperienceToNextLevelRowName,
		TEXT("UMALProgressionComponent::EvaluateExperienceToNextLevel"),
		false);

	return Curve ? Curve->Eval(static_cast<float>(Level)) : 0.0f;
}