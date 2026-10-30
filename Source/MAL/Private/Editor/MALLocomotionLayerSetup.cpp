// Copyright Epic Games, Inc. All Rights Reserved.

#include "MALLocomotionLayerSetup.h"

#if WITH_EDITOR

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimClassInterface.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimNode_LinkedAnimLayer.h"
#include "Animation/AnimNode_LinkedInputPose.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/MALAnimInstance.h"
#include "Animation/Skeleton.h"
#include "AnimGraphNode_AssetPlayerBase.h"
#include "AnimGraphNode_Inertialization.h"
#include "AnimGraphNode_LinkedAnimLayer.h"
#include "AnimGraphNode_LinkedInputPose.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_Slot.h"
#include "AnimGraphNode_StateMachine.h"
#include "AnimGraphNode_TransitionResult.h"
#include "AnimGraphNode_StateResult.h"
#include "AnimationGraph.h"
#include "AnimationGraphSchema.h"
#include "AnimationStateGraph.h"
#include "AnimationStateMachineGraph.h"
#include "AnimStateNode.h"
#include "AnimStateTransitionNode.h"
#include "AssetToolsModule.h"
#include "Character/MALCharacterBase.h"
#include "Combat/MALWeaponDefinition.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/MemberReference.h"
#include "Factories/AnimBlueprintFactory.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogMALLocomotionSetup, Log, All);

namespace MALLocomotionSetup
{
	const TCHAR* HostBlueprintPath = TEXT("/Game/Blueprint/Characters/MALCharacter/ABP_MALCharacter");
	const TCHAR* LayerFolderPath = TEXT("/Game/Blueprint/Characters/MALCharacter/Locomotion");
	const TCHAR* SkeletonPath = TEXT("/Game/ParagonKwang/Characters/Heroes/Kwang/Meshes/Kwang_Skeleton");
	const TCHAR* PlayerBlueprintPath = TEXT("/Game/Blueprint/Characters/MALCharacter/BP_MALCharacter");
	const TCHAR* InterfaceAsset = TEXT("ABP_LocomotionLayerInterface");

	const TCHAR* UnarmedLayerAsset = TEXT("ABP_Locomotion_Unarmed");
	const TCHAR* SwordLayerAsset = TEXT("ABP_Locomotion_Sword");
	const TCHAR* BowLayerAsset = TEXT("ABP_Locomotion_Bow");

	const TCHAR* IdleLayerFunction = TEXT("IdleLayer");
	const TCHAR* JogLayerFunction = TEXT("JogLayer");
	const TCHAR* SprintLayerFunction = TEXT("SprintLayer");

	const TCHAR* WeaponDefinitionPaths[] =
	{
		TEXT("/Game/Blueprint/Data/Weapons/Weapon/DA_Weapon_Sword"),
		TEXT("/Game/Blueprint/Data/Weapons/Weapon/DA_Weapon_Axe"),
		TEXT("/Game/Blueprint/Data/Weapons/Weapon/DA_Weapon_Dagger"),
		TEXT("/Game/Blueprint/Data/Weapons/Weapon/DA_Weapon_Hammer"),
		TEXT("/Game/Blueprint/Data/Weapons/Weapon/DA_Weapon_Scythe"),
		TEXT("/Game/Blueprint/Data/Weapons/Weapon/DA_Weapon_Spear"),
	};

	/** Blend times the engine reads from each layer function graph when it relinks a class. */
	constexpr float LayerBlendInTime = 0.15f;
	constexpr float LayerBlendOutTime = 0.2f;

	FString LayerAssetPath(const TCHAR* AssetName)
	{
		return FString::Printf(TEXT("%s/%s.%s"), LayerFolderPath, AssetName, AssetName);
	}

	FString LayerClassPath(const TCHAR* AssetName)
	{
		return LayerAssetPath(AssetName) + TEXT("_C");
	}

	FString InterfaceAssetPath()
	{
		return FString::Printf(TEXT("%s/%s.%s"), LayerFolderPath, InterfaceAsset, InterfaceAsset);
	}

	FString InterfaceClassPath()
	{
		return InterfaceAssetPath() + TEXT("_C");
	}

	bool IsPosePin(const UEdGraphPin* Pin)
	{
		return Pin
			&& Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct
			&& Pin->PinType.PinSubCategoryObject == FPoseLink::StaticStruct();
	}

