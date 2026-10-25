// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/MALEnemyAIController.h"

#include "AI/BTTask_ActivateAbilityByTag.h"
#include "AI/BTTask_PatrolMoveTo.h"
#include "AI/BTTask_PickPatrolPoint.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BehaviorTree/BTDecorator.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/Composites/BTComposite_Selector.h"
#include "BehaviorTree/Composites/BTComposite_Sequence.h"
#include "BehaviorTree/Tasks/BTTask_Wait.h"
#include "BrainComponent.h"
#include "Character/MALCharacterBase.h"
#include "Combat/MALCombatEvents.h"
#include "Combat/MALCombatEventsSubsystem.h"
#include "Components/MALAbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Tags/MALGameplayTags.h"
#include "UObject/UnrealType.h"

namespace MALAIBlackboardKeys
{
	const FName TargetActor(TEXT("TargetActor"));
	const FName LastKnownLocation(TEXT("LastKnownLocation"));
	const FName HomeLocation(TEXT("HomeLocation"));
	const FName InAttackRange(TEXT("bInAttackRange"));
	const FName PatrolLocation(TEXT("PatrolLocation"));
}

namespace
{
	/** Squared distance ratio a newcomer has to beat before it steals a healthy current target. */
	constexpr float TargetSwitchRatio = 0.8f;

	/** Selector cadence while the attack branch is not ready. */
	constexpr float ChasePollInterval = 0.25f;

	/**
	 * Print the running tree. A mis-wired asset (branch in the wrong order, a trailing wait that
	 * always succeeds, a missing fallback) looks exactly like broken code from the outside, so the
	 * log has to show what actually runs.
	 */
	void LogBehaviorTreeNode(const UBTNode* Node, int32 Depth)
	{
		if (!Node)
		{
			return;
		}

		const FString Indent = FString::ChrN(Depth * 2, TEXT(' '));
		UE_LOG(LogTemp, Log, TEXT("MAL|AI|Tree: %s%s '%s'"),
			*Indent, *Node->GetClass()->GetName(), *Node->GetNodeName());

		const UBTCompositeNode* Composite = Cast<UBTCompositeNode>(Node);
		if (!Composite)
		{
			return;
		}

		for (const FBTCompositeChild& Child : Composite->Children)
		{
			for (const TObjectPtr<UBTDecorator>& Decorator : Child.Decorators)
			{
				if (Decorator)
				{
					UE_LOG(LogTemp, Log, TEXT("MAL|AI|Tree: %s  [decorator] %s (%s)"),
						*Indent, *Decorator->GetClass()->GetName(), *Decorator->GetStaticDescription());
				}
			}

			if (Child.ChildComposite)
			{
				LogBehaviorTreeNode(Child.ChildComposite, Depth + 1);
			}
			else if (Child.ChildTask)
			{
				LogBehaviorTreeNode(Child.ChildTask, Depth + 1);
			}
		}
	}
}

AMALEnemyAIController::AMALEnemyAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	bStartAILogicOnPossess = false;
	bStopAILogicOnUnposses = true;

	Blackboard = CreateDefaultSubobject<UBlackboardComponent>(TEXT("BlackboardComponent"));
	BrainComponent = CreateDefaultSubobject<UBehaviorTreeComponent>(TEXT("BehaviorTreeComponent"));
	PerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("PerceptionComponent"));
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));

	AttackAbilityTag = UMALGameplayTags::Get()->Ability_Attack;
}

void AMALEnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	SetGenericTeamId(FGenericTeamId(MALTeam::Enemy));

	ConfigurePerception();
	BindCombatEvents();

	if (InPawn)
	{
		HomeLocation = InPawn->GetActorLocation();

		if (Blackboard)
		{
			Blackboard->SetValueAsVector(MALAIBlackboardKeys::HomeLocation, HomeLocation);
		}
	}

	StartEnemyBehavior();
}

void AMALEnemyAIController::OnUnPossess()
{
	if (BrainComponent)
	{
		BrainComponent->StopLogic(TEXT("Unpossessed"));
	}

	Super::OnUnPossess();
}

