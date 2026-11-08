// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/MALBossAttackSelectorComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/Attributes/MALAttributeSet.h"
#include "AIController.h"
#include "Character/MALCharacterBase.h"
#include "Character/MALEnemyCharacter.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Tags/MALGameplayTags.h"

namespace
{
	struct FScoredBossAttack
	{
		FMALBossAttackEntry Entry;
		float Score = 0.0f;
	};
}

UMALBossAttackSelectorComponent::UMALBossAttackSelectorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;

	PhaseAttackIntervals = { 1.1f, 0.85f, 0.65f };
	PhaseAttackBudgets = { 3, 4, 5 };
}

void UMALBossAttackSelectorComponent::BeginPlay()
{
	Super::BeginPlay();

	// The shared enemy class hosts the component; only Bosses assign BossAttackSet. Fall back to
	// the character's data asset so a Boss is configured entirely from Blueprint defaults.
	if (!AttackSet)
	{
		if (const AMALEnemyCharacter* Enemy = Cast<AMALEnemyCharacter>(GetOwner()))
		{
			AttackSet = Enemy->BossAttackSet;
		}
	}

	if (const UMALBossAttackSet* Set = AttackSet)
	{
		CurrentPhase = Set->GetPhaseForHealthPercent(GetHealthPercent());
	}

	if (UWorld* World = GetWorld())
	{
		NextAttackAllowedTime = World->GetTimeSeconds() + 0.5f;
	}
}

void UMALBossAttackSelectorComponent::TickComponent(
	float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// The existing Blueprint BT task activates through TryActivateAbilityByTag and does not own a
	// wait node, so the selector itself observes the spec. This keeps phasing/cadence correct even
	// when no C++ activation task is used.
	if (bAttackInProgress && ActiveAttackClass)
	{
		UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
		const FGameplayAbilitySpec* Spec = ASC ? ASC->FindAbilitySpecFromClass(ActiveAttackClass) : nullptr;
		if (!Spec || !Spec->IsActive())
		{
			NotifyAttackEnded();
		}
	}

	UpdatePhaseState();
}

AMALCharacterBase* UMALBossAttackSelectorComponent::GetOwnerCharacter() const
{
	return Cast<AMALCharacterBase>(GetOwner());
}

float UMALBossAttackSelectorComponent::GetHealthPercent() const
{
	const AMALCharacterBase* Character = GetOwnerCharacter();
	const UMALAttributeSet* Attributes = Character ? Character->GetMALAttributeSet() : nullptr;
	if (!Attributes)
	{
		return 1.0f;
	}

	const float MaxHealth = Attributes->GetMaxHealth();
	return MaxHealth > 0.0f ? FMath::Clamp(Attributes->GetHealth() / MaxHealth, 0.0f, 1.0f) : 1.0f;
}

float UMALBossAttackSelectorComponent::GetPhaseInterval(int32 Phase) const
{
	return PhaseAttackIntervals.IsValidIndex(Phase)
		? FMath::Max(PhaseAttackIntervals[Phase], 0.0f)
		: 1.0f;
}

int32 UMALBossAttackSelectorComponent::GetPhaseBudget(int32 Phase) const
{
	return PhaseAttackBudgets.IsValidIndex(Phase) ? FMath::Max(PhaseAttackBudgets[Phase], 0) : 3;
}

bool UMALBossAttackSelectorComponent::HasLineOfSight(AActor* Target) const
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Target)
	{
		return false;
	}

	if (const APawn* OwnerPawn = Cast<APawn>(Owner))
	{
		if (const AAIController* AI = Cast<AAIController>(OwnerPawn->GetController()))
		{
			return AI->LineOfSightTo(Target);
		}
	}

	// Not AI-controlled (e.g. a scripted arena Boss): do not invent an occlusion result.
	return true;
}

int32 UMALBossAttackSelectorComponent::GetConsecutiveUses(const TSubclassOf<UGameplayAbility>& AbilityClass) const
{
	int32 Count = 0;
	for (int32 Index = RecentAttacks.Num() - 1; Index >= 0; --Index)
	{
		if (RecentAttacks[Index] != AbilityClass)
		{
			break;
		}
		++Count;
	}

	return Count;
}