	UEdGraphPin* FindPosePin(UEdGraphNode* Node, EEdGraphPinDirection Direction)
	{
		if (!Node)
		{
			return nullptr;
		}

		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->Direction == Direction && IsPosePin(Pin))
			{
				return Pin;
			}
		}

		return nullptr;
	}

	UEdGraphNode* GetLinkedPoseSource(UEdGraphPin* InputPin)
	{
		if (!InputPin || InputPin->LinkedTo.Num() == 0)
		{
			return nullptr;
		}

		const UEdGraphPin* SourcePin = InputPin->LinkedTo[0];
		return SourcePin ? SourcePin->GetOwningNode() : nullptr;
	}

	template <typename NodeType>
	NodeType* CreateGraphNode(UEdGraph* Graph, int32 PosX, int32 PosY)
	{
		FGraphNodeCreator<NodeType> NodeCreator(*Graph);
		NodeType* NewNode = NodeCreator.CreateNode(/*bSelectNewNode=*/false);
		NewNode->NodePosX = PosX;
		NewNode->NodePosY = PosY;
		NodeCreator.Finalize();
		return NewNode;
	}

	bool ConnectPins(UEdGraphPin* OutputPin, UEdGraphPin* InputPin)
	{
		if (!OutputPin || !InputPin)
		{
			return false;
		}

		if (OutputPin->LinkedTo.Contains(InputPin))
		{
			return true;
		}

		const UEdGraphSchema* Schema = OutputPin->GetSchema();
		const bool bConnected = Schema && Schema->TryCreateConnection(OutputPin, InputPin);
		UE_CLOG(!bConnected, LogMALLocomotionSetup, Warning, TEXT("Could not connect %s to %s."),
			*OutputPin->GetName(), *InputPin->GetName());
		return bConnected;
	}

	bool SaveAssetPackage(UObject* Asset)
	{
		if (!Asset)
		{
			return false;
		}

		UPackage* Package = Asset->GetOutermost();
		Package->MarkPackageDirty();

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;

		const FString FileName = FPackageName::LongPackageNameToFilename(
			Package->GetName(), FPackageName::GetAssetPackageExtension());

		const bool bSaved = UPackage::SavePackage(Package, Asset, *FileName, SaveArgs);
		UE_CLOG(!bSaved, LogMALLocomotionSetup, Error, TEXT("Failed to save %s."), *Package->GetName());
		return bSaved;
	}

	void CompileBlueprint(UBlueprint* Blueprint)
	{
		if (!Blueprint)
		{
			return;
		}

		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		UE_LOG(LogMALLocomotionSetup, Display, TEXT("%s compile status: %d"), *Blueprint->GetName(), static_cast<int32>(Blueprint->Status));
	}

	/** Returns the layer function an anim state machine state should hand its pose to. */
	FName PickLayerFunction(const FString& StateName)
	{
		if (StateName.Contains(TEXT("Sprint"), ESearchCase::IgnoreCase))
		{
			return FName(SprintLayerFunction);
		}

		if (StateName.Contains(TEXT("Jog"), ESearchCase::IgnoreCase)
			|| StateName.Contains(TEXT("Run"), ESearchCase::IgnoreCase)
			|| StateName.Contains(TEXT("Walk"), ESearchCase::IgnoreCase))
		{
			return FName(JogLayerFunction);
		}

		if (StateName.Contains(TEXT("Idle"), ESearchCase::IgnoreCase))
		{
			return FName(IdleLayerFunction);
		}

		return NAME_None;
	}

	UAnimationGraph* FindMainAnimGraph(const UAnimBlueprint* Blueprint)
	{
		if (!Blueprint)
		{
			return nullptr;
		}

		for (UEdGraph* Graph : Blueprint->FunctionGraphs)
		{
			UAnimationGraph* AnimGraph = Cast<UAnimationGraph>(Graph);
			if (AnimGraph && AnimGraph->GetFName() == UEdGraphSchema_K2::GN_AnimGraph)
			{
				return AnimGraph;
			}
		}

		return nullptr;
	}

	UAnimationGraph* FindInterfaceGraph(const UAnimBlueprint* Blueprint, FName FunctionName, UClass* InterfaceClass, FGuid* OutInterfaceGuid = nullptr)
	{
		if (!Blueprint)
		{
			return nullptr;
		}

		for (const FBPInterfaceDescription& Description : Blueprint->ImplementedInterfaces)
		{
			if (InterfaceClass && Description.Interface != InterfaceClass)
			{
				continue;
			}

			for (UEdGraph* Graph : Description.Graphs)
			{
				if (Graph && Graph->GetFName() == FunctionName)
				{
					if (OutInterfaceGuid)
					{
						*OutInterfaceGuid = Graph->InterfaceGuid;
					}

					return Cast<UAnimationGraph>(Graph);
				}
			}
		}

		return nullptr;
	}
}

using namespace MALLocomotionSetup;