void AMALEnemyAIController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// The combat subsystem outlives this controller; a stale listener would call back into a
	// controller whose blackboard is already gone.
	if (UWorld* World = GetWorld())
	{
		if (UMALCombatEventsSubsystem* CombatEvents = World->GetSubsystem<UMALCombatEventsSubsystem>())
		{
			CombatEvents->OnDamageResolved.RemoveAll(this);
		}
	}

	bCombatEventsBound = false;

	Super::EndPlay(EndPlayReason);
}

void AMALEnemyAIController::ConfigurePerception()
{
	// A Blueprint that replaced the inherited component with one of its own leaves this member
	// dangling, and every setting below would silently go nowhere. Look the component up by class
	// instead of giving up on perception.
	if (!PerceptionComponent)
	{
		PerceptionComponent = FindComponentByClass<UAIPerceptionComponent>();
	}

	// The sight config is a plain subobject, not a component, and a generated Blueprint class can
	// arrive without it. Rebuild one from the settings below instead of leaving sight permanently
	// dark: the null check would otherwise abort on the first line of real work.
	if (!SightConfig)
	{
		SightConfig = NewObject<UAISenseConfig_Sight>(this);
		UE_LOG(LogTemp, Warning,
			TEXT("MALEnemyAIController %s: default SightConfig was missing, rebuilt it at runtime"), *GetName());
	}

	if (!PerceptionComponent || !SightConfig)
	{
		UE_LOG(LogTemp, Error,
			TEXT("MALEnemyAIController %s: no perception component (component=%s sightConfig=%s), sight will never work"),
			*GetName(), *GetNameSafe(PerceptionComponent), *GetNameSafe(SightConfig));
		return;
	}

	SightConfig->SightRadius = SightRadius;
	SightConfig->LoseSightRadius = FMath::Max(LoseSightRadius, SightRadius);
	SightConfig->PeripheralVisionAngleDegrees = PeripheralVisionAngleDegrees;
	SightConfig->SetMaxAge(5.0f);
	SightConfig->AutoSuccessRangeFromLastSeenLocation = -1.0f;
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = false;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = false;

	PerceptionComponent->ConfigureSense(*SightConfig);
	PerceptionComponent->SetDominantSense(SightConfig->GetSenseImplementation());
	PerceptionComponent->SetActive(true);

	// ConfigureSense can run before the component has registered its listener (OnPossess is
	// earlier than the perception system's first update). Ask the system to register/update
	// the listener now that the sight config has been added; without this the listener id is
	// invalid and sight never produces a target.
	PerceptionComponent->RequestStimuliListenerUpdate();

	// Rebind on every possession instead of once: a Blueprint component replacement would leave
	// the original delegate on a component that is no longer driving this controller. RemoveAll
	// first so a second call cannot stack duplicate bindings.
	PerceptionComponent->OnTargetPerceptionUpdated.RemoveAll(this);
	PerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(this, &AMALEnemyAIController::HandleTargetPerceptionUpdated);

	UE_LOG(LogTemp, Warning,
		TEXT("MALEnemyAIController::ConfigurePerception %s: component=%s registered=%d active=%d listener=%d sight=%.0f loseSight=%.0f halfAngle=%.0f"),
		*GetName(),
		*GetNameSafe(PerceptionComponent),
		PerceptionComponent->IsRegistered() ? 1 : 0,
		PerceptionComponent->IsActive() ? 1 : 0,
		PerceptionComponent->GetListenerId().IsValid() ? 1 : 0,
		SightConfig->SightRadius,
		SightConfig->LoseSightRadius,
		SightConfig->PeripheralVisionAngleDegrees);

	// More than one perception component means the Blueprint added its own next to the native one;
	// the extra listener never produces a stimulus and is easy to mistake for a broken sight cone.
	TArray<UAIPerceptionComponent*> PerceptionComponents;
	GetComponents<UAIPerceptionComponent>(PerceptionComponents);
	UE_LOG(LogTemp, Warning, TEXT("MALEnemyAIController %s: %d perception component(s) on the controller"),
		*GetName(), PerceptionComponents.Num());

	for (const UAIPerceptionComponent* Component : PerceptionComponents)
	{
		UE_LOG(LogTemp, Warning, TEXT("MALEnemyAIController %s:   %s active=%d listener=%d"),
			*GetName(),
			*GetNameSafe(Component),
			Component->IsActive() ? 1 : 0,
			Component->GetListenerId().IsValid() ? 1 : 0);
	}
}

