// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/MALBossAttackSelectorComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/Attributes/MALAttributeSet.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/MALCharacterBase.h"
#include "Character/MALEnemyCharacter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameplayAbilitySpec.h"
#include "Tags/MALGameplayTags.h"

namespace
{
	struct FScoredBossAttack
	{
		FMALBossSkillEntry Entry;
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

	if (const UMALBossSkillKit* Set = GetSkillKit())
	{
		CurrentPhase = Set->GetPhaseForHealthPercent(GetHealthPercent());
	}

	if (UWorld* World = GetWorld())
	{
		NextAttackAllowedTime = World->GetTimeSeconds() + 0.5f;
	}
}

const UMALBossSkillKit* UMALBossAttackSelectorComponent::GetSkillKit() const
{
	const AMALEnemyCharacter* Enemy = Cast<AMALEnemyCharacter>(GetOwner());
	return Enemy ? Enemy->BossSkillKit : nullptr;
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

#if !UE_BUILD_SHIPPING
	if (bDrawSkillScores)
	{
		DrawSkillScores();
	}
#endif
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

bool UMALBossAttackSelectorComponent::IsSkillOnCooldown(const TSubclassOf<UGameplayAbility>& AbilityClass) const
{
	if (!AbilityClass)
	{
		return false;
	}

	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	const FGameplayAbilityActorInfo* ActorInfo = ASC ? ASC->AbilityActorInfo.Get() : nullptr;
	const FGameplayAbilitySpec* Spec = ASC ? ASC->FindAbilitySpecFromClass(AbilityClass) : nullptr;
	if (!ActorInfo || !Spec || !Spec->Ability)
	{
		return false;
	}

	// The ability's own cooldown GE is the single source of truth; a skill forced from Blueprint
	// consumes the same timer, so the selector can never disagree with GAS.
	return Spec->Ability->GetCooldownTimeRemaining(ActorInfo) > KINDA_SMALL_NUMBER;
}

bool UMALBossAttackSelectorComponent::SelectAttack(AActor* Target, FMALBossSkillEntry& OutAttack, FString& OutDebug)
{
	OutDebug.Reset();

	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World || !Owner->HasAuthority())
	{
		OutDebug = TEXT("no authority/owner");
		return false;
	}

	const UMALBossSkillKit* Kit = GetSkillKit();
	if (!Kit || Kit->Skills.Num() == 0)
	{
		OutDebug = TEXT("no SkillKit configured");
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

	const bool bTargetCasting = TargetASC && TargetASC->HasMatchingGameplayTag(Tags->Status_Casting);
	const bool bTargetAttacking = TargetASC && TargetASC->HasMatchingGameplayTag(Tags->Status_Attacking);

	TArray<FScoredBossAttack> Candidates;
	FString CandidateLog;

	for (const FMALBossSkillEntry& Entry : Kit->Skills)
	{
		if (!Entry.AbilityClass || !Entry.bSelectable)
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

		// Cooldown is GAS-owned: skip while the ability's own cooldown GE is still running.
		if (IsSkillOnCooldown(Entry.AbilityClass))
		{
			continue;
		}

		if (GetConsecutiveUses(Entry.AbilityClass) >= FMath::Max(Entry.MaxConsecutive, 1))
		{
			continue;
		}

		// Jitter only perturbs the pick; the deterministic part is shared with the debug table.
		const float Score = ScoreEntry(
			Entry, Distance, FacingDot, bTargetBehind, TargetHealthPercent,
			bTargetCasting, bTargetAttacking) * FMath::FRandRange(0.92f, 1.08f);

		FScoredBossAttack& Candidate = Candidates.AddDefaulted_GetRef();
		Candidate.Entry = Entry;
		Candidate.Score = Score;

		// ScoreEntry folds the repeat penalty in; log it once so the table and log agree.
		CandidateLog += FString::Printf(
			TEXT("\n  %s=%.2f (R=%.2f)"),
			*Entry.GetDebugName(), Score, GetRepeatPenalty(Entry));
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
	bAttackInProgress = true;
	bHasSelection = false;

	AttackTimes.Add(Now);
	RecentAttacks.Add(ActiveAttackClass);
	TrimRecentAttacks();

	UE_LOG(LogTemp, Log, TEXT("MALBossSelect %s: activated %s (P%d)"),
		*GetNameSafe(GetOwner()), *LastSelectedAttack.GetDebugName(), CurrentPhase);

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
	const UMALBossSkillKit* Kit = GetSkillKit();
	if (!Kit)
	{
		return;
	}

	const int32 DesiredPhase = Kit->GetPhaseForHealthPercent(GetHealthPercent());
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
	const UMALBossSkillKit* Kit = GetSkillKit();
	if (!World || !Kit)
	{
		return;
	}

	CurrentPhase = FMath::Clamp(NewPhase, 0, 2);
	bHasPendingPhase = false;
	bHasSelection = false;

	const float Now = World->GetTimeSeconds();
	PhaseTransitionEndTime = Now + FMath::Max(Kit->PhaseTransitionDuration, 0.0f);
	NextAttackAllowedTime = PhaseTransitionEndTime + FMath::Max(Kit->PhaseTransitionGrace, 0.0f);

	// A new phase starts with a clean cadence budget, but cooldowns intentionally survive.
	AttackTimes.Reset();

	UE_LOG(LogTemp, Log, TEXT("MALBossSelect %s: phase -> %d (transition %.2fs + grace %.2fs)"),
		*GetNameSafe(GetOwner()), CurrentPhase,
		Kit->PhaseTransitionDuration, Kit->PhaseTransitionGrace);
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

float UMALBossAttackSelectorComponent::GetRepeatPenalty(const FMALBossSkillEntry& Entry) const
{
	if (RecentAttacks.Num() > 0 && RecentAttacks.Last() == Entry.AbilityClass)
	{
		return 0.2f;
	}

	const int32 FirstRecent = FMath::Max(0, RecentAttacks.Num() - 2);
	for (int32 Index = RecentAttacks.Num() - 1; Index >= FirstRecent; --Index)
	{
		if (RecentAttacks[Index] == Entry.AbilityClass)
		{
			return 0.5f;
		}
	}

	return 1.0f;
}

float UMALBossAttackSelectorComponent::ScoreEntry(
	const FMALBossSkillEntry& Entry,
	float Distance,
	float FacingDot,
	bool bTargetBehind,
	float TargetHealthPercent,
	bool bTargetCasting,
	bool bTargetAttacking) const
{
	const float DistanceScore = Entry.bRadial
		? 1.0f
		: 1.0f - FMath::Clamp(
			FMath::Abs(Distance - Entry.PreferredRange) / FMath::Max(Entry.RangeTolerance, 1.0f), 0.0f, 1.0f);

	const float AngleScore = Entry.bRadial
		? 1.0f
		: FMath::Clamp(0.5f + 0.5f * FacingDot, 0.0f, 1.0f);

	float StateMultiplier = 1.0f;
	if (bTargetCasting)
	{
		StateMultiplier *= Entry.CastingMultiplier;
	}
	if (bTargetAttacking)
	{
		StateMultiplier *= Entry.AttackingMultiplier;
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

	return Entry.GetPhaseWeight(CurrentPhase) * DistanceScore * AngleScore * StateMultiplier
		* GetRepeatPenalty(Entry);
}

void UMALBossAttackSelectorComponent::DrawSkillScores()
{
#if !UE_BUILD_SHIPPING
	if (!GEngine)
	{
		return;
	}

	const UMALBossSkillKit* Kit = GetSkillKit();
	const AActor* Owner = GetOwner();
	if (!Kit || !Owner)
	{
		return;
	}

	// Current target: focus actor first, then the blackboard key the BT tasks write.
	AActor* Target = nullptr;
	if (const APawn* OwnerPawn = Cast<APawn>(Owner))
	{
		if (const AAIController* AI = Cast<AAIController>(OwnerPawn->GetController()))
		{
			Target = AI->GetFocusActor();
			if (!Target)
			{
				if (const UBlackboardComponent* Blackboard = AI->GetBlackboardComponent())
				{
					Target = Cast<AActor>(Blackboard->GetValueAsObject(TEXT("TargetActor")));
				}
			}
		}
	}

	float Distance = 0.0f;
	float FacingDot = 1.0f;
	bool bTargetBehind = false;
	float TargetHealthPercent = 1.0f;
	bool bTargetCasting = false;
	bool bTargetAttacking = false;

	if (Target)
	{
		FVector ToTarget = Target->GetActorLocation() - Owner->GetActorLocation();
		ToTarget.Z = 0.0f;
		Distance = ToTarget.Size();
		const FVector Direction = Distance > KINDA_SMALL_NUMBER
			? ToTarget / Distance
			: Owner->GetActorForwardVector();
		FacingDot = FVector::DotProduct(Owner->GetActorForwardVector(), Direction);
		bTargetBehind = FacingDot < -0.25f;

		if (const AMALCharacterBase* TargetCharacter = Cast<AMALCharacterBase>(Target))
		{
			if (const UMALAttributeSet* Attributes = TargetCharacter->GetMALAttributeSet())
			{
				if (Attributes->GetMaxHealth() > 0.0f)
				{
					TargetHealthPercent = FMath::Clamp(
						Attributes->GetHealth() / Attributes->GetMaxHealth(), 0.0f, 1.0f);
				}
			}
		}

		if (const UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target))
		{
			const UMALGameplayTags* Tags = UMALGameplayTags::Get();
			bTargetCasting = TargetASC->HasMatchingGameplayTag(Tags->Status_Casting);
			bTargetAttacking = TargetASC->HasMatchingGameplayTag(Tags->Status_Attacking);
		}
	}

	// One stable key per kit row: the same key rewrites its own line, so rows never reorder and
	// the table does not scroll. Index-based, because the kit itself is the fixed row order.
	static constexpr int32 BaseKey = 0x4D420000;
	for (int32 Index = 0; Index < Kit->Skills.Num(); ++Index)
	{
		const FMALBossSkillEntry& Entry = Kit->Skills[Index];
		const FString Name = Entry.GetDebugName();

		FString Line;
		FColor Color = FColor::White;
		if (!Entry.AbilityClass || !Entry.bSelectable)
		{
			// Grant-only rows stay in place so the table shape never changes.
			Line = FString::Printf(TEXT("%-26s     --"), *Name);
			Color = FColor(130, 130, 130);
		}
		else
		{
			const float Score = Target
				? ScoreEntry(Entry, Distance, FacingDot, bTargetBehind, TargetHealthPercent,
					bTargetCasting, bTargetAttacking)
				: 0.0f;
			Line = FString::Printf(TEXT("%-26s %6.2f"), *Name, Score);

			// Grey while GAS still holds the cooldown; the value itself stays readable.
			if (IsSkillOnCooldown(Entry.AbilityClass))
			{
				Color = FColor(140, 140, 140);
			}
		}

		GEngine->AddOnScreenDebugMessage(static_cast<uint64>(BaseKey + Index), 0.25f, Color, Line);
	}
#endif
}