namespace
{
	void DumpAnimGraphNodes(const UEdGraph* Graph, const FString& Indent)
	{
		if (!Graph)
		{
			return;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node)
			{
				continue;
			}

			UE_LOG(LogMALLocomotionSetup, Display, TEXT("%s%s (%s) at (%d, %d)"),
				*Indent, *Node->GetClass()->GetName(), *Node->GetName(), Node->NodePosX, Node->NodePosY);

			if (UEdGraphPin* PoseInput = FindPosePin(Node, EGPD_Input))
			{
				const UEdGraphNode* PoseSource = GetLinkedPoseSource(PoseInput);
				UE_LOG(LogMALLocomotionSetup, Display, TEXT("%s    pose input from: %s"),
					*Indent, PoseSource ? *PoseSource->GetName() : TEXT("nothing"));
			}

			if (const UAnimGraphNode_SequencePlayer* SequencePlayer = Cast<UAnimGraphNode_SequencePlayer>(Node))
			{
				const UAnimSequenceBase* Sequence = SequencePlayer->Node.GetSequence();
				UE_LOG(LogMALLocomotionSetup, Display, TEXT("%s    sequence: %s"),
					*Indent, Sequence ? *Sequence->GetPathName() : TEXT("none"));
			}

			if (const UAnimGraphNode_LinkedAnimLayer* LayerNode = Cast<UAnimGraphNode_LinkedAnimLayer>(Node))
			{
				UE_LOG(LogMALLocomotionSetup, Display, TEXT("%s    layer: %s interface: %s, notifies from linked instances: %s"),
					*Indent, *LayerNode->Node.Layer.ToString(),
					LayerNode->Node.Interface ? *LayerNode->Node.Interface->GetName() : TEXT("none"),
					LayerNode->Node.bReceiveNotifiesFromLinkedInstances ? TEXT("on") : TEXT("off"));
			}

			if (const UAnimGraphNode_TransitionResult* TransitionResult = Cast<UAnimGraphNode_TransitionResult>(Node))
			{
				for (const UEdGraphPin* Pin : TransitionResult->Pins)
				{
					if (Pin && Pin->Direction == EGPD_Input)
					{
						UE_LOG(LogMALLocomotionSetup, Display, TEXT("%s    rule pin '%s': %d link(s), default '%s'"),
							*Indent, *Pin->GetName(), Pin->LinkedTo.Num(), *Pin->GetDefaultAsString());
					}
				}
			}
		}
	}

	void DumpHostBlueprint(UAnimBlueprint* HostBlueprint)
	{
		UAnimationGraph* AnimGraph = FindMainAnimGraph(HostBlueprint);
		UE_LOG(LogMALLocomotionSetup, Display, TEXT("=== %s ==="), *HostBlueprint->GetName());
		UE_LOG(LogMALLocomotionSetup, Display, TEXT("parent class: %s"),
			HostBlueprint->ParentClass ? *HostBlueprint->ParentClass->GetName() : TEXT("none"));
		UE_LOG(LogMALLocomotionSetup, Display, TEXT("target skeleton: %s"),
			HostBlueprint->TargetSkeleton ? *HostBlueprint->TargetSkeleton->GetPathName() : TEXT("none"));
		UE_LOG(LogMALLocomotionSetup, Display, TEXT("implemented interfaces: %d"), HostBlueprint->ImplementedInterfaces.Num());

		for (const FBPInterfaceDescription& Description : HostBlueprint->ImplementedInterfaces)
		{
			UE_LOG(LogMALLocomotionSetup, Display, TEXT("  interface %s, graphs %d"),
				Description.Interface ? *Description.Interface->GetName() : TEXT("none"), Description.Graphs.Num());

			for (UEdGraph* Graph : Description.Graphs)
			{
				UE_LOG(LogMALLocomotionSetup, Display, TEXT("    graph %s (guid %s)"),
					Graph ? *Graph->GetName() : TEXT("none"),
					Graph ? *Graph->InterfaceGuid.ToString() : TEXT("none"));

				DumpAnimGraphNodes(Graph, TEXT("      "));
			}
		}

		UE_LOG(LogMALLocomotionSetup, Display, TEXT("--- function graphs ---"));
		for (UEdGraph* Graph : HostBlueprint->FunctionGraphs)
		{
			UE_LOG(LogMALLocomotionSetup, Display, TEXT("graph %s (%s)"), *Graph->GetName(), *Graph->GetClass()->GetName());
			if (Graph == AnimGraph)
			{
				DumpAnimGraphNodes(Graph, TEXT("  "));
			}
		}

		if (!AnimGraph)
		{
			return;
		}

		TArray<UAnimGraphNode_StateMachine*> StateMachines;
		AnimGraph->GetNodesOfClass<UAnimGraphNode_StateMachine>(StateMachines);
		for (UAnimGraphNode_StateMachine* StateMachine : StateMachines)
		{
			UAnimationStateMachineGraph* StateMachineGraph = StateMachine->EditorStateMachineGraph;
			UE_LOG(LogMALLocomotionSetup, Display, TEXT("--- state machine %s ---"), StateMachineGraph ? *StateMachineGraph->GetName() : TEXT("none"));

			if (!StateMachineGraph)
			{
				continue;
			}

			for (UEdGraphNode* Node : StateMachineGraph->Nodes)
			{
				UAnimStateNode* StateNode = Cast<UAnimStateNode>(Node);
				if (!StateNode)
				{
					continue;
				}

				UE_LOG(LogMALLocomotionSetup, Display, TEXT("  state '%s' graph %s -> layer %s"),
					*StateNode->GetStateName(),
					StateNode->BoundGraph ? *StateNode->BoundGraph->GetName() : TEXT("none"),
					*PickLayerFunction(StateNode->GetStateName()).ToString());

				if (StateNode->BoundGraph)
				{
					DumpAnimGraphNodes(StateNode->BoundGraph, TEXT("    "));
				}
			}

			for (UEdGraphNode* Node : StateMachineGraph->Nodes)
			{
				const UAnimStateTransitionNode* TransitionNode = Cast<UAnimStateTransitionNode>(Node);
				if (!TransitionNode)
				{
					continue;
				}

				auto StateNameOf = [](const UEdGraphPin* Pin)
				{
					if (!Pin || Pin->LinkedTo.Num() == 0)
					{
						return FString(TEXT("?"));
					}

					const UAnimStateNode* LinkedState = Cast<UAnimStateNode>(Pin->LinkedTo[0]->GetOwningNode());
					return LinkedState ? LinkedState->GetStateName() : FString(TEXT("?"));
				};

				UE_LOG(LogMALLocomotionSetup, Display, TEXT("  transition %s -> %s, crossfade %.2f, logic %d, automatic rule %s%s"),
					*StateNameOf(TransitionNode->GetInputPin()), *StateNameOf(TransitionNode->GetOutputPin()),
					TransitionNode->CrossfadeDuration, static_cast<int32>(TransitionNode->LogicType.GetValue()),
					TransitionNode->bAutomaticRuleBasedOnSequencePlayerInState ? TEXT("on") : TEXT("off"),
					TransitionNode->bDisabled ? TEXT(" (disabled)") : TEXT(""));

				DumpAnimGraphNodes(TransitionNode->BoundGraph, TEXT("      "));
			}
		}
	}

	// Reads the compiled class rather than the graph: the layer functions and the blend times
	// only exist there, and they are what the runtime actually links against.
	void DumpCompiledAnimClass(const FString& Label, UClass* AnimClass)
	{
		IAnimClassInterface* AnimClassInterface = IAnimClassInterface::GetFromClass(AnimClass);
		if (!AnimClassInterface)
		{
			UE_LOG(LogMALLocomotionSetup, Display, TEXT("=== %s === no anim class (asset not compiled)"), *Label);
			return;
		}

		UE_LOG(LogMALLocomotionSetup, Display, TEXT("=== %s === %s"), *Label, *AnimClass->GetName());

		for (const FAnimBlueprintFunction& Function : AnimClassInterface->GetAnimBlueprintFunctions())
		{
			UE_LOG(LogMALLocomotionSetup, Display, TEXT("  function '%s' %s"),
				*Function.Name.ToString(), Function.bImplemented ? TEXT("implemented") : TEXT("stub only"));
		}

		for (const TPair<FName, FAnimGraphBlendOptions>& Blending : AnimClassInterface->GetGraphBlendOptions())
		{
			UE_LOG(LogMALLocomotionSetup, Display, TEXT("  blend '%s': in %.2f out %.2f"),
				*Blending.Key.ToString(), Blending.Value.BlendInTime, Blending.Value.BlendOutTime);
		}
	}

	// Which layer a weapon resolves to lives on the assets, so the dump reads them instead of
	// trusting what an earlier apply pass reported.
	void DumpLocomotionData()
	{
		for (const TCHAR* DefinitionPath : WeaponDefinitionPaths)
		{
			const UMALWeaponDefinition* Definition = LoadObject<UMALWeaponDefinition>(nullptr, DefinitionPath);
			const UClass* LayerClass = Definition ? Definition->LocomotionLayer.LoadSynchronous() : nullptr;

			UE_LOG(LogMALLocomotionSetup, Display, TEXT("weapon %s -> locomotion layer %s"),
				DefinitionPath, LayerClass ? *LayerClass->GetName() : TEXT("none"));
		}

		const UBlueprint* PlayerBlueprint = LoadObject<UBlueprint>(nullptr, PlayerBlueprintPath);
		const AMALCharacterBase* Character = PlayerBlueprint && PlayerBlueprint->GeneratedClass
			? Cast<AMALCharacterBase>(PlayerBlueprint->GeneratedClass->GetDefaultObject())
			: nullptr;
		const UClass* DefaultLayer = Character ? Character->DefaultLocomotionLayer.LoadSynchronous() : nullptr;

		UE_LOG(LogMALLocomotionSetup, Display, TEXT("BP_MALCharacter default layer -> %s"),
			DefaultLayer ? *DefaultLayer->GetName() : TEXT("none"));
	}
}