bool UMALBossAttackSelectorComponent::SelectAttack(AActor* Target, FMALBossAttackEntry& OutAttack, FString& OutDebug)
{
	OutDebug.Reset();

	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World || !Owner->HasAuthority())
	{
		OutDebug = TEXT("no authority/owner");
		return false;
	}

	if (!AttackSet || AttackSet->Attacks.Num() == 0)
	{
		OutDebug = TEXT("no AttackSet configured");
		WriteDebug(OutDebug);
		return false;
	}

	if (!Target)
	{
		OutDebug = TEXT("no target");
		WriteDebug(OutDebug);
		return false;
	}

	UpdatePhaseState();

	const float Now = World->GetTimeSeconds();
	if (bAttackInProgress)
	{
		OutDebug = TEXT("attack in progress");
		return false;
	}

	if (IsPhaseTransitioning())
	{
		OutDebug = FString::Printf(TEXT("phase transition (%.2fs left)"), PhaseTransitionEndTime - Now);
		return false;
	}

	if (Now < NextAttackAllowedTime)
	{
		OutDebug = FString::Printf(TEXT("cadence wait (%.2fs)"), NextAttackAllowedTime - Now);
		return false;
	}

	AttackTimes.RemoveAll([Now, this](float StartTime)
	{
		return StartTime <= Now - FMath::Max(BudgetWindow, 0.1f);
	});

	const int32 Budget = GetPhaseBudget(CurrentPhase);
	if (AttackTimes.Num() >= Budget)
	{
		OutDebug = FString::Printf(TEXT("attack budget reached (%d/%d)"), AttackTimes.Num(), Budget);
		return false;
	}

	const UMALGameplayTags* Tags = UMALGameplayTags::Get();
	const UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	const AMALCharacterBase* TargetCharacter = Cast<AMALCharacterBase>(Target);
	const float TargetHealthPercent = [TargetCharacter]()
	{
		const UMALAttributeSet* Attributes = TargetCharacter ? TargetCharacter->GetMALAttributeSet() : nullptr;
		if (!Attributes || Attributes->GetMaxHealth() <= 0.0f)
		{
			return 1.0f;
		}
		return FMath::Clamp(Attributes->GetHealth() / Attributes->GetMaxHealth(), 0.0f, 1.0f);
	}();

	FVector ToTarget = Target->GetActorLocation() - Owner->GetActorLocation();
	ToTarget.Z = 0.0f;
	const float Distance = ToTarget.Size();
	const FVector Direction = Distance > KINDA_SMALL_NUMBER ? ToTarget / Distance : Owner->GetActorForwardVector();
	const float FacingDot = FVector::DotProduct(Owner->GetActorForwardVector(), Direction);
	const bool bTargetBehind = FacingDot < -0.25f;

	TArray<FScoredBossAttack> Candidates;
	FString CandidateLog;

	for (const FMALBossAttackEntry& Entry : AttackSet->Attacks)
	{
		if (!Entry.AbilityClass)
		{
			continue;
		}
		
		const float PhaseWeight = Entry.GetPhaseWeight(CurrentPhase);
		if (PhaseWeight <= 0.0f)
		{
			continue;
		}
		
		if (Distance < Entry.MinRange || Distance > Entry.MaxRange)
		{
			continue;
		}

		if (Entry.bRequiresLineOfSight && !HasLineOfSight(Target))
		{
			continue;
		}

		if (const float* ReadyTime = ReadyTimes.Find(Entry.AbilityClass))
		{
			if (*ReadyTime > Now)
			{
				continue;
			}
		}

		if (GetConsecutiveUses(Entry.AbilityClass) >= FMath::Max(Entry.MaxConsecutive, 1))
		{
			continue;
		}

		const float DistanceScore = Entry.bRadial
			? 1.0f
			: 1.0f - FMath::Clamp(
				FMath::Abs(Distance - Entry.PreferredRange) / FMath::Max(Entry.RangeTolerance, 1.0f), 0.0f, 1.0f);

		const float AngleScore = Entry.bRadial
			? 1.0f
			: FMath::Clamp(0.5f + 0.5f * FacingDot, 0.0f, 1.0f);

		float StateMultiplier = 1.0f;
		if (TargetASC)
		{
			if (TargetASC->HasMatchingGameplayTag(Tags->Status_Casting))
			{
				StateMultiplier *= Entry.CastingMultiplier;
			}
			if (TargetASC->HasMatchingGameplayTag(Tags->Status_Attacking))
			{
				StateMultiplier *= Entry.AttackingMultiplier;
			}
		}

		if (bTargetBehind)
		{
			StateMultiplier *= Entry.BehindMultiplier;
		}

		if (TargetHealthPercent <= Entry.LowHealthThreshold)
		{
			StateMultiplier *= Entry.LowHealthMultiplier;
		}

		// Prevent one situational bonus from fully determining the pick. The Boss should react,
		// not read the player's inputs.
		StateMultiplier = FMath::Clamp(StateMultiplier, 0.5f, 2.5f);

		float Readiness = 1.0f;
		if (const float* ReadyTime = ReadyTimes.Find(Entry.AbilityClass))
		{
			if (Now - *ReadyTime < 2.0f)
			{
				Readiness = 0.85f;
			}
		}

		float RepeatPenalty = 1.0f;
		if (RecentAttacks.Num() > 0 && RecentAttacks.Last() == Entry.AbilityClass)
		{
			RepeatPenalty = 0.2f;
		}
		else
		{
			const int32 FirstRecent = FMath::Max(0, RecentAttacks.Num() - 2);
			for (int32 Index = RecentAttacks.Num() - 1; Index >= FirstRecent; --Index)
			{
				if (RecentAttacks[Index] == Entry.AbilityClass)
				{
					RepeatPenalty = 0.5f;
					break;
				}
			}
		}

		const float Jitter = FMath::FRandRange(0.92f, 1.08f);
		const float Score = PhaseWeight * DistanceScore * AngleScore * StateMultiplier * RepeatPenalty * Readiness * Jitter;

		FScoredBossAttack& Candidate = Candidates.AddDefaulted_GetRef();
		Candidate.Entry = Entry;
		Candidate.Score = Score;

		CandidateLog += FString::Printf(
			TEXT("\n  %s=%.2f (D=%.2f A=%.2f S=%.2f R=%.2f)"),
			*Entry.GetDebugName(), Score, DistanceScore, AngleScore, StateMultiplier, RepeatPenalty);
	}

	if (Candidates.Num() == 0)
	{
		OutDebug = FString::Printf(TEXT("P%d dist=%.0f: no candidate%s"), CurrentPhase, Distance,
			*CandidateLog);
		WriteDebug(OutDebug);
		return false;
	}

	Candidates.Sort([](const FScoredBossAttack& A, const FScoredBossAttack& B)
	{
		return A.Score > B.Score;
	});

	int32 ChosenIndex = 0;
	const int32 TopCount = FMath::Clamp(TopCandidateCount, 1, Candidates.Num());
	if (TopCount > 1)
	{
		// Uniform over the top N: a slightly worse attack is still a valid choice, which keeps
		// the Boss from settling into the same optimal move every cycle.
		ChosenIndex = FMath::RandRange(0, TopCount - 1);
	}

	LastSelectedAttack = Candidates[ChosenIndex].Entry;
	bHasSelection = true;

	LastSelectionDebug = FString::Printf(
		TEXT("P%d dist=%.0f target=%s%s\nChosen: %s (%.2f, top-%d random)"),
		CurrentPhase, Distance, *GetNameSafe(Target),
		*CandidateLog, *LastSelectedAttack.GetDebugName(), Candidates[ChosenIndex].Score, TopCount);

	OutAttack = LastSelectedAttack;
	OutDebug = LastSelectionDebug;
	WriteDebug(LastSelectionDebug);
	return true;
}