void AMALEnemyAIController::BindCombatEvents()
{
	if (bCombatEventsBound)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (UMALCombatEventsSubsystem* CombatEvents = World->GetSubsystem<UMALCombatEventsSubsystem>())
	{
		CombatEvents->OnDamageResolved.AddUObject(this, &AMALEnemyAIController::HandleDamageResolved);
		bCombatEventsBound = true;
	}
}

void AMALEnemyAIController::HandleDamageResolved(const FMALDamageResolvedEvent& Event)
{
	if (!HasAuthority() || !Blackboard)
	{
		return;
	}

	APawn* MyPawn = GetPawn();
	if (!MyPawn)
	{
		return;
	}

	// Enemies own their AttributeSet, so a hit on this pawn arrives with the pawn as the victim.
	// PlayerState-owned sets never match here, which is exactly what we want.
	if (Event.Victim.Get() != MyPawn)
	{
		return;
	}

	AActor* Attacker = Event.Instigator.Get();
	if (!IsValid(Attacker) || Attacker == MyPawn)
	{
		return;
	}

	if (FGenericTeamId::GetAttitude(MyPawn, Attacker) != ETeamAttitude::Hostile)
	{
		return;
	}

	AActor* CurrentTarget = GetCombatTarget();
	if (CurrentTarget == Attacker)
	{
		Blackboard->SetValueAsVector(MALAIBlackboardKeys::LastKnownLocation, Attacker->GetActorLocation());
		return;
	}

	// Sight is a cone in front of the enemy, so the first hit from behind would otherwise be
	// ignored until the attacker walks into view. A resolved hit is harder evidence than sight.
	if (CurrentTarget && IsValid(CurrentTarget) && !ShouldSwitchTarget(CurrentTarget, Attacker))
	{
		return;
	}

	Blackboard->SetValueAsObject(MALAIBlackboardKeys::TargetActor, Attacker);
	Blackboard->SetValueAsVector(MALAIBlackboardKeys::LastKnownLocation, Attacker->GetActorLocation());
}

void AMALEnemyAIController::StartEnemyBehavior()
{
	if (!BrainComponent)
	{
		return;
	}

	// Prefer the authored asset. Loading by path keeps a natively placed enemy working without
	// a Blueprint AI controller; the path has to match the generated asset location exactly.
	if (!BehaviorTreeAsset && !BehaviorTreeAssetPath.IsEmpty())
	{
		BehaviorTreeAsset = LoadObject<UBehaviorTree>(nullptr, *BehaviorTreeAssetPath);

		UE_CLOG(!BehaviorTreeAsset, LogTemp, Warning,
			TEXT("MALEnemyAIController::StartEnemyBehavior: no BehaviorTree at %s, falling back to the runtime tree"),
			*BehaviorTreeAssetPath);
	}

	UBehaviorTree* Tree = BehaviorTreeAsset ? BehaviorTreeAsset.Get() : BuildRuntimeBehaviorTree();
	if (!Tree)
	{
		return;
	}

	const bool bStarted = RunBehaviorTree(Tree);
	UE_LOG(LogTemp, Warning, TEXT("MALEnemyAIController::StartEnemyBehavior %s: tree=%s started=%s"),
		*GetName(), *Tree->GetName(), bStarted ? TEXT("true") : TEXT("false"));

	LogBehaviorTreeNode(Tree->RootNode, 0);
}

void AMALEnemyAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!HasAuthority() || !Blackboard)
	{
		return;
	}

	// A selector that runs out of children finishes the tree, and a finished tree stops thinking
	// for the rest of the match. Restarting keeps the AI alive even when the authored asset ends
	// with a branch that always succeeds, such as a trailing idle wait.
	if (BrainComponent && !BrainComponent->IsRunning() && (BehaviorTreeAsset || RuntimeBehaviorTree))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("MALEnemyAIController %s: behavior tree finished and stopped, restarting it"), *GetName());
		BrainComponent->RestartLogic();
	}

	APawn* MyPawn = GetPawn();
	if (!MyPawn)
	{
		return;
	}

	AActor* Target = GetCombatTarget();

	if (Target)
	{
		const AMALCharacterBase* TargetCharacter = Cast<AMALCharacterBase>(Target);
		if (!IsValid(Target) || (TargetCharacter && TargetCharacter->IsDead()))
		{
			Blackboard->ClearValue(MALAIBlackboardKeys::TargetActor);
			Target = nullptr;
		}
	}

	const bool bInAttackRange = Target && IsWithinAttackRange(MyPawn, Target);
	Blackboard->SetValueAsBool(MALAIBlackboardKeys::InAttackRange, bInAttackRange);

	// A sight cone that never fires leaves no trace in the log, so print the listener state on a
	// slow timer; the alternative is guessing which half of perception is broken.
	if (PerceptionComponent && GetWorld() && GetWorld()->GetTimeSeconds() - LastPerceptionLogTime >= 5.0f)
	{
		LastPerceptionLogTime = GetWorld()->GetTimeSeconds();

		TArray<AActor*> CurrentlyPerceived;
		PerceptionComponent->GetCurrentlyPerceivedActors(nullptr, CurrentlyPerceived);

		UE_LOG(LogTemp, Warning,
			TEXT("MALEnemyAIController %s: perception perceived=%d listener=%d target=%s"),
			*GetName(),
			CurrentlyPerceived.Num(),
			PerceptionComponent->GetListenerId().IsValid() ? 1 : 0,
			*GetNameSafe(Target));
	}

	// Nothing to fight: the tree's patrol branch owns the wandering, and it already reacts to
	// this key being set (PickPatrolPoint fails), so there is nothing to do here.
	if (!Target)
	{
		return;
	}

	// A patrol leg can be several seconds long, and the tree only re-evaluates once its current
	// task finishes. Stopping here aborts the MoveTo node, whose failure lets the selector fall
	// through to the attack branch. The flag is cleared here as well so a completion message that
	// never arrives cannot freeze the chase.
	if (bPatrolMoveActive)
	{
		bPatrolMoveActive = false;
		StopMovement();
		return;
	}

	// A swing owns the pawn's motion: the lunge is a root motion source, and movement input
	// pushed at the same time would drag the body out of its own animation.
	if (IsAttacking())
	{
		StopMovement();
		return;
	}

	if (bInAttackRange)
	{
		StopMovement();
		FaceTarget(MyPawn, Target);
		return;
	}

	UpdateChase(Target);
}

void AMALEnemyAIController::UpdateChase(AActor* Target)
{
	// Only refill the request once the previous path is done: MoveToActor replaces the current
	// request, so calling it every frame would restart pathfinding on every tick.
	if (GetMoveStatus() != EPathFollowingStatus::Idle)
	{
		return;
	}

	RequestMoveToActor(Target);
}

bool AMALEnemyAIController::PickPatrolPoint(FVector& OutPoint) const
{
	const APawn* MyPawn = GetPawn();
	if (!MyPawn || PatrolRadius <= 0.0f)
	{
		return false;
	}

	// Stay inside 75% of the radius: a point on the boundary plus a path that overshoots slightly
	// would slowly drag the enemy out of its area and leave it stranded there.
	const float LegRadius = PatrolRadius * 0.75f;

	if (UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		FNavLocation Reachable;
		if (NavSystem->GetRandomReachablePointInRadius(HomeLocation, LegRadius, Reachable))
		{
			OutPoint = Reachable.Location;
			return true;
		}
	}

	// No navigation data: pick a point on the ground plane and let MoveTo's direct path walk there.
	// The minimum distance keeps the enemy from twitching around its own feet.
	const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
	const float Distance = FMath::FRandRange(LegRadius * 0.4f, LegRadius);
	OutPoint = HomeLocation + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Distance;

	return true;
}