// Creates (or tops up) the animation layer interface that the host and the layer blueprints talk
// through. It has to be an animation blueprint of type interface: UAnimGraphNode_LinkedAnimLayer
// casts the class generated by an interface to UAnimBlueprint, so a native UAnimLayerInterface
// cannot be used by a layer node.
static UAnimBlueprint* EnsureLayerInterfaceBlueprint(USkeleton* Skeleton, const TArray<FName>& FunctionNames)
{
	const FString FullPath = InterfaceAssetPath();
	UAnimBlueprint* Blueprint = LoadObject<UAnimBlueprint>(nullptr, *FullPath);
	if (!Blueprint)
	{
		UAnimLayerInterfaceFactory* Factory = NewObject<UAnimLayerInterfaceFactory>();
		Factory->BlueprintType = BPTYPE_Interface;
		Factory->TargetSkeleton = Skeleton;

		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		Blueprint = Cast<UAnimBlueprint>(AssetTools.CreateAsset(
			FString(InterfaceAsset), LayerFolderPath, UAnimBlueprint::StaticClass(), Factory));

		if (!Blueprint)
		{
			UE_LOG(LogMALLocomotionSetup, Error, TEXT("Failed to create %s."), *FullPath);
			return nullptr;
		}

		UE_LOG(LogMALLocomotionSetup, Display, TEXT("Created %s."), *FullPath);
	}

	for (const FName& FunctionName : FunctionNames)
	{
		const bool bHasGraph = Blueprint->FunctionGraphs.ContainsByPredicate([FunctionName](const UEdGraph* Graph)
		{
			return Graph && Graph->GetFName() == FunctionName;
		});

		if (bHasGraph)
		{
			continue;
		}

		UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(
			Blueprint, FunctionName, UAnimationGraph::StaticClass(), UAnimationGraphSchema::StaticClass());
		if (!Graph)
		{
			UE_LOG(LogMALLocomotionSetup, Error, TEXT("Failed to add the %s layer to %s."),
				*FunctionName.ToString(), *FullPath);
			continue;
		}

		// A domain specific graph gets the output pose node the layer function needs.
		FBlueprintEditorUtils::AddDomainSpecificGraph(Blueprint, Graph);

		TArray<UAnimGraphNode_Root*> RootNodes;
		Graph->GetNodesOfClass<UAnimGraphNode_Root>(RootNodes);
		UEdGraphPin* RootInput = RootNodes.Num() > 0 ? FindPosePin(RootNodes[0], EGPD_Input) : nullptr;

		// The input pose is what the linked layer hands to the graph; without it the layer
		// function would only declare an output pose and the host node would have no input.
		UAnimGraphNode_LinkedInputPose* InputPoseNode = CreateGraphNode<UAnimGraphNode_LinkedInputPose>(
			Graph,
			RootNodes.Num() > 0 ? RootNodes[0]->NodePosX - 400 : -400,
			RootNodes.Num() > 0 ? RootNodes[0]->NodePosY : 0);
		InputPoseNode->Node.Name = FAnimNode_LinkedInputPose::DefaultInputPoseName;
		ConnectPins(FindPosePin(InputPoseNode, EGPD_Output), RootInput);

		UE_LOG(LogMALLocomotionSetup, Display, TEXT("Added the %s layer to %s."),
			*FunctionName.ToString(), *FullPath);
	}

	return Blueprint;
}

// Fills in the interface entry and the layer function graphs the engine expects on a blueprint
// that takes part in Linked Anim Layers. FBlueprintEditorUtils::ImplementNewInterface is not used
// directly because it reads the interface functions straight from the interface class, which only
// carries them once the interface asset has been compiled.
static bool AddLocomotionInterface(UBlueprint* Blueprint, UClass* InterfaceClass, const TArray<FName>& FunctionNames)
{
	if (!Blueprint || !InterfaceClass)
	{
		return false;
	}

	FBPInterfaceDescription* Description = Blueprint->ImplementedInterfaces.FindByPredicate(
		[InterfaceClass](const FBPInterfaceDescription& Candidate)
		{
			return Candidate.Interface == InterfaceClass;
		});

	const bool bNewDescription = Description == nullptr;
	if (bNewDescription)
	{
		Description = &Blueprint->ImplementedInterfaces.AddDefaulted_GetRef();
		Description->Interface = InterfaceClass;
	}

	int32 CreatedGraphs = 0;
	for (const FName& FunctionName : FunctionNames)
	{
		const bool bHasGraph = Description->Graphs.ContainsByPredicate([FunctionName](const UEdGraph* Graph)
		{
			return Graph && Graph->GetFName() == FunctionName;
		});

		if (bHasGraph)
		{
			continue;
		}

		UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(
			Blueprint, FunctionName, UAnimationGraph::StaticClass(), UAnimationGraphSchema::StaticClass());
		if (!Graph)
		{
			UE_LOG(LogMALLocomotionSetup, Error, TEXT("Failed to create the %s graph on %s."), *FunctionName.ToString(), *Blueprint->GetName());
			return false;
		}

		Graph->bAllowDeletion = false;

		FGuid FunctionGuid;
		FBlueprintEditorUtils::GetFunctionGuidFromClassByFieldName(InterfaceClass, FunctionName, FunctionGuid);
		Graph->InterfaceGuid = FunctionGuid;

		// Creates the output pose node plus a linked input pose node for the pose-link parameter.
		FBlueprintEditorUtils::AddInterfaceGraph(Blueprint, Graph, InterfaceClass);
		Description->Graphs.Add(Graph);
		++CreatedGraphs;
	}

	if (bNewDescription || CreatedGraphs > 0)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}

	UE_LOG(LogMALLocomotionSetup, Display, TEXT("%s implements %s (%d new layer function graphs)."),
		*Blueprint->GetName(), *InterfaceClass->GetName(), CreatedGraphs);
	return true;
}