bool UMALBossAttackSelectorComponent::ActivateLastSelectedAttack()
{
	if (!bHasSelection || bAttackInProgress)
	{
		return false;
	}

	AMALEnemyCharacter* Enemy = Cast<AMALEnemyCharacter>(GetOwner());
	UWorld* World = GetWorld();
	if (!Enemy || !World || !LastSelectedAttack.AbilityClass)
	{
		bHasSelection = false;
		return false;
	}

	if (!Enemy->TryActivateAbilityByClass(LastSelectedAttack.AbilityClass))
	{
		UE_LOG(LogTemp, Warning, TEXT("MALBossSelect %s: activation refused for %s"),
			*GetNameSafe(GetOwner()), *LastSelectedAttack.GetDebugName());
		bHasSelection = false;
		return false;
	}

	const float Now = World->GetTimeSeconds();
	ActiveAttackClass = LastSelectedAttack.AbilityClass;
	ActiveAttackCooldown = FMath::Max(LastSelectedAttack.Cooldown, 0.0f);
	bAttackInProgress = true;
	bHasSelection = false;

	AttackTimes.Add(Now);
	RecentAttacks.Add(ActiveAttackClass);
	TrimRecentAttacks();

	UE_LOG(LogTemp, Log, TEXT("MALBossSelect %s: activated %s (P%d, cooldown %.1fs)"),
		*GetNameSafe(GetOwner()), *LastSelectedAttack.GetDebugName(), CurrentPhase, ActiveAttackCooldown);

	return true;
}

