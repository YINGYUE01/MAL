// Copyright Epic Games, Inc. All Rights Reserved.

#include "MAL.h"
#include "Modules/ModuleManager.h"
#include "Tags/MALGameplayTags.h"

#if WITH_GAMEPLAY_DEBUGGER
#include "GameplayDebugger.h"
#include "Debug/MALGameplayDebuggerCategory.h"
#endif

namespace
{
	const FName MALAIDebuggerCategoryName(TEXT("MAL.AI"));

	// Slots 0-2 are taken by Navmesh, AI and BehaviorTree, and 3-5 get auto-assigned to
	// EQS, Perception and PerceptionSystem. Six keeps the page on a key that never moves.
	constexpr int32 MALAIDebuggerCategorySlot = 6;
}

class FMALModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		// Force early initialization of gameplay tags before DoneAddingNativeTags
		UMALGameplayTags::Get();

#if WITH_GAMEPLAY_DEBUGGER
		// The engine pages show the behavior tree and perception internals but know nothing about
		// the blackboard keys this project drives, so add a page that reports them. Registration
		// defaults to Disabled, which would leave the page invisible until someone found the
		// project settings entry, so enable it outright.
		IGameplayDebugger::Get().RegisterCategory(
			MALAIDebuggerCategoryName,
			IGameplayDebugger::FOnGetCategory::CreateStatic(&FMALGameplayDebuggerCategory::MakeInstance),
			EGameplayDebuggerCategoryState::EnabledInGameAndSimulate,
			MALAIDebuggerCategorySlot);

		UE_LOG(LogTemp, Log, TEXT("MAL: gameplay debugger page '%s' registered on slot %d"),
			*MALAIDebuggerCategoryName.ToString(), MALAIDebuggerCategorySlot);
#endif
	}

	virtual void ShutdownModule() override
	{
#if WITH_GAMEPLAY_DEBUGGER
		if (IGameplayDebugger::IsAvailable())
		{
			IGameplayDebugger::Get().UnregisterCategory(MALAIDebuggerCategoryName);
		}
#endif
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(
	FMALModule,
	MAL,
	"MAL"
);