static void ConfigureLayerNode(UAnimGraphNode_LinkedAnimLayer* LayerNode, FName LayerName, UClass* InterfaceClass, const FGuid& InterfaceGuid)
{
	if (!LayerNode)
	{
		return;
	}

	// A non-null interface is what makes the node an externally linkable layer instead of a
	// self layer that always runs the host implementation.
	LayerNode->Node.Interface = InterfaceClass;
	LayerNode->Node.InstanceClass = nullptr;
	LayerNode->Node.bReceiveNotifiesFromLinkedInstances = true;

	// SetLayerName fills in Node.Layer and the external function reference the compiler reads,
	// but it is not exported from the AnimGraph module, so the reference is written through
	// reflection instead.
	LayerNode->Node.Layer = LayerName;
	if (const FStructProperty* ReferenceProperty = FindFProperty<FStructProperty>(LayerNode->GetClass(), TEXT("FunctionReference")))
	{
		if (FMemberReference* Reference = ReferenceProperty->ContainerPtrToValuePtr<FMemberReference>(LayerNode))
		{
			FGuid FunctionGuid;
			FBlueprintEditorUtils::GetFunctionGuidFromClassByFieldName(InterfaceClass, LayerName, FunctionGuid);
			Reference->SetExternalMember(LayerName, InterfaceClass, FunctionGuid);
		}
	}
	else
	{
		UE_LOG(LogMALLocomotionSetup, Error, TEXT("Could not write the layer node FunctionReference."));
	}

	LayerNode->InterfaceGuid = InterfaceGuid;

	// No ReconstructNode here: the input pose pins are hidden by default, so nothing they would
	// rebuild is needed, and reconstructing reaches into the editor debugger state, which is not
	// valid while running headless.
}

// A ground state no longer plays anything itself: its layer node is the only node wired to the
// state output. Any asset player left behind still ticks and fires its own notifies.
static int32 RemoveUnusedAssetPlayers(UAnimationStateGraph* StateGraph)
{
	TArray<UAnimGraphNode_AssetPlayerBase*> PlayerNodes;
	StateGraph->GetNodesOfClass<UAnimGraphNode_AssetPlayerBase>(PlayerNodes);

	int32 RemovedCount = 0;
	for (UAnimGraphNode_AssetPlayerBase* PlayerNode : PlayerNodes)
	{
		const UEdGraphPin* PoseOutput = FindPosePin(PlayerNode, EGPD_Output);
		if (PoseOutput && PoseOutput->LinkedTo.Num() > 0)
		{
			continue;
		}

		PlayerNode->BreakAllNodeLinks();
		StateGraph->RemoveNode(PlayerNode);
		++RemovedCount;
	}

	return RemovedCount;
}

// Wires every ground state of the host state machines to a linked anim layer node and records the
// player it replaced, so the layer blueprints can reuse the same animation set.
static bool ConfigureHostStateMachines(UAnimBlueprint* HostBlueprint, UClass* InterfaceClass, TMap<FName, FAnimNode_SequencePlayer>& OutGroundPlayers)
{
	UAnimationGraph* AnimGraph = FindMainAnimGraph(HostBlueprint);
	if (!AnimGraph)
	{
		UE_LOG(LogMALLocomotionSetup, Error, TEXT("No AnimGraph found on %s."), *HostBlueprint->GetName());
		return false;
	}

	bool bChanged = false;
	TArray<UAnimGraphNode_StateMachine*> StateMachines;
	AnimGraph->GetNodesOfClass<UAnimGraphNode_StateMachine>(StateMachines);

	for (UAnimGraphNode_StateMachine* StateMachine : StateMachines)
	{
		UAnimationStateMachineGraph* StateMachineGraph = StateMachine ? StateMachine->EditorStateMachineGraph : nullptr;
		if (!StateMachineGraph)
		{
			continue;
		}

		for (UEdGraphNode* StateMachineNode : StateMachineGraph->Nodes)
		{
			UAnimStateNode* StateNode = Cast<UAnimStateNode>(StateMachineNode);
			if (!StateNode || !StateNode->BoundGraph)
			{
				continue;
			}

			const FName LayerFunction = PickLayerFunction(StateNode->GetStateName());
			if (LayerFunction.IsNone())
			{
				UE_LOG(LogMALLocomotionSetup, Display, TEXT("State '%s' keeps its own graph (no layer)."), *StateNode->GetStateName());
				continue;
			}

			UAnimationStateGraph* StateGraph = Cast<UAnimationStateGraph>(StateNode->BoundGraph);
			if (!StateGraph)
			{
				continue;
			}

			// RemoveNode invalidates the node pointer, so work on a local copy of the array.
			TArray<UAnimGraphNode_LinkedAnimLayer*> ExistingLayerNodes;
			StateGraph->GetNodesOfClass<UAnimGraphNode_LinkedAnimLayer>(ExistingLayerNodes);
			if (ExistingLayerNodes.Num() > 0)
			{
				UE_LOG(LogMALLocomotionSetup, Display, TEXT("State '%s' already uses a layer node."), *StateNode->GetStateName());
				bChanged |= RemoveUnusedAssetPlayers(StateGraph) > 0;
				continue;
			}

			TArray<UAnimGraphNode_StateResult*> ResultNodes;
			StateGraph->GetNodesOfClass<UAnimGraphNode_StateResult>(ResultNodes);
			UAnimGraphNode_StateResult* ResultNode = ResultNodes.Num() > 0 ? ResultNodes[0] : nullptr;
			UEdGraphPin* ResultInput = ResultNode ? FindPosePin(ResultNode, EGPD_Input) : nullptr;
			if (!ResultInput)
			{
				UE_LOG(LogMALLocomotionSetup, Warning, TEXT("State '%s' has no output pose pin, skipping."), *StateNode->GetStateName());
				continue;
			}

			UAnimGraphNode_AssetPlayerBase* OldPlayer = Cast<UAnimGraphNode_AssetPlayerBase>(GetLinkedPoseSource(ResultInput));
			if (const UAnimGraphNode_SequencePlayer* OldSequencePlayer = Cast<UAnimGraphNode_SequencePlayer>(OldPlayer))
			{
				OutGroundPlayers.Add(LayerFunction, OldSequencePlayer->Node);

				const UAnimSequenceBase* Sequence = OldSequencePlayer->Node.GetSequence();
				UE_LOG(LogMALLocomotionSetup, Display, TEXT("State '%s' currently plays: %s"),
					*StateNode->GetStateName(), Sequence ? *Sequence->GetPathName() : TEXT("none"));
			}

			FGuid InterfaceGuid;
			FindInterfaceGraph(HostBlueprint, LayerFunction, InterfaceClass, &InterfaceGuid);

			const int32 NodePosX = OldPlayer ? OldPlayer->NodePosX : ResultNode->NodePosX - 400;
			const int32 NodePosY = OldPlayer ? OldPlayer->NodePosY : ResultNode->NodePosY;
			UAnimGraphNode_LinkedAnimLayer* LayerNode = CreateGraphNode<UAnimGraphNode_LinkedAnimLayer>(StateGraph, NodePosX, NodePosY);
			ConfigureLayerNode(LayerNode, LayerFunction, InterfaceClass, InterfaceGuid);

			if (OldPlayer)
			{
				OldPlayer->BreakAllNodeLinks();
				StateGraph->RemoveNode(OldPlayer);
			}

			ConnectPins(FindPosePin(LayerNode, EGPD_Output), ResultInput);
			RemoveUnusedAssetPlayers(StateGraph);
			bChanged = true;
			UE_LOG(LogMALLocomotionSetup, Display, TEXT("State '%s' now uses the %s layer node."),
				*StateNode->GetStateName(), *LayerFunction.ToString());
		}
	}

	return bChanged;
}