void AMALEnemyAIController::FaceTarget(APawn* MyPawn, AActor* Target)
{
	if (!MyPawn || !IsValid(Target))
	{
		return;
	}

	const FVector ToTarget = Target->GetActorLocation() - MyPawn->GetActorLocation();
	const FRotator Facing(0.0f, ToTarget.Rotation().Yaw, 0.0f);

	// The lunge reads the controller's control rotation, so the controller has to agree with
	// the body before the ability activates.
	SetControlRotation(Facing);
	MyPawn->SetActorRotation(Facing);
}

bool AMALEnemyAIController::RequestMoveToActor(AActor* Goal)
{
	if (!IsValid(Goal))
	{
		return false;
	}

	EPathFollowingRequestResult::Type Result = EPathFollowingRequestResult::Failed;

	if (bUsePathfindingChase)
	{
		Result = MoveToActor(Goal, ChaseAcceptableRadius, /*bStopOnOverlap*/ true, /*bUsePathfinding*/ true,
			/*bCanStrafe*/ false, nullptr, /*bAllowPartialPath*/ true);

		if (Result == EPathFollowingRequestResult::Failed)
		{
			// No navigation data, or the goal is outside it. Keep the enemy useful with direct
			// movement instead of leaving it standing still for the rest of the match.
			bUsePathfindingChase = false;
		}
	}

	if (!bUsePathfindingChase)
	{
		Result = MoveToActor(Goal, ChaseAcceptableRadius, true, false, false, nullptr, true);
	}

	return Result != EPathFollowingRequestResult::Failed;
}

void AMALEnemyAIController::HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!Actor || !Blackboard)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("MALEnemyAIController %s: perception update actor=%s sensed=%d age=%.2f"),
		*GetName(), *Actor->GetName(), Stimulus.WasSuccessfullySensed() ? 1 : 0, Stimulus.GetAge());

	APawn* MyPawn = GetPawn();
	if (!MyPawn)
	{
		return;
	}

	AActor* CurrentTarget = GetCombatTarget();

	if (!Stimulus.WasSuccessfullySensed())
	{
		// LastKnownLocation is left alone on purpose: losing sight means the enemy no longer
		// knows where the target is, and the last seen point is the only honest answer.
		if (CurrentTarget == Actor)
		{
			Blackboard->ClearValue(MALAIBlackboardKeys::TargetActor);
		}
		return;
	}

	if (FGenericTeamId::GetAttitude(MyPawn, Actor) != ETeamAttitude::Hostile)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("MALEnemyAIController %s: sensed %s but attitude is %d (myTeam=%d theirTeam=%d), ignoring"),
			*GetName(), *Actor->GetName(),
			static_cast<int32>(FGenericTeamId::GetAttitude(MyPawn, Actor)),
			FGenericTeamId::GetTeamIdentifier(MyPawn).GetId(),
			FGenericTeamId::GetTeamIdentifier(Actor).GetId());
		return;
	}

	if (const AMALCharacterBase* TargetCharacter = Cast<AMALCharacterBase>(Actor))
	{
		if (TargetCharacter->IsDead())
		{
			return;
		}
	}

	if (CurrentTarget && CurrentTarget != Actor && !ShouldSwitchTarget(CurrentTarget, Actor))
	{
		return;
	}

	Blackboard->SetValueAsObject(MALAIBlackboardKeys::TargetActor, Actor);
	Blackboard->SetValueAsVector(MALAIBlackboardKeys::LastKnownLocation, Actor->GetActorLocation());

	UE_LOG(LogTemp, Warning, TEXT("MALEnemyAIController %s: perceived target %s"),
		*GetName(), *Actor->GetName());
}

AActor* AMALEnemyAIController::GetCombatTarget() const
{
	return Blackboard ? Cast<AActor>(Blackboard->GetValueAsObject(MALAIBlackboardKeys::TargetActor)) : nullptr;
}

bool AMALEnemyAIController::IsAttacking() const
{
	const AMALCharacterBase* MALCharacter = Cast<AMALCharacterBase>(GetPawn());
	if (!MALCharacter)
	{
		return false;
	}

	const UMALAbilitySystemComponent* ASC = MALCharacter->GetMALAbilitySystemComponent();
	return ASC && ASC->HasMatchingGameplayTag(UMALGameplayTags::Get()->Status_Attacking);
}

