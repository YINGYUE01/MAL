// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/MALGenerateAIAssetsCommandlet.h"

#if WITH_EDITOR

#include "AI/BTTask_ActivateAbilityByTag.h"
#include "AI/BTTask_PatrolMoveTo.h"
#include "AI/BTTask_PickPatrolPoint.h"
#include "AI/MALEnemyAIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "BehaviorTree/Composites/BTComposite_Selector.h"
#include "BehaviorTree/Composites/BTComposite_Sequence.h"
#include "BehaviorTree/Tasks/BTTask_Wait.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/PackageName.h"
#include "Tags/MALGameplayTags.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace
{
	// Must match the folder the controller loads from (BehaviorTreeAssetPath); generating into a
	// different folder produces assets that nothing picks up.
	const TCHAR* BlackboardPackageName = TEXT("/Game/Blueprint/AI/BB_StoneGolem");
	const TCHAR* BehaviorTreePackageName = TEXT("/Game/Blueprint/AI/BT_StoneGolem");

	bool SaveAssetPackage(UPackage* Package, UObject* Asset)
	{
		if (!Package || !Asset)
		{
			return false;
		}

		Package->MarkPackageDirty();

		const FString Filename = FPackageName::LongPackageNameToFilename(
			Package->GetName(), FPackageName::GetAssetPackageExtension());

		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		Args.Error = GError;
		Args.bWarnOfLongFilename = false;
		Args.bSlowTask = false;

		return UPackage::SavePackage(Package, Asset, *Filename, Args);
	}

	UBlackboardData* BuildBlackboard()
	{
		UPackage* Package = CreatePackage(BlackboardPackageName);
		UBlackboardData* Blackboard = NewObject<UBlackboardData>(
			Package, TEXT("BB_StoneGolem"), RF_Public | RF_Standalone);

		UBlackboardKeyType_Object* TargetKey =
			Blackboard->UpdatePersistentKey<UBlackboardKeyType_Object>(MALAIBlackboardKeys::TargetActor);
		TargetKey->BaseClass = AActor::StaticClass();

		Blackboard->UpdatePersistentKey<UBlackboardKeyType_Vector>(MALAIBlackboardKeys::LastKnownLocation);
		Blackboard->UpdatePersistentKey<UBlackboardKeyType_Vector>(MALAIBlackboardKeys::HomeLocation);
		Blackboard->UpdatePersistentKey<UBlackboardKeyType_Vector>(MALAIBlackboardKeys::PatrolLocation);
		Blackboard->UpdatePersistentKey<UBlackboardKeyType_Bool>(MALAIBlackboardKeys::InAttackRange);

		if (!SaveAssetPackage(Package, Blackboard))
		{
			UE_LOG(LogTemp, Error, TEXT("MALGenerateAIAssets: failed to save %s"), BlackboardPackageName);
		}

		return Blackboard;
	}

	UBehaviorTree* BuildBehaviorTree(UBlackboardData* Blackboard)
	{
		// Read the tuning from the controller class so the generated asset cannot drift away
		// from the values the native runtime tree uses.
		const AMALEnemyAIController::FMALAIDefaults Defaults = AMALEnemyAIController::GetAssetDefaults();

		UPackage* Package = CreatePackage(BehaviorTreePackageName);
		UBehaviorTree* Tree = NewObject<UBehaviorTree>(
			Package, TEXT("BT_StoneGolem"), RF_Public | RF_Standalone);
		Tree->BlackboardAsset = Blackboard;

		UBTComposite_Selector* Root = NewObject<UBTComposite_Selector>(Tree, TEXT("RootSelector"));
		Root->NodeName = TEXT("Root Selector");
		Tree->RootNode = Root;

		UBTComposite_Sequence* AttackSequence = NewObject<UBTComposite_Sequence>(Tree, TEXT("AttackSequence"));
		AttackSequence->NodeName = TEXT("Attack Sequence");

		UBTTask_ActivateAbilityByTag* AttackTask = NewObject<UBTTask_ActivateAbilityByTag>(Tree, TEXT("AttackTask"));
		AttackTask->NodeName = TEXT("Activate Attack");
		AttackTask->AbilityTag = UMALGameplayTags::Get()->Ability_Attack;
		AttackTask->AttackRange = Defaults.AttackRange;
		AttackTask->bFaceTarget = true;

		FBTCompositeChild& AttackChild = AttackSequence->Children.AddDefaulted_GetRef();
		AttackChild.ChildTask = AttackTask;

		UBTTask_Wait* CooldownTask = NewObject<UBTTask_Wait>(Tree, TEXT("AttackCooldown"));
		CooldownTask->NodeName = TEXT("Attack Cooldown");
		CooldownTask->WaitTime = Defaults.AttackCooldown;
		CooldownTask->RandomDeviation = Defaults.AttackCooldownDeviation;

		FBTCompositeChild& CooldownChild = AttackSequence->Children.AddDefaulted_GetRef();
		CooldownChild.ChildTask = CooldownTask;

		FBTCompositeChild& RootAttackChild = Root->Children.AddDefaulted_GetRef();
		RootAttackChild.ChildComposite = AttackSequence;

		// Patrol sequence: pick a point, walk there, rest. PickPatrolPoint fails while a target
		// exists, so this branch never competes with the chase for a path request.
		UBTComposite_Sequence* PatrolSequence = NewObject<UBTComposite_Sequence>(Tree, TEXT("PatrolSequence"));
		PatrolSequence->NodeName = TEXT("Patrol Sequence");

		UBTTask_PickPatrolPoint* PickTask = NewObject<UBTTask_PickPatrolPoint>(Tree, TEXT("PickPatrolPoint"));
		PickTask->NodeName = TEXT("Pick Patrol Point");

		FBTCompositeChild& PickChild = PatrolSequence->Children.AddDefaulted_GetRef();
		PickChild.ChildTask = PickTask;

		UBTTask_PatrolMoveTo* PatrolMove = NewObject<UBTTask_PatrolMoveTo>(Tree, TEXT("PatrolMove"));
		PatrolMove->NodeName = TEXT("Patrol Move");
		PatrolMove->AcceptableRadius = Defaults.PatrolAcceptableRadius;

		FBTCompositeChild& PatrolMoveChild = PatrolSequence->Children.AddDefaulted_GetRef();
		PatrolMoveChild.ChildTask = PatrolMove;

		UBTTask_Wait* PatrolPause = NewObject<UBTTask_Wait>(Tree, TEXT("PatrolPause"));
		PatrolPause->NodeName = TEXT("Patrol Pause");
		PatrolPause->WaitTime = (Defaults.PatrolPauseMin + Defaults.PatrolPauseMax) * 0.5f;
		PatrolPause->RandomDeviation = FMath::Max(0.0f, (Defaults.PatrolPauseMax - Defaults.PatrolPauseMin) * 0.5f);

		FBTCompositeChild& PatrolPauseChild = PatrolSequence->Children.AddDefaulted_GetRef();
		PatrolPauseChild.ChildTask = PatrolPause;

		FBTCompositeChild& RootPatrolChild = Root->Children.AddDefaulted_GetRef();
		RootPatrolChild.ChildComposite = PatrolSequence;

		UBTTask_Wait* ChasePoll = NewObject<UBTTask_Wait>(Tree, TEXT("ChasePoll"));
		ChasePoll->NodeName = TEXT("Chase Poll");
		ChasePoll->WaitTime = 0.25f;

		FBTCompositeChild& RootChaseChild = Root->Children.AddDefaulted_GetRef();
		RootChaseChild.ChildTask = ChasePoll;

		// No idle branch: ChasePoll always succeeds, so anything after it in the selector would
		// be unreachable dead weight in the asset.
		if (!SaveAssetPackage(Package, Tree))
		{
			UE_LOG(LogTemp, Error, TEXT("MALGenerateAIAssets: failed to save %s"), BehaviorTreePackageName);
		}

		return Tree;
	}
}

UMALGenerateAIAssetsCommandlet::UMALGenerateAIAssetsCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UMALGenerateAIAssetsCommandlet::Main(const FString& Params)
{
	// These assets are hand-tuned in the editor today, so overwriting them silently would throw
	// away designer work. Rebuilding requires an explicit -Force.
	if (!Params.Contains(TEXT("-Force")))
	{
		if (FPackageName::DoesPackageExist(BlackboardPackageName) || FPackageName::DoesPackageExist(BehaviorTreePackageName))
		{
			UE_LOG(LogTemp, Error,
				TEXT("MALGenerateAIAssets: %s or %s already exists, pass -Force to rebuild"),
				BlackboardPackageName, BehaviorTreePackageName);
			return 1;
		}
	}

	UBlackboardData* Blackboard = BuildBlackboard();
	UBehaviorTree* Tree = BuildBehaviorTree(Blackboard);

	UE_LOG(LogTemp, Display, TEXT("MALGenerateAIAssets: BB=%s BT=%s"),
		Blackboard ? *Blackboard->GetPathName() : TEXT("null"),
		Tree ? *Tree->GetPathName() : TEXT("null"));

	return Blackboard && Tree ? 0 : 1;
}

#endif // WITH_EDITOR