// Inserts the Inertialization node between the montage slot and the graph output. The engine only
// sends the blend request that makes a layer swap smooth when such a node is present.
static bool InsertInertializationNode(UAnimationGraph* AnimGraph)
{
	TArray<UAnimGraphNode_Root*> RootNodes;
	TArray<UAnimGraphNode_Slot*> SlotNodes;
	AnimGraph->GetNodesOfClass<UAnimGraphNode_Root>(RootNodes);
	AnimGraph->GetNodesOfClass<UAnimGraphNode_Slot>(SlotNodes);

	UAnimGraphNode_Root* RootNode = RootNodes.Num() > 0 ? RootNodes[0] : nullptr;
	UAnimGraphNode_Slot* SlotNode = SlotNodes.Num() > 0 ? SlotNodes[0] : nullptr;
	UEdGraphPin* RootInput = RootNode ? FindPosePin(RootNode, EGPD_Input) : nullptr;
	UEdGraphPin* SlotOutput = SlotNode ? FindPosePin(SlotNode, EGPD_Output) : nullptr;

	if (!RootInput || !SlotOutput)
	{
		UE_LOG(LogMALLocomotionSetup, Warning, TEXT("Root or Slot node missing, Inertialization not inserted."));
		return false;
	}

	TArray<UAnimGraphNode_Inertialization*> ExistingNodes;
	AnimGraph->GetNodesOfClass<UAnimGraphNode_Inertialization>(ExistingNodes);

	UAnimGraphNode_Inertialization* InertializationNode = ExistingNodes.Num() > 0
		? ExistingNodes[0]
		: CreateGraphNode<UAnimGraphNode_Inertialization>(AnimGraph, SlotNode->NodePosX + 250, SlotNode->NodePosY);

	UEdGraphPin* InertializationInput = FindPosePin(InertializationNode, EGPD_Input);
	UEdGraphPin* InertializationOutput = FindPosePin(InertializationNode, EGPD_Output);
	if (!InertializationInput || !InertializationOutput)
	{
		UE_LOG(LogMALLocomotionSetup, Warning, TEXT("The Inertialization node has no pose pins."));
		return false;
	}

	SlotOutput->BreakAllPinLinks();
	RootInput->BreakAllPinLinks();
	InertializationOutput->BreakAllPinLinks();

	ConnectPins(SlotOutput, InertializationInput);
	ConnectPins(InertializationOutput, RootInput);

	UE_LOG(LogMALLocomotionSetup, Display, TEXT("Inserted Inertialization: Slot -> Inertialization -> Root."));
	return true;
}

// Host layer functions are never played (the nodes always name an external interface), but the
// graph still has to compile, so the input pose is wired straight to the output pose.
static void WirePassThroughGraph(UAnimationGraph* Graph)
{
	if (!Graph)
	{
		return;
	}

	TArray<UAnimGraphNode_Root*> RootNodes;
	TArray<UAnimGraphNode_LinkedInputPose*> InputPoseNodes;
	Graph->GetNodesOfClass<UAnimGraphNode_Root>(RootNodes);
	Graph->GetNodesOfClass<UAnimGraphNode_LinkedInputPose>(InputPoseNodes);

	UEdGraphPin* RootInput = RootNodes.Num() > 0 ? FindPosePin(RootNodes[0], EGPD_Input) : nullptr;
	UEdGraphPin* InputPoseOutput = InputPoseNodes.Num() > 0 ? FindPosePin(InputPoseNodes[0], EGPD_Output) : nullptr;

	if (ConnectPins(InputPoseOutput, RootInput))
	{
		UE_LOG(LogMALLocomotionSetup, Display, TEXT("Layer graph %s wired as a pass-through (the host never plays it)."), *Graph->GetName());
	}
}

// Whether a layer graph already drives its output pose from an animation.
static bool HasWiredPlayer(const UAnimationGraph* Graph)
{
	TArray<UAnimGraphNode_AssetPlayerBase*> PlayerNodes;
	Graph->GetNodesOfClass<UAnimGraphNode_AssetPlayerBase>(PlayerNodes);

	for (UAnimGraphNode_AssetPlayerBase* PlayerNode : PlayerNodes)
	{
		const UEdGraphPin* PoseOutput = FindPosePin(PlayerNode, EGPD_Output);
		if (PoseOutput && PoseOutput->LinkedTo.Num() > 0)
		{
			return true;
		}
	}

	return false;
}

static UAnimBlueprint* CreateLayerBlueprint(const TCHAR* AssetName, USkeleton* Skeleton)
{
	const FString FullPath = LayerAssetPath(AssetName);
	if (UAnimBlueprint* Existing = LoadObject<UAnimBlueprint>(nullptr, *FullPath))
	{
		UE_LOG(LogMALLocomotionSetup, Display, TEXT("%s already exists."), *FullPath);
		return Existing;
	}

	UAnimBlueprintFactory* Factory = NewObject<UAnimBlueprintFactory>();
	Factory->BlueprintType = BPTYPE_Normal;
	Factory->ParentClass = UMALAnimInstance::StaticClass();
	Factory->TargetSkeleton = Skeleton;

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UObject* Created = AssetTools.CreateAsset(FString(AssetName), LayerFolderPath, UAnimBlueprint::StaticClass(), Factory);
	UAnimBlueprint* LayerBlueprint = Cast<UAnimBlueprint>(Created);

	UE_CLOG(!LayerBlueprint, LogMALLocomotionSetup, Error, TEXT("Failed to create %s."), *FullPath);
	return LayerBlueprint;
}