void UMALBossAttackSelectorComponent::NotifyAttackEnded()
{
	if (!bAttackInProgress)
	{
		return;
	}

	UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.0f;

	if (ActiveAttackClass)
	{
		ReadyTimes.Add(ActiveAttackClass, Now + FMath::Max(ActiveAttackCooldown, 0.0f));
	}

	bAttackInProgress = false;
	ActiveAttackClass = nullptr;

	float ExtraDelay = 0.0f;
	if (PlayerHitStreak >= FMath::Max(HitStreakRecoveryThreshold, 1))
	{
		ExtraDelay += FMath::Max(HitStreakRecovery, 0.0f);
		PlayerHitStreak = 0;
	}
	if (PlayerEvadeStreak >= FMath::Max(EvadeStreakDelayThreshold, 1))
	{
		ExtraDelay += FMath::Max(EvadeStreakDelay, 0.0f);
		PlayerEvadeStreak = 0;
	}

	NextAttackAllowedTime = Now + GetPhaseInterval(CurrentPhase) + ExtraDelay;

	// A threshold crossed during the attack is honoured only now: the current animation always
	// finishes before the phase beat starts.
	UpdatePhaseState();

	UE_LOG(LogTemp, Log, TEXT("MALBossSelect %s: attack ended, next allowed in %.2fs (phase %d)"),
		*GetNameSafe(GetOwner()), FMath::Max(NextAttackAllowedTime - Now, 0.0f), CurrentPhase);
}

bool UMALBossAttackSelectorComponent::IsPhaseTransitioning() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimeSeconds() < PhaseTransitionEndTime;
}

float UMALBossAttackSelectorComponent::GetNextAttackAllowedTime() const
{
	return NextAttackAllowedTime;
}

void UMALBossAttackSelectorComponent::NotifyPlayerHit()
{
	++PlayerHitStreak;
	PlayerEvadeStreak = 0;
}

void UMALBossAttackSelectorComponent::NotifyPlayerEvaded()
{
	++PlayerEvadeStreak;
	PlayerHitStreak = 0;
}

void UMALBossAttackSelectorComponent::UpdatePhaseState()
{
	if (!AttackSet)
	{
		return;
	}

	const int32 DesiredPhase = AttackSet->GetPhaseForHealthPercent(GetHealthPercent());
	if (DesiredPhase > CurrentPhase)
	{
		PendingPhase = DesiredPhase;
		bHasPendingPhase = true;
	}

	if (bHasPendingPhase && !bAttackInProgress && !IsPhaseTransitioning())
	{
		StartPhaseTransition(PendingPhase);
	}
}

void UMALBossAttackSelectorComponent::StartPhaseTransition(int32 NewPhase)
{
	UWorld* World = GetWorld();
	if (!World || !AttackSet)
	{
		return;
	}

	CurrentPhase = FMath::Clamp(NewPhase, 0, 2);
	bHasPendingPhase = false;
	bHasSelection = false;

	const float Now = World->GetTimeSeconds();
	PhaseTransitionEndTime = Now + FMath::Max(AttackSet->PhaseTransitionDuration, 0.0f);
	NextAttackAllowedTime = PhaseTransitionEndTime + FMath::Max(AttackSet->PhaseTransitionGrace, 0.0f);

	// A new phase starts with a clean cadence budget, but cooldowns intentionally survive.
	AttackTimes.Reset();

	UE_LOG(LogTemp, Log, TEXT("MALBossSelect %s: phase -> %d (transition %.2fs + grace %.2fs)"),
		*GetNameSafe(GetOwner()), CurrentPhase,
		AttackSet->PhaseTransitionDuration, AttackSet->PhaseTransitionGrace);
}

void UMALBossAttackSelectorComponent::TrimRecentAttacks()
{
	constexpr int32 MaxRecent = 4;
	if (RecentAttacks.Num() > MaxRecent)
	{
		RecentAttacks.RemoveAt(0, RecentAttacks.Num() - MaxRecent);
	}
}

void UMALBossAttackSelectorComponent::WriteDebug(const FString& Text)
{
	LastSelectionDebug = Text;
	UE_LOG(LogTemp, Verbose, TEXT("MALBossSelect %s: %s"), *GetNameSafe(GetOwner()), *Text);
}