bool AMALEnemyAIController::IsWithinAttackRange(const APawn* MyPawn, const AActor* Target) const
{
	if (!MyPawn || !IsValid(Target))
	{
		return false;
	}

	return FVector::Dist2D(MyPawn->GetActorLocation(), Target->GetActorLocation()) <= AttackRange;
}

bool AMALEnemyAIController::ShouldSwitchTarget(AActor* CurrentTarget, AActor* NewTarget) const
{
	if (!IsValid(CurrentTarget) || CurrentTarget == NewTarget)
	{
		return true;
	}

	if (const AMALCharacterBase* CurrentCharacter = Cast<AMALCharacterBase>(CurrentTarget))
	{
		if (CurrentCharacter->IsDead())
		{
			return true;
		}
	}

	const APawn* MyPawn = GetPawn();
	if (!MyPawn || !IsValid(NewTarget))
	{
		return false;
	}

	// Keep the current target unless the newcomer is clearly closer. Without the dead band two
	// players standing side by side would make the enemy flip targets on every perception update.
	const float CurrentDistSq = FVector::DistSquared(MyPawn->GetActorLocation(), CurrentTarget->GetActorLocation());
	const float NewDistSq = FVector::DistSquared(MyPawn->GetActorLocation(), NewTarget->GetActorLocation());

	return NewDistSq < CurrentDistSq * TargetSwitchRatio * TargetSwitchRatio;
}

UBlackboardData* AMALEnemyAIController::BuildRuntimeBlackboard()
{
	UBlackboardData* BlackboardData = NewObject<UBlackboardData>(this, TEXT("RuntimeBlackboard"));

	UBlackboardKeyType_Object* TargetKey = BlackboardData->UpdatePersistentKey<UBlackboardKeyType_Object>(MALAIBlackboardKeys::TargetActor);
	TargetKey->BaseClass = AActor::StaticClass();

	BlackboardData->UpdatePersistentKey<UBlackboardKeyType_Vector>(MALAIBlackboardKeys::LastKnownLocation);
	BlackboardData->UpdatePersistentKey<UBlackboardKeyType_Vector>(MALAIBlackboardKeys::HomeLocation);
	BlackboardData->UpdatePersistentKey<UBlackboardKeyType_Vector>(MALAIBlackboardKeys::PatrolLocation);
	BlackboardData->UpdatePersistentKey<UBlackboardKeyType_Bool>(MALAIBlackboardKeys::InAttackRange);

	RuntimeBlackboard = BlackboardData;
	return BlackboardData;
}