static bool PopulateLayerFunctionGraph(UAnimationGraph* Graph, const FAnimNode_SequencePlayer& SourcePlayer)
{
	TArray<UAnimGraphNode_Root*> RootNodes;
	Graph->GetNodesOfClass<UAnimGraphNode_Root>(RootNodes);
	UEdGraphPin* RootInput = RootNodes.Num() > 0 ? FindPosePin(RootNodes[0], EGPD_Input) : nullptr;
	if (!RootInput)
	{
		UE_LOG(LogMALLocomotionSetup, Warning, TEXT("Layer graph %s has no output pose node."), *Graph->GetName());
		return false;
	}

	TArray<UAnimGraphNode_SequencePlayer*> PlayerNodes;
	Graph->GetNodesOfClass<UAnimGraphNode_SequencePlayer>(PlayerNodes);

	UAnimGraphNode_SequencePlayer* PlayerNode = PlayerNodes.Num() > 0 ? PlayerNodes[0] : nullptr;
	if (!PlayerNode)
	{
		PlayerNode = CreateGraphNode<UAnimGraphNode_SequencePlayer>(
			Graph, RootNodes[0]->NodePosX - 350, RootNodes[0]->NodePosY);
		PlayerNode->Node = SourcePlayer;
	}

	if (!PlayerNode->Node.GetSequence())
	{
		UE_LOG(LogMALLocomotionSetup, Warning, TEXT("The player in layer graph %s has no animation."), *Graph->GetName());
		return false;
	}

	Graph->BlendOptions.BlendInTime = LayerBlendInTime;
	Graph->BlendOptions.BlendOutTime = LayerBlendOutTime;

	return ConnectPins(FindPosePin(PlayerNode, EGPD_Output), RootInput);
}

static void ConfigureWeaponDefinitions(const FString& SwordLayerClassPath)
{
	int32 ConfiguredCount = 0;

	for (const TCHAR* DefinitionPath : WeaponDefinitionPaths)
	{
		UMALWeaponDefinition* Definition = LoadObject<UMALWeaponDefinition>(nullptr, DefinitionPath);
		if (!Definition)
		{
			UE_LOG(LogMALLocomotionSetup, Warning, TEXT("Weapon definition %s not found."), DefinitionPath);
			continue;
		}

		Definition->LocomotionLayer = TSoftClassPtr<UAnimInstance>(FSoftObjectPath(SwordLayerClassPath));
		Definition->MarkPackageDirty();
		SaveAssetPackage(Definition);
		++ConfiguredCount;
	}

	UE_LOG(LogMALLocomotionSetup, Display, TEXT("LocomotionLayer set on %d weapon definitions."), ConfiguredCount);
}

static void ConfigurePlayerBlueprint(const FString& UnarmedLayerClassPath)
{
	UBlueprint* PlayerBlueprint = LoadObject<UBlueprint>(nullptr, PlayerBlueprintPath);
	UClass* GeneratedClass = PlayerBlueprint ? PlayerBlueprint->GeneratedClass : nullptr;
	AMALCharacterBase* CharacterCDO = GeneratedClass ? Cast<AMALCharacterBase>(GeneratedClass->GetDefaultObject()) : nullptr;

	if (!CharacterCDO)
	{
		UE_LOG(LogMALLocomotionSetup, Error, TEXT("BP_MALCharacter has no MAL character default object."));
		return;
	}

	CharacterCDO->DefaultLocomotionLayer = TSoftClassPtr<UAnimInstance>(FSoftObjectPath(UnarmedLayerClassPath));
	CharacterCDO->MarkPackageDirty();
	SaveAssetPackage(PlayerBlueprint);
	UE_LOG(LogMALLocomotionSetup, Display, TEXT("BP_MALCharacter DefaultLocomotionLayer = %s"), *UnarmedLayerClassPath);
}

UMALLocomotionLayerSetupCommandlet::UMALLocomotionLayerSetupCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UMALLocomotionLayerSetupCommandlet::Main(const FString& Params)
{
	FString Mode = TEXT("apply");
	FParse::Value(*Params, TEXT("Mode="), Mode);
	const bool bDumpOnly = Mode.Equals(TEXT("dump"), ESearchCase::IgnoreCase);
	const bool bCompileOnly = Mode.Equals(TEXT("compile"), ESearchCase::IgnoreCase);

	// Diagnostics switches: the graph is written to disk before it is compiled, so a compile
	// failure can be bisected without losing the edit.
	const bool bSkipCompile = FParse::Param(*Params, TEXT("SkipCompile"));
	const bool bSkipStateMachines = FParse::Param(*Params, TEXT("SkipStateMachines"));
	const bool bSkipInertialization = FParse::Param(*Params, TEXT("SkipInertialization"));

	UAnimBlueprint* HostBlueprint = LoadObject<UAnimBlueprint>(nullptr, HostBlueprintPath);
	if (!HostBlueprint)
	{
		UE_LOG(LogMALLocomotionSetup, Error, TEXT("%s not found."), HostBlueprintPath);
		return 1;
	}

	USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, SkeletonPath);
	if (!Skeleton)
	{
		UE_LOG(LogMALLocomotionSetup, Error, TEXT("%s is missing."), SkeletonPath);
		return 1;
	}

	DumpHostBlueprint(HostBlueprint);

	if (bDumpOnly)
	{
		DumpCompiledAnimClass(HostBlueprint->GetName(), HostBlueprint->GeneratedClass.Get());
		if (UAnimBlueprint* InterfaceBlueprint = LoadObject<UAnimBlueprint>(nullptr, *InterfaceAssetPath()))
		{
			DumpHostBlueprint(InterfaceBlueprint);
			DumpCompiledAnimClass(InterfaceBlueprint->GetName(), InterfaceBlueprint->GeneratedClass.Get());
		}

		for (const TCHAR* LayerAsset : {UnarmedLayerAsset, SwordLayerAsset, BowLayerAsset})
		{
			if (UAnimBlueprint* LayerBlueprint = LoadObject<UAnimBlueprint>(nullptr, *LayerAssetPath(LayerAsset)))
			{
				DumpHostBlueprint(LayerBlueprint);
				DumpCompiledAnimClass(LayerBlueprint->GetName(), LayerBlueprint->GeneratedClass.Get());
			}
		}

		DumpLocomotionData();

		return 0;
	}

	const TArray<FName> LayerFunctions = {
		FName(IdleLayerFunction), FName(JogLayerFunction), FName(SprintLayerFunction)};
	const TCHAR* LayerAssets[] = {UnarmedLayerAsset, SwordLayerAsset, BowLayerAsset};

	// The interface has to exist and be compiled before anything implements it: its layer
	// functions are only generated from its graphs once the asset has been compiled.
	UAnimBlueprint* InterfaceBlueprint = EnsureLayerInterfaceBlueprint(Skeleton, LayerFunctions);
	if (!InterfaceBlueprint)
	{
		return 1;
	}

	CompileBlueprint(InterfaceBlueprint);
	SaveAssetPackage(InterfaceBlueprint);

	UClass* InterfaceClass = InterfaceBlueprint->GeneratedClass.Get();
	if (!InterfaceClass)
	{
		InterfaceClass = InterfaceBlueprint->SkeletonGeneratedClass.Get();
	}

	if (!InterfaceClass)
	{
		UE_LOG(LogMALLocomotionSetup, Error, TEXT("%s has no generated interface class."), InterfaceAsset);
		return 1;
	}

	UE_LOG(LogMALLocomotionSetup, Display, TEXT("Layer interface class is %s."), *InterfaceClass->GetName());

	if (bCompileOnly)
	{
		CompileBlueprint(HostBlueprint);
		SaveAssetPackage(HostBlueprint);

		for (const TCHAR* LayerAsset : LayerAssets)
		{
			if (UAnimBlueprint* LayerBlueprint = LoadObject<UAnimBlueprint>(nullptr, *LayerAssetPath(LayerAsset)))
			{
				CompileBlueprint(LayerBlueprint);
				SaveAssetPackage(LayerBlueprint);
			}
		}

		return 0;
	}

	// The host has to implement the interface for the layer nodes to resolve their target class.
	AddLocomotionInterface(HostBlueprint, InterfaceClass, LayerFunctions);
	for (const FName& LayerFunction : LayerFunctions)
	{
		if (UAnimationGraph* HostLayerGraph = FindInterfaceGraph(HostBlueprint, LayerFunction, InterfaceClass))
		{
			WirePassThroughGraph(HostLayerGraph);
		}
	}

	// Collect the animations the host plays today, then hand the ground states to the layer.
	TMap<FName, FAnimNode_SequencePlayer> GroundPlayers;
	if (!bSkipStateMachines)
	{
		ConfigureHostStateMachines(HostBlueprint, InterfaceClass, GroundPlayers);
	}

	if (!bSkipInertialization)
	{
		InsertInertializationNode(FindMainAnimGraph(HostBlueprint));
	}

	SaveAssetPackage(HostBlueprint);

	if (!bSkipCompile)
	{
		CompileBlueprint(HostBlueprint);
		SaveAssetPackage(HostBlueprint);
	}

	for (const TCHAR* LayerAsset : LayerAssets)
	{
		UAnimBlueprint* LayerBlueprint = CreateLayerBlueprint(LayerAsset, Skeleton);
		if (!LayerBlueprint)
		{
			continue;
		}

		AddLocomotionInterface(LayerBlueprint, InterfaceClass, LayerFunctions);

		for (const FName& LayerFunction : LayerFunctions)
		{
			UAnimationGraph* LayerGraph = FindInterfaceGraph(LayerBlueprint, LayerFunction, InterfaceClass);
			if (!LayerGraph)
			{
				UE_LOG(LogMALLocomotionSetup, Warning, TEXT("%s has no %s layer graph."), LayerAsset, *LayerFunction.ToString());
				continue;
			}

			const FAnimNode_SequencePlayer* SourcePlayer = GroundPlayers.Find(LayerFunction);

			// The host has no sprint state yet, so the first pass reuses the jog set for sprint
			// instead of leaving the layer graph empty (an unwired output pose fails the compile).
			if (!SourcePlayer)
			{
				SourcePlayer = GroundPlayers.Find(FName(JogLayerFunction));
			}

			if (!SourcePlayer)
			{
				SourcePlayer = GroundPlayers.Find(FName(IdleLayerFunction));
			}

			if (!SourcePlayer)
			{
				// Second run: the host states already point at the layer, so there is nothing to copy
				// from. The graph keeps what the first run filled in.
				UE_CLOG(!HasWiredPlayer(LayerGraph), LogMALLocomotionSetup, Warning,
					TEXT("%s/%s has no animation to reuse, set it by hand."), LayerAsset, *LayerFunction.ToString());
				UE_CLOG(HasWiredPlayer(LayerGraph), LogMALLocomotionSetup, Display,
					TEXT("%s/%s already has its animation."), LayerAsset, *LayerFunction.ToString());
				continue;
			}

			const UAnimSequenceBase* SourceSequence = SourcePlayer->GetSequence();
			UE_LOG(LogMALLocomotionSetup, Display, TEXT("%s/%s uses animation %s"), LayerAsset, *LayerFunction.ToString(),
				SourceSequence ? *SourceSequence->GetPathName() : TEXT("none"));

			PopulateLayerFunctionGraph(LayerGraph, *SourcePlayer);
		}

		SaveAssetPackage(LayerBlueprint);

		if (!bSkipCompile)
		{
			CompileBlueprint(LayerBlueprint);
			SaveAssetPackage(LayerBlueprint);
		}
	}

	ConfigureWeaponDefinitions(LayerClassPath(SwordLayerAsset));
	ConfigurePlayerBlueprint(LayerClassPath(UnarmedLayerAsset));

	UE_LOG(LogMALLocomotionSetup, Display, TEXT("Done: layer assets, host graph nodes and data assets written."));
	return 0;
}

#else

UMALLocomotionLayerSetupCommandlet::UMALLocomotionLayerSetupCommandlet()
{
	IsEditor = false;
}

int32 UMALLocomotionLayerSetupCommandlet::Main(const FString& Params)
{
	return 0;
}

#endif // WITH_EDITOR