UBehaviorTree* AMALEnemyAIController::BuildRuntimeBehaviorTree()
{
	UBehaviorTree* Tree = NewObject<UBehaviorTree>(this, TEXT("RuntimeBehaviorTree"));
	UBlackboardData* BlackboardData = RuntimeBlackboard ? RuntimeBlackboard.Get() : BuildRuntimeBlackboard();
	Tree->BlackboardAsset = BlackboardData;

	UBTComposite_Selector* Root = NewObject<UBTComposite_Selector>(Tree, TEXT("RootSelector"));
	Root->NodeName = TEXT("Root Selector");
	Tree->RootNode = Root;

	// Attack sequence: try the ability, then wait out a randomized cooldown. If the target is
	// out of range the task fails and the selector falls through to the patrol branch.
	UBTComposite_Sequence* AttackSequence = NewObject<UBTComposite_Sequence>(Tree, TEXT("AttackSequence"));
	AttackSequence->NodeName = TEXT("Attack Sequence");

	UBTTask_ActivateAbilityByTag* AttackTask = NewObject<UBTTask_ActivateAbilityByTag>(Tree, TEXT("AttackTask"));
	AttackTask->NodeName = TEXT("Activate Attack");
	AttackTask->AbilityTag = AttackAbilityTag;
	AttackTask->AttackRange = AttackRange;
	AttackTask->bFaceTarget = true;

	FBTCompositeChild& AttackChild = AttackSequence->Children.AddDefaulted_GetRef();
	AttackChild.ChildTask = AttackTask;

	UBTTask_Wait* CooldownTask = NewObject<UBTTask_Wait>(Tree, TEXT("AttackCooldown"));
	CooldownTask->NodeName = TEXT("Attack Cooldown");
	CooldownTask->WaitTime = AttackCooldown;
	CooldownTask->RandomDeviation = AttackCooldownDeviation;

	FBTCompositeChild& CooldownChild = AttackSequence->Children.AddDefaulted_GetRef();
	CooldownChild.ChildTask = CooldownTask;

	FBTCompositeChild& RootAttackChild = Root->Children.AddDefaulted_GetRef();
	RootAttackChild.ChildComposite = AttackSequence;

	// Patrol sequence: pick a point, walk there, rest. PickPatrolPoint refuses to run while a
	// target exists, so this branch never competes with the chase for a path request.
	UBTComposite_Sequence* PatrolSequence = NewObject<UBTComposite_Sequence>(Tree, TEXT("PatrolSequence"));
	PatrolSequence->NodeName = TEXT("Patrol Sequence");

	UBTTask_PickPatrolPoint* PickTask = NewObject<UBTTask_PickPatrolPoint>(Tree, TEXT("PickPatrolPoint"));
	PickTask->NodeName = TEXT("Pick Patrol Point");

	FBTCompositeChild& PickChild = PatrolSequence->Children.AddDefaulted_GetRef();
	PickChild.ChildTask = PickTask;

	UBTTask_PatrolMoveTo* PatrolMove = NewObject<UBTTask_PatrolMoveTo>(Tree, TEXT("PatrolMove"));
	PatrolMove->NodeName = TEXT("Patrol Move");
	PatrolMove->AcceptableRadius = PatrolAcceptableRadius;

	FBTCompositeChild& PatrolMoveChild = PatrolSequence->Children.AddDefaulted_GetRef();
	PatrolMoveChild.ChildTask = PatrolMove;

	UBTTask_Wait* PatrolPause = NewObject<UBTTask_Wait>(Tree, TEXT("PatrolPause"));
	PatrolPause->NodeName = TEXT("Patrol Pause");
	PatrolPause->WaitTime = (PatrolPauseMin + PatrolPauseMax) * 0.5f;
	PatrolPause->RandomDeviation = FMath::Max(0.0f, (PatrolPauseMax - PatrolPauseMin) * 0.5f);

	FBTCompositeChild& PatrolPauseChild = PatrolSequence->Children.AddDefaulted_GetRef();
	PatrolPauseChild.ChildTask = PatrolPause;

	FBTCompositeChild& RootPatrolChild = Root->Children.AddDefaulted_GetRef();
	RootPatrolChild.ChildComposite = PatrolSequence;

	// The controller's Tick owns the chase. This short wait exists only to re-arm the selector
	// at a useful cadence; it always succeeds, so the selector never runs out of children.
	UBTTask_Wait* ChasePoll = NewObject<UBTTask_Wait>(Tree, TEXT("ChasePoll"));
	ChasePoll->NodeName = TEXT("Chase Poll");
	ChasePoll->WaitTime = ChasePollInterval;

	FBTCompositeChild& RootChaseChild = Root->Children.AddDefaulted_GetRef();
	RootChaseChild.ChildTask = ChasePoll;

	RuntimeBehaviorTree = Tree;
	return Tree;
}

AMALEnemyAIController::FMALAIDefaults AMALEnemyAIController::GetAssetDefaults()
{
	FMALAIDefaults Defaults;

	if (const AMALEnemyAIController* CDO = GetDefault<AMALEnemyAIController>())
	{
		Defaults.AttackRange = CDO->AttackRange;
		Defaults.AttackCooldown = CDO->AttackCooldown;
		Defaults.AttackCooldownDeviation = CDO->AttackCooldownDeviation;
		Defaults.PatrolAcceptableRadius = CDO->PatrolAcceptableRadius;
		Defaults.PatrolPauseMin = CDO->PatrolPauseMin;
		Defaults.PatrolPauseMax = CDO->PatrolPauseMax;
	}

	return Defaults;
}
