// Copyright Epic Games, Inc. All Rights Reserved.

#include "MALUpperBodyLayerSetup.h"

#if WITH_EDITOR

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimClassInterface.h"
#include "Animation/AnimData/BoneMaskFilter.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/MALAnimInstance.h"
#include "Animation/Skeleton.h"
#include "AnimGraphNode_BlendListByBool.h"
#include "AnimGraphNode_BlendSpacePlayer.h"
#include "AnimGraphNode_Inertialization.h"
#include "AnimGraphNode_LayeredBoneBlend.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_Slot.h"
#include "AnimationGraph.h"
#include "AnimationGraphSchema.h"
#include "AssetToolsModule.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Factories/BlendSpaceFactoryNew.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogMALUpperBodySetup, Log, All);

namespace MALUpperBodySetup
{
	const TCHAR* HostBlueprintPath = TEXT("/Game/Blueprint/Characters/MALCharacter/ABP_MALCharacter");
	const TCHAR* BowLayerPath = TEXT("/Game/Blueprint/Characters/MALCharacter/Locomotion/ABP_Locomotion_Bow");
	const TCHAR* LayerFolderPath = TEXT("/Game/Blueprint/Characters/MALCharacter/Locomotion");
	const TCHAR* SkeletonPath = TEXT("/Game/ParagonKwang/Characters/Heroes/Kwang/Meshes/Kwang_Skeleton");
	const TCHAR* BowMontagePath = TEXT("/Game/Blueprint/GameAbilitySyste/Ability/Archer/AM_ShotMontage");

	const TCHAR* BlendSpaceAsset = TEXT("BS_Bow_Move");

	/**
	 * Slot the shot montage plays in. The host blends this slot onto the spine and up, so the
	 * montage no longer owns the legs.
	 */
	const TCHAR* UpperBodySlotName = TEXT("UpperBodySlot");

	/** Slot the melee montages keep using: still drives the whole pose through the base branch. */
	const TCHAR* FullBodySlotName = TEXT("DefaultSlot");

	/**
	 * Bone the upper-body half starts at. Everything from here down the hierarchy (spine, arms,
	 * neck, head) comes from the slot; pelvis and legs stay with locomotion.
	 */
	const TCHAR* UpperBodyRootBone = TEXT("spine_01");

	/** Layer functions ABP_Locomotion_Bow implements; all of them get the blend space. */
	const TCHAR* LayerFunctionNames[] = { TEXT("IdleLayer"), TEXT("JogLayer"), TEXT("SprintLayer") };

	/** Variables computed by UMALAnimInstance, used as the blend space axes. */
	const FName SpeedVariable = TEXT("Speed");
	const FName DirectionVariable = TEXT("Direction");

	/** Stance flag UMALAnimInstance publishes; the bow layer branches its lower body on it. */
	const FName AimingVariable = TEXT("bIsAiming");

	/**
	 * The lower body each bow layer plays when the character is *not* aiming.
	 *
	 * Carrying a bow is not shooting: the directional aim set belongs to the stance, so the plain
	 * forward set stays in charge the rest of the time, exactly as it did before the split.
	 */
	struct FPlantedAnimation
	{
		const TCHAR* FunctionName;
		const TCHAR* SequencePath;
	};

	const FPlantedAnimation PlantedAnimations[] =
	{
		{ TEXT("IdleLayer"), TEXT("/Game/Frank_RPG_Archer/Animations/Frank_Archer_IP/Frank_RPG_Archer_Idle_IP") },
		{ TEXT("JogLayer"), TEXT("/Game/Frank_RPG_Archer/Animations/Frank_Archer_IP/Frank_RPG_Archer_Run01_IP") },
		{ TEXT("SprintLayer"), TEXT("/Game/Frank_RPG_Archer/Animations/Frank_Archer_IP/Frank_RPG_Archer_Run02_IP") },
	};

	/**
	 * One art-directed move: the animation, the speed its feet are authored for and the direction
	 * it faces relative to the character.
	 *
	 * The side and back samples sit at walk speed only. The pack's 8-way run set has no in-place
	 * version, so a strafe at jog speed still falls back to walk footfalls; that is the known
	 * limitation of this pass, not an oversight.
	 */
	struct FMoveSample
	{
		const TCHAR* SequencePath;
		float Speed;
		float Direction;
	};

	const FMoveSample MoveSamples[] =
	{
		{ TEXT("/Game/Frank_RPG_Archer/Animations/Frank_Archer_IP/Frank_RPG_Archer_Idle_IP"), 0.f, 0.f },
		{ TEXT("/Game/Frank_RPG_Archer/Animations/Frank_Archer_IP/Frank_RPG_Archer_4Way_Walk_F_IP"), 150.f, 0.f },
		{ TEXT("/Game/Frank_RPG_Archer/Animations/Frank_Archer_IP/Frank_RPG_Archer_4Way_Walk_B_IP"), 150.f, 180.f },
		{ TEXT("/Game/Frank_RPG_Archer/Animations/Frank_Archer_IP/Frank_RPG_Archer_4Way_Walk_L_IP"), 150.f, -90.f },
		{ TEXT("/Game/Frank_RPG_Archer/Animations/Frank_Archer_IP/Frank_RPG_Archer_4Way_Walk_R_IP"), 150.f, 90.f },
		{ TEXT("/Game/Frank_RPG_Archer/Animations/Frank_Archer_IP/Frank_RPG_Archer_Run01_IP"), 400.f, 0.f },
		{ TEXT("/Game/Frank_RPG_Archer/Animations/Frank_Archer_IP/Frank_RPG_Archer_Run02_IP"), 700.f, 0.f },
	};

	constexpr float SpeedAxisMax = 700.f;

	// --- Graph helpers -------------------------------------------------------------------

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

	/**
	 * Pose pin by name, then by position among the pose inputs.
	 *
	 * The blend node exposes its base pose as "BasePose" and each layer as "BlendPoses_<index>",
	 * which the editor builds from the node struct's property names. The positional fallback keeps
	 * the pass working if that naming ever changes.
	 */
	UEdGraphPin* FindPosePinNamed(UEdGraphNode* Node, FName PinName, EEdGraphPinDirection Direction, int32 FallbackIndex = INDEX_NONE)
	{
		if (!Node)
		{
			return nullptr;
		}

		if (UEdGraphPin* Named = Node->FindPin(PinName, Direction))
		{
			if (IsPosePin(Named))
			{
				return Named;
			}
		}

		if (FallbackIndex == INDEX_NONE)
		{
			return nullptr;
		}

		int32 PoseIndex = 0;
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || Pin->Direction != Direction || !IsPosePin(Pin))
			{
				continue;
			}

			if (PoseIndex == FallbackIndex)
			{
				return Pin;
			}

			++PoseIndex;
		}

		return nullptr;
	}

	/** The bool input of a blend-by-bool node, named after its property as the schema builds it. */
	UEdGraphPin* FindBoolPin(UEdGraphNode* Node, FName PreferredName = TEXT("bActiveValue"))
	{
		if (!Node)
		{
			return nullptr;
		}

		if (UEdGraphPin* Named = Node->FindPin(PreferredName, EGPD_Input))
		{
			return Named;
		}

		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->Direction == EGPD_Input && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Boolean)
			{
				return Pin;
			}
		}

		return nullptr;
	}

	UEdGraphNode* GetLinkedPoseSource(const UEdGraphPin* InputPin)
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

	/**
	 * Variable getter for the anim graph.
	 *
	 * The pins are allocated in Finalize, so the variable has to be named before that; naming it
	 * afterwards would leave the node with a pin that no longer matches its member reference.
	 */
	UK2Node_VariableGet* CreateVariableGetter(UEdGraph* Graph, FName VariableName, int32 PosX, int32 PosY)
	{
		UK2Node_VariableGet* Getter = nullptr;

		FGraphNodeCreator<UK2Node_VariableGet> NodeCreator(*Graph);
		Getter = NodeCreator.CreateNode(/*bSelectNewNode=*/false);
		Getter->NodePosX = PosX;
		Getter->NodePosY = PosY;
		Getter->VariableReference.SetSelfMember(VariableName);
		NodeCreator.Finalize();

		return Getter;
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
		UE_CLOG(!bConnected, LogMALUpperBodySetup, Warning, TEXT("Could not connect %s to %s."),
			*OutputPin->GetName(), *InputPin->GetName());
		return bConnected;
	}

	/**
	 * Second consumer of an already connected pose output.
	 *
	 * UAnimationGraphSchema keeps pose pins a tree: connecting an output that already drives an
	 * input answers BREAK_OTHERS_AB, so the previous link is dropped and the last connection wins.
	 * The upper body split is the one case that genuinely needs the same pose twice - once as the
	 * blend base and once as the slot's source - so the link is written directly instead. The
	 * runtime is fine with it: the compiler emits one pose link per input pin, so a forked output is
	 * simply evaluated once per consumer. The extra cost is one pass over the locomotion branch.
	 */
	bool ForceConnectPins(UEdGraphPin* OutputPin, UEdGraphPin* InputPin)
	{
		if (!OutputPin || !InputPin)
		{
			return false;
		}

		if (OutputPin->LinkedTo.Contains(InputPin))
		{
			return true;
		}

		OutputPin->MakeLinkTo(InputPin);

		if (UEdGraph* Graph = OutputPin->GetOwningNode()->GetGraph())
		{
			Graph->NotifyGraphChanged();
		}

		UE_LOG(LogMALUpperBodySetup, Display, TEXT("Forked %s to %s (second consumer of the pose)."),
			*OutputPin->GetOwningNode()->GetName(), *InputPin->GetOwningNode()->GetName());
		return OutputPin->LinkedTo.Contains(InputPin);
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
		UE_CLOG(!bSaved, LogMALUpperBodySetup, Error, TEXT("Failed to save %s."), *Package->GetName());
		return bSaved;
	}

	void CompileBlueprint(UBlueprint* Blueprint)
	{
		if (!Blueprint)
		{
			return;
		}

		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		UE_LOG(LogMALUpperBodySetup, Display, TEXT("%s compile status: %d"),
			*Blueprint->GetName(), static_cast<int32>(Blueprint->Status));
	}

	UAnimationGraph* FindAnimGraph(const UAnimBlueprint* Blueprint, FName GraphName = UEdGraphSchema_K2::GN_AnimGraph)
	{
		if (!Blueprint)
		{
			return nullptr;
		}

		for (const FBPInterfaceDescription& Description : Blueprint->ImplementedInterfaces)
		{
			for (UEdGraph* Graph : Description.Graphs)
			{
				if (Graph && Graph->GetFName() == GraphName)
				{
					return Cast<UAnimationGraph>(Graph);
				}
			}
		}

		for (UEdGraph* Graph : Blueprint->FunctionGraphs)
		{
			UAnimationGraph* AnimGraph = Cast<UAnimationGraph>(Graph);
			if (AnimGraph && AnimGraph->GetFName() == GraphName)
			{
				return AnimGraph;
			}
		}

		return nullptr;
	}

	// --- Dump ----------------------------------------------------------------------------

	void DumpGraph(const UEdGraph* Graph, const FString& Indent)
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

			UE_LOG(LogMALUpperBodySetup, Display, TEXT("%s%s at (%d, %d)"),
				*Indent, *Node->GetClass()->GetName(), Node->NodePosX, Node->NodePosY);

			for (const UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin || !IsPosePin(Pin))
				{
					continue;
				}

				const FString Target = Pin->LinkedTo.Num() > 0 && Pin->LinkedTo[0]
					? Pin->LinkedTo[0]->GetOwningNode()->GetName()
					: FString(TEXT("nothing"));

				UE_LOG(LogMALUpperBodySetup, Display, TEXT("%s    pose pin '%s' (%s) -> %s"),
					*Indent, *Pin->PinName.ToString(),
					Pin->Direction == EGPD_Input ? TEXT("in") : TEXT("out"), *Target);
			}

			if (const UAnimGraphNode_Slot* SlotNode = Cast<UAnimGraphNode_Slot>(Node))
			{
				UE_LOG(LogMALUpperBodySetup, Display, TEXT("%s    slot name: %s"), *Indent, *SlotNode->Node.SlotName.ToString());
			}

			if (const UAnimGraphNode_LayeredBoneBlend* BlendNode = Cast<UAnimGraphNode_LayeredBoneBlend>(Node))
			{
				for (int32 LayerIndex = 0; LayerIndex < BlendNode->Node.LayerSetup.Num(); ++LayerIndex)
				{
					const FInputBlendPose& Layer = BlendNode->Node.LayerSetup[LayerIndex];
					for (const FBranchFilter& Filter : Layer.BranchFilters)
					{
						UE_LOG(LogMALUpperBodySetup, Display, TEXT("%s    layer %d branch: %s depth %d"),
							*Indent, LayerIndex, *Filter.BoneName.ToString(), Filter.BlendDepth);
					}
				}
			}

			if (const UAnimGraphNode_BlendSpacePlayer* BlendSpacePlayer = Cast<UAnimGraphNode_BlendSpacePlayer>(Node))
			{
				// The axes are pins, so a blend space player that looks configured can still be
				// sampling a constant (0, 0) if these two links are missing.
				const UEdGraphPin* XPin = BlendSpacePlayer->FindPin(TEXT("X"), EGPD_Input);
				const UEdGraphPin* YPin = BlendSpacePlayer->FindPin(TEXT("Y"), EGPD_Input);

				UE_LOG(LogMALUpperBodySetup, Display, TEXT("%s    blend space: %s, X links %d, Y links %d"),
					*Indent,
					BlendSpacePlayer->Node.GetBlendSpace() ? *BlendSpacePlayer->Node.GetBlendSpace()->GetName() : TEXT("none"),
					XPin ? XPin->LinkedTo.Num() : -1, YPin ? YPin->LinkedTo.Num() : -1);
			}

			if (const UAnimGraphNode_SequencePlayer* SequencePlayer = Cast<UAnimGraphNode_SequencePlayer>(Node))
			{
				const UAnimSequenceBase* Sequence = SequencePlayer->Node.GetSequence();
				UE_LOG(LogMALUpperBodySetup, Display, TEXT("%s    sequence: %s"),
					*Indent, Sequence ? *Sequence->GetName() : TEXT("none"));
			}

			if (const UAnimGraphNode_BlendListByBool* SwitchNode = Cast<UAnimGraphNode_BlendListByBool>(Node))
			{
				const UEdGraphPin* FalsePin = FindPosePinNamed(const_cast<UAnimGraphNode_BlendListByBool*>(SwitchNode), TEXT("BlendPoses_0"), EGPD_Input, 0);
				const UEdGraphPin* TruePin = FindPosePinNamed(const_cast<UAnimGraphNode_BlendListByBool*>(SwitchNode), TEXT("BlendPoses_1"), EGPD_Input, 1);
				const UEdGraphPin* FlagPin = FindBoolPin(const_cast<UAnimGraphNode_BlendListByBool*>(SwitchNode));

				UE_LOG(LogMALUpperBodySetup, Display, TEXT("%s    aim switch: false links %d, true links %d, flag links %d"),
					*Indent, FalsePin ? FalsePin->LinkedTo.Num() : -1,
					TruePin ? TruePin->LinkedTo.Num() : -1, FlagPin ? FlagPin->LinkedTo.Num() : -1);
			}
		}
	}

	void DumpHost(const UAnimBlueprint* HostBlueprint)
	{
		UE_LOG(LogMALUpperBodySetup, Display, TEXT("=== %s ==="), *HostBlueprint->GetName());
		DumpGraph(FindAnimGraph(HostBlueprint), TEXT("  "));
	}

	void DumpShotMontage(UAnimMontage* Montage)
	{
		if (!Montage)
		{
			UE_LOG(LogMALUpperBodySetup, Warning, TEXT("Shot montage not found at %s."), BowMontagePath);
			return;
		}

		UE_LOG(LogMALUpperBodySetup, Display, TEXT("=== %s ==="), *Montage->GetName());

		for (const FSlotAnimationTrack& Track : Montage->SlotAnimTracks)
		{
			UE_LOG(LogMALUpperBodySetup, Display, TEXT("  slot track: %s"), *Track.SlotName.ToString());
		}

		// CompositeSections is protected and there is no accessor for the names, so the dump
		// reads it through reflection the same way the bow setup commandlet reads private state.
		if (const FArrayProperty* SectionsProperty = FindFProperty<FArrayProperty>(UAnimMontage::StaticClass(), TEXT("CompositeSections")))
		{
			if (const FStructProperty* SectionProperty = CastField<FStructProperty>(SectionsProperty->Inner))
			{
				FScriptArrayHelper Helper(SectionsProperty, SectionsProperty->ContainerPtrToValuePtr<void>(Montage));
				for (int32 Index = 0; Index < Helper.Num(); ++Index)
				{
					if (const FNameProperty* NameProperty = FindFProperty<FNameProperty>(SectionProperty->Struct, TEXT("SectionName")))
					{
						const FName SectionName = NameProperty->GetPropertyValue(NameProperty->ContainerPtrToValuePtr<void>(Helper.GetRawPtr(Index)));
						UE_LOG(LogMALUpperBodySetup, Display, TEXT("  section: %s"), *SectionName.ToString());
					}
				}
			}
		}
	}

	void DumpBowLayer(const UAnimBlueprint* BowBlueprint)
	{
		UE_LOG(LogMALUpperBodySetup, Display, TEXT("=== %s ==="), *BowBlueprint->GetName());
		UE_LOG(LogMALUpperBodySetup, Display, TEXT("  parent class: %s"),
			BowBlueprint->ParentClass ? *BowBlueprint->ParentClass->GetName() : TEXT("none"));

		for (const TCHAR* FunctionName : LayerFunctionNames)
		{
			UAnimationGraph* LayerGraph = FindAnimGraph(BowBlueprint, FName(FunctionName));
			UE_LOG(LogMALUpperBodySetup, Display, TEXT("  --- %s ---"), FunctionName);
			DumpGraph(LayerGraph, TEXT("    "));
		}
	}

	/**
	 * Slot names and branch filters the runtime actually uses, read off the compiled class.
	 *
	 * The graph is what was authored; the compiled class is what plays. Checking the baked copy is
	 * the only way to prove the split survived compilation instead of only looking right in the
	 * editor.
	 */
	void DumpBakedNodes(const UAnimBlueprint* Blueprint, TSet<FName>* OutSlotNames = nullptr)
	{
		UClass* AnimClass = Blueprint ? Blueprint->GeneratedClass.Get() : nullptr;
		IAnimClassInterface* AnimClassInterface = IAnimClassInterface::GetFromClass(AnimClass);
		if (!AnimClassInterface)
		{
			UE_LOG(LogMALUpperBodySetup, Warning, TEXT("%s has no compiled anim class to inspect."),
				Blueprint ? *Blueprint->GetName() : TEXT("none"));
			return;
		}

		UObject* ClassDefaults = AnimClass->GetDefaultObject();
		const TArray<FStructProperty*>& NodeProperties = AnimClassInterface->GetAnimNodeProperties();

		UE_LOG(LogMALUpperBodySetup, Display, TEXT("=== %s: %d baked node(s) ==="),
			*AnimClass->GetName(), NodeProperties.Num());

		for (const FStructProperty* NodeProperty : NodeProperties)
		{
			if (!NodeProperty || !ClassDefaults)
			{
				continue;
			}

			const void* NodeMemory = NodeProperty->ContainerPtrToValuePtr<void>(ClassDefaults);

			if (NodeProperty->Struct == FAnimNode_Slot::StaticStruct())
			{
				const FAnimNode_Slot* SlotNode = static_cast<const FAnimNode_Slot*>(NodeMemory);
				UE_LOG(LogMALUpperBodySetup, Display, TEXT("  baked slot: %s"), *SlotNode->SlotName.ToString());

				if (OutSlotNames)
				{
					OutSlotNames->Add(SlotNode->SlotName);
				}
			}
			else if (NodeProperty->Struct == FAnimNode_LayeredBoneBlend::StaticStruct())
			{
				const FAnimNode_LayeredBoneBlend* BlendNode = static_cast<const FAnimNode_LayeredBoneBlend*>(NodeMemory);
				UE_LOG(LogMALUpperBodySetup, Display, TEXT("  baked layered blend: %d layer(s)"), BlendNode->LayerSetup.Num());

				for (const FInputBlendPose& Layer : BlendNode->LayerSetup)
				{
					for (const FBranchFilter& Filter : Layer.BranchFilters)
					{
						UE_LOG(LogMALUpperBodySetup, Display, TEXT("    baked branch: %s depth %d"),
							*Filter.BoneName.ToString(), Filter.BlendDepth);
					}
				}
			}
		}
	}

	/**
	 * The three silent failures of this split: a branch bone the skeleton does not have, a montage
	 * whose slot no node answers, and a split that never made it into the compiled class.
	 */
	bool ValidateUpperBodySplit(const UAnimBlueprint* HostBlueprint, const USkeleton* Skeleton, const UAnimMontage* Montage)
	{
		bool bValid = true;

		if (!Skeleton || Skeleton->GetReferenceSkeleton().FindBoneIndex(FName(UpperBodyRootBone)) == INDEX_NONE)
		{
			UE_LOG(LogMALUpperBodySetup, Error, TEXT("The skeleton has no bone '%s': the branch filter would blend nothing."),
				UpperBodyRootBone);
			bValid = false;
		}

		TSet<FName> BakedSlotNames;
		DumpBakedNodes(HostBlueprint, &BakedSlotNames);

		if (!BakedSlotNames.Contains(FName(UpperBodySlotName)))
		{
			UE_LOG(LogMALUpperBodySetup, Error, TEXT("The compiled host graph has no %s slot: the shot would play nothing."),
				UpperBodySlotName);
			bValid = false;
		}

		if (Montage)
		{
			for (const FSlotAnimationTrack& Track : Montage->SlotAnimTracks)
			{
				if (!BakedSlotNames.Contains(Track.SlotName))
				{
					UE_LOG(LogMALUpperBodySetup, Error,
						TEXT("%s plays in slot '%s', which no graph node answers."),
						*Montage->GetName(), *Track.SlotName.ToString());
					bValid = false;
				}
			}
		}

		UE_CLOG(bValid, LogMALUpperBodySetup, Display,
			TEXT("Upper body split validated: branch %s, slot %s, montage slots answered."),
			UpperBodyRootBone, UpperBodySlotName);

		return bValid;
	}

	/**
	 * The bow layer reads Speed and Direction off itself, so each linked instance has to derive
	 * from UMALAnimInstance; a plain UAnimInstance parent leaves the blend space sampling (0, 0).
	 */
	bool EnsureBowLayerParent(UAnimBlueprint* BowBlueprint)
	{
		if (!BowBlueprint || BowBlueprint->ParentClass == UMALAnimInstance::StaticClass())
		{
			return false;
		}

		UE_LOG(LogMALUpperBodySetup, Display, TEXT("%s parent class was %s, reparenting to UMALAnimInstance."),
			*BowBlueprint->GetName(),
			BowBlueprint->ParentClass ? *BowBlueprint->ParentClass->GetName() : TEXT("none"));

		BowBlueprint->ParentClass = UMALAnimInstance::StaticClass();
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BowBlueprint);
		return true;
	}

	/**
	 * Every bow layer must branch on the stance and keep both branches wired, or the character either
	 * strafes while standing around or animates nothing while aiming.
	 */
	bool ValidateBowLayerAimingSwitch(const UAnimBlueprint* BowBlueprint, const UBlendSpace* BlendSpace)
	{
		if (!BowBlueprint)
		{
			return false;
		}

		bool bValid = true;

		for (const FPlantedAnimation& Layer : PlantedAnimations)
		{
			const UAnimationGraph* LayerGraph = FindAnimGraph(BowBlueprint, FName(Layer.FunctionName));

			if (!LayerGraph)
			{
				UE_LOG(LogMALUpperBodySetup, Error, TEXT("%s: no %s graph found for validation."),
					*BowBlueprint->GetName(), Layer.FunctionName);
				bValid = false;
				continue;
			}

			TArray<UAnimGraphNode_BlendListByBool*> Switches;
			LayerGraph->GetNodesOfClass<UAnimGraphNode_BlendListByBool>(Switches);

			UEdGraphPin* FalsePin = Switches.Num() > 0
				? FindPosePinNamed(Switches[0], TEXT("BlendPoses_0"), EGPD_Input, 0)
				: nullptr;
			UEdGraphPin* TruePin = Switches.Num() > 0
				? FindPosePinNamed(Switches[0], TEXT("BlendPoses_1"), EGPD_Input, 1)
				: nullptr;
			UEdGraphPin* FlagPin = Switches.Num() > 0 ? FindBoolPin(Switches[0]) : nullptr;

			if (!FalsePin || !TruePin || !FlagPin
				|| FalsePin->LinkedTo.Num() == 0 || TruePin->LinkedTo.Num() == 0 || FlagPin->LinkedTo.Num() == 0)
			{
				UE_LOG(LogMALUpperBodySetup, Error,
					TEXT("%s: the aim switch is not wired on both branches and the stance flag."), Layer.FunctionName);
				bValid = false;
				continue;
			}

			// The aim side has to be the blend space; the planted side has to be a sequence, or the
			// stance would do nothing at all.
			const UAnimGraphNode_BlendSpacePlayer* AimPlayer = Cast<UAnimGraphNode_BlendSpacePlayer>(GetLinkedPoseSource(TruePin));
			const UAnimGraphNode_SequencePlayer* PlantedPlayer = Cast<UAnimGraphNode_SequencePlayer>(GetLinkedPoseSource(FalsePin));

			if (!AimPlayer || AimPlayer->Node.GetBlendSpace() != BlendSpace)
			{
				UE_LOG(LogMALUpperBodySetup, Error, TEXT("%s: the true branch is not %s."),
					Layer.FunctionName, BlendSpace ? *BlendSpace->GetName() : TEXT("the aim blend space"));
				bValid = false;
			}

			if (!PlantedPlayer || !PlantedPlayer->Node.GetSequence())
			{
				UE_LOG(LogMALUpperBodySetup, Error, TEXT("%s: the false branch has no forward animation."),
					Layer.FunctionName);
				bValid = false;
			}
		}

		UE_CLOG(bValid, LogMALUpperBodySetup, Display,
			TEXT("Bow layer validated: every layer switches to the aim set only while aiming."));

		return bValid;
	}

	FBlendParameter* GetBlendParameter(UBlendSpace* BlendSpace, int32 AxisIndex)
	{
		if (!BlendSpace)
		{
			return nullptr;
		}

		// The axes live in a protected fixed-size array with no setter, so they are written
		// through reflection rather than grown into the engine class.
		FStructProperty* Property = FindFProperty<FStructProperty>(UBlendSpace::StaticClass(), TEXT("BlendParameters"));
		if (!Property || AxisIndex < 0 || AxisIndex >= Property->ArrayDim)
		{
			UE_LOG(LogMALUpperBodySetup, Error, TEXT("Could not reach UBlendSpace::BlendParameters."));
			return nullptr;
		}

		return Property->ContainerPtrToValuePtr<FBlendParameter>(BlendSpace, AxisIndex);
	}

	void DumpBlendSpace(const UBlendSpace* BlendSpace)
	{
		if (!BlendSpace)
		{
			UE_LOG(LogMALUpperBodySetup, Display, TEXT("BS_Bow_Move: not created yet"));
			return;
		}

		UE_LOG(LogMALUpperBodySetup, Display, TEXT("=== %s ==="), *BlendSpace->GetName());

		for (int32 AxisIndex = 0; AxisIndex < 2; ++AxisIndex)
		{
			const FBlendParameter& Parameter = BlendSpace->GetBlendParameter(AxisIndex);
			UE_LOG(LogMALUpperBodySetup, Display, TEXT("  axis %d: %s [%.0f .. %.0f] grid %d wrap %s"),
				AxisIndex, *Parameter.DisplayName, Parameter.Min, Parameter.Max, Parameter.GridNum,
				Parameter.bWrapInput ? TEXT("on") : TEXT("off"));
		}

		for (const FBlendSample& Sample : BlendSpace->GetBlendSamples())
		{
			UE_LOG(LogMALUpperBodySetup, Display, TEXT("  sample (%.0f, %.0f): %s"),
				Sample.SampleValue.X, Sample.SampleValue.Y,
				Sample.Animation ? *Sample.Animation->GetName() : TEXT("none"));
		}
	}

	// --- Apply ---------------------------------------------------------------------------

	/**
	 * Rebuilds the host tail as
	 *
	 *   locomotion -> DefaultSlot -> [LayeredBlendPerBone(spine_01)] -> Inertialization -> Root
	 *                             \-> UpperBodySlot -/
	 *
	 * DefaultSlot stays in the base pose so melee montages still animate the whole body. The
	 * upper slot is a second branch that only overrides the spine and its children, which is what
	 * lets the shot montage move while the legs keep running.
	 */
	bool InsertUpperBodyBlendStack(UAnimBlueprint* HostBlueprint)
	{
		UAnimationGraph* AnimGraph = FindAnimGraph(HostBlueprint);
		if (!AnimGraph)
		{
			UE_LOG(LogMALUpperBodySetup, Error, TEXT("No AnimGraph on %s."), *HostBlueprint->GetName());
			return false;
		}

		TArray<UAnimGraphNode_Root*> RootNodes;
		TArray<UAnimGraphNode_Slot*> SlotNodes;
		TArray<UAnimGraphNode_LayeredBoneBlend*> BlendNodes;
		TArray<UAnimGraphNode_Inertialization*> InertializationNodes;
		AnimGraph->GetNodesOfClass<UAnimGraphNode_Root>(RootNodes);
		AnimGraph->GetNodesOfClass<UAnimGraphNode_Slot>(SlotNodes);
		AnimGraph->GetNodesOfClass<UAnimGraphNode_LayeredBoneBlend>(BlendNodes);
		AnimGraph->GetNodesOfClass<UAnimGraphNode_Inertialization>(InertializationNodes);

		UAnimGraphNode_Root* RootNode = RootNodes.Num() > 0 ? RootNodes[0] : nullptr;
		UEdGraphPin* RootInput = RootNode ? FindPosePin(RootNode, EGPD_Input) : nullptr;
		if (!RootInput)
		{
			UE_LOG(LogMALUpperBodySetup, Error, TEXT("The host graph has no output pose node."));
			return false;
		}

		auto FindSlotByName = [](TArray<UAnimGraphNode_Slot*>& Candidates, const FString& SlotName) -> UAnimGraphNode_Slot*
		{
			for (UAnimGraphNode_Slot* Candidate : Candidates)
			{
				if (Candidate && Candidate->Node.SlotName == FName(*SlotName))
				{
					return Candidate;
				}
			}

			return nullptr;
		};

		UAnimGraphNode_Slot* UpperBodySlot = FindSlotByName(SlotNodes, UpperBodySlotName);
		UAnimGraphNode_LayeredBoneBlend* BlendNode = BlendNodes.Num() > 0 ? BlendNodes[0] : nullptr;

		UAnimGraphNode_Slot* BaseSlot = FindSlotByName(SlotNodes, FullBodySlotName);
		if (!BaseSlot && SlotNodes.Num() > 0)
		{
			BaseSlot = SlotNodes[0];
		}

		UEdGraphPin* BaseSlotOutput = BaseSlot ? FindPosePin(BaseSlot, EGPD_Output) : nullptr;
		if (!BaseSlotOutput)
		{
			UE_LOG(LogMALUpperBodySetup, Error, TEXT("The host graph has no body slot to split from."));
			return false;
		}

		const int32 BaseX = BaseSlot->NodePosX;
		const int32 BaseY = BaseSlot->NodePosY;

		if (!UpperBodySlot)
		{
			UpperBodySlot = CreateGraphNode<UAnimGraphNode_Slot>(AnimGraph, BaseX, BaseY + 300);
			UpperBodySlot->Node.SlotName = FName(UpperBodySlotName);

			// The source keeps being evaluated while the montage plays, so the pose the slot
			// blends back into when the shot ends is current instead of the frame it started on.
			UpperBodySlot->Node.bAlwaysUpdateSourcePose = true;

			UE_LOG(LogMALUpperBodySetup, Display, TEXT("Created the %s slot."), UpperBodySlotName);
		}

		if (!BlendNode)
		{
			BlendNode = CreateGraphNode<UAnimGraphNode_LayeredBoneBlend>(AnimGraph, BaseX + 300, BaseY);
			BlendNode->Node.BlendMode = ELayeredBoneBlendMode::BranchFilter;

			// A fresh node carries exactly one blend pose, so the layer setup has to exist for it
			// rather than be added: adding a pose without its pin desyncs the node.
			if (BlendNode->Node.LayerSetup.Num() == 0)
			{
				BlendNode->Node.LayerSetup.AddDefaulted();
			}
			if (BlendNode->Node.BlendWeights.Num() == 0)
			{
				BlendNode->Node.BlendWeights.Add(1.f);
			}

			UE_LOG(LogMALUpperBodySetup, Display, TEXT("Created the layered blend node."));
		}

		FBranchFilter Filter;
		Filter.BoneName = FName(UpperBodyRootBone);
		Filter.BlendDepth = 0;
		BlendNode->Node.BlendMode = ELayeredBoneBlendMode::BranchFilter;
		BlendNode->Node.LayerSetup[0].BranchFilters.Reset();
		BlendNode->Node.LayerSetup[0].BranchFilters.Add(Filter);
		BlendNode->Node.BlendWeights[0] = 1.f;

		UEdGraphPin* UpperSlotInput = FindPosePin(UpperBodySlot, EGPD_Input);
		UEdGraphPin* UpperSlotOutput = FindPosePin(UpperBodySlot, EGPD_Output);
		UEdGraphPin* BasePoseInput = FindPosePinNamed(BlendNode, TEXT("BasePose"), EGPD_Input, 0);
		UEdGraphPin* BlendPoseInput = FindPosePinNamed(BlendNode, TEXT("BlendPoses_0"), EGPD_Input, 1);
		UEdGraphPin* BlendOutput = FindPosePin(BlendNode, EGPD_Output);

		// Whatever the body slot used to feed now feeds the blend instead, so the tail input is
		// re-pointed rather than left with two owners of the same pin.
		UEdGraphPin* TailInput = InertializationNodes.Num() > 0
			? FindPosePin(InertializationNodes[0], EGPD_Input)
			: RootInput;

		if (!UpperSlotInput || !UpperSlotOutput || !BasePoseInput || !BlendPoseInput || !BlendOutput || !TailInput)
		{
			UE_LOG(LogMALUpperBodySetup, Error, TEXT("The upper body split is missing one of its pose pins."));
			return false;
		}

		// Every link of the split is rebuilt on each pass instead of only on the pass that
		// created the nodes: a connection made against a freshly created node does not survive
		// the pin rebuild that can follow, and the silent result is a slot with no source pose.
		for (UEdGraphPin* Target : { UpperSlotInput, BasePoseInput, BlendPoseInput, TailInput })
		{
			if (Target && Target->LinkedTo.Num() > 0)
			{
				Target->BreakAllPinLinks();
			}
		}

		bool bWired = true;

		// Order matters: the schema connection has to happen while the body slot output still has a
		// single consumer, otherwise the fork below is not needed but the base pose loses its
		// link to the tree rule.
		bWired &= ConnectPins(BaseSlotOutput, BasePoseInput);
		bWired &= ForceConnectPins(BaseSlotOutput, UpperSlotInput);
		bWired &= ConnectPins(UpperSlotOutput, BlendPoseInput);
		bWired &= ConnectPins(BlendOutput, TailInput);

		// The link counts are logged because a silently missing link here is the difference
		// between a working split and a reference pose.
		UE_LOG(LogMALUpperBodySetup, Display, TEXT("Tail wiring: %s source %d, base %d, blend %d, tail %d%s"),
			UpperBodySlotName, UpperSlotInput->LinkedTo.Num(), BasePoseInput->LinkedTo.Num(),
			BlendPoseInput->LinkedTo.Num(), TailInput->LinkedTo.Num(),
			bWired ? TEXT("") : TEXT(" (incomplete)"));

		// Keep the nodes readable in the editor: the tail moves right of the blend.
		if (InertializationNodes.Num() > 0)
		{
			InertializationNodes[0]->NodePosX = BlendNode->NodePosX + 300;
			InertializationNodes[0]->NodePosY = BaseY;
		}

		UE_CLOG(!bWired, LogMALUpperBodySetup, Error,
			TEXT("The upper body split is not fully wired; check the graph by hand."));

		return bWired;
	}

	/** Every slot track of the shot montage moves to the upper body slot. */
	bool MoveShotMontageToUpperBodySlot()
	{
		UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, BowMontagePath);
		if (!Montage)
		{
			UE_LOG(LogMALUpperBodySetup, Error, TEXT("%s not found."), BowMontagePath);
			return false;
		}

		bool bChanged = false;
		for (FSlotAnimationTrack& Track : Montage->SlotAnimTracks)
		{
			if (Track.SlotName == FName(UpperBodySlotName))
			{
				continue;
			}

			Track.SlotName = FName(UpperBodySlotName);
			bChanged = true;
		}

		if (!bChanged)
		{
			UE_LOG(LogMALUpperBodySetup, Display, TEXT("%s already plays in %s."), *Montage->GetName(), UpperBodySlotName);
			return false;
		}

		Montage->MarkPackageDirty();
		SaveAssetPackage(Montage);

		UE_LOG(LogMALUpperBodySetup, Display, TEXT("%s slot tracks now use %s."), *Montage->GetName(), UpperBodySlotName);
		return true;
	}

	/** Speed on X, direction on Y, built from the in-place archer set the bow layer already used. */
	UBlendSpace* EnsureBowMoveBlendSpace(USkeleton* Skeleton)
	{
		const FString FullPath = FString::Printf(TEXT("%s/%s.%s"), LayerFolderPath, BlendSpaceAsset, BlendSpaceAsset);
		UBlendSpace* BlendSpace = LoadObject<UBlendSpace>(nullptr, *FullPath);

		if (!BlendSpace)
		{
			UBlendSpaceFactoryNew* Factory = NewObject<UBlendSpaceFactoryNew>();
			Factory->TargetSkeleton = Skeleton;

			IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
			BlendSpace = Cast<UBlendSpace>(AssetTools.CreateAsset(
				FString(BlendSpaceAsset), LayerFolderPath, UBlendSpace::StaticClass(), Factory));

			if (!BlendSpace)
			{
				UE_LOG(LogMALUpperBodySetup, Error, TEXT("Failed to create %s."), *FullPath);
				return nullptr;
			}

			UE_LOG(LogMALUpperBodySetup, Display, TEXT("Created %s."), *FullPath);
		}

		if (FBlendParameter* SpeedAxis = GetBlendParameter(BlendSpace, 0))
		{
			SpeedAxis->DisplayName = TEXT("Speed");
			SpeedAxis->Min = 0.f;
			SpeedAxis->Max = SpeedAxisMax;
			SpeedAxis->GridNum = 7;
			SpeedAxis->bSnapToGrid = false;
			SpeedAxis->bWrapInput = false;
		}

		if (FBlendParameter* DirectionAxis = GetBlendParameter(BlendSpace, 1))
		{
			DirectionAxis->DisplayName = TEXT("Direction");
			DirectionAxis->Min = -180.f;
			DirectionAxis->Max = 180.f;
			DirectionAxis->GridNum = 8;

			// Walking beyond -180 reads as 179 again, so the axis has to wrap instead of clamp.
			DirectionAxis->bWrapInput = true;
		}

		if (BlendSpace->GetNumberOfBlendSamples() == 0)
		{
			int32 AddedCount = 0;
			for (const FMoveSample& Sample : MoveSamples)
			{
				UAnimSequence* Sequence = LoadObject<UAnimSequence>(nullptr, Sample.SequencePath);
				if (!Sequence)
				{
					UE_LOG(LogMALUpperBodySetup, Warning, TEXT("Sample animation %s not found, skipped."), Sample.SequencePath);
					continue;
				}

				// AddSample keeps the sample list, the grid and the triangulation in sync; writing
				// SampleData by hand would leave the runtime data stale.
				BlendSpace->AddSample(Sequence, FVector(Sample.Speed, Sample.Direction, 0.f));
				++AddedCount;
			}

			BlendSpace->ResampleData();
			BlendSpace->ValidateSampleData();
			BlendSpace->MarkPackageDirty();
			SaveAssetPackage(BlendSpace);

			UE_LOG(LogMALUpperBodySetup, Display, TEXT("%s now has %d samples."), *BlendSpace->GetName(), AddedCount);
		}
		else
		{
			UE_LOG(LogMALUpperBodySetup, Display, TEXT("%s already carries %d samples, axes refreshed only."),
				*BlendSpace->GetName(), BlendSpace->GetNumberOfBlendSamples());
			BlendSpace->MarkPackageDirty();
			SaveAssetPackage(BlendSpace);
		}

		return BlendSpace;
	}

	UK2Node_VariableGet* FindVariableGetter(UEdGraph* Graph, FName VariableName)
	{
		if (!Graph)
		{
			return nullptr;
		}

		for (UEdGraphNode* GraphNode : Graph->Nodes)
		{
			UK2Node_VariableGet* Getter = Cast<UK2Node_VariableGet>(GraphNode);
			if (Getter && Getter->VariableReference.GetMemberName() == VariableName)
			{
				return Getter;
			}
		}

		return nullptr;
	}

	/**
	 * Puts the directional aim set behind the aiming stance.
	 *
	 * Aiming strafes, so the layer plays the speed/direction blend space while the stance is on.
	 * The rest of the time - bow in hand, not shooting - it plays the plain forward sequence the
	 * layer used before, because carrying a bow should not change how the character walks or runs.
	 *
	 * The branch is a Blend Poses by Bool driven by UMALAnimInstance::bIsAiming, which every linked
	 * layer instance computes for itself.
	 */
	bool WireBowLayerAimingSwitch(UAnimBlueprint* BowBlueprint, UBlendSpace* BlendSpace)
	{
		if (!BowBlueprint || !BlendSpace)
		{
			return false;
		}

		bool bChanged = false;

		for (const FPlantedAnimation& Layer : PlantedAnimations)
		{
			UAnimationGraph* LayerGraph = FindAnimGraph(BowBlueprint, FName(Layer.FunctionName));
			if (!LayerGraph)
			{
				UE_LOG(LogMALUpperBodySetup, Warning, TEXT("%s has no %s graph."), *BowBlueprint->GetName(), Layer.FunctionName);
				continue;
			}

			TArray<UAnimGraphNode_Root*> RootNodes;
			LayerGraph->GetNodesOfClass<UAnimGraphNode_Root>(RootNodes);
			UEdGraphPin* RootInput = RootNodes.Num() > 0 ? FindPosePin(RootNodes[0], EGPD_Input) : nullptr;
			if (!RootInput)
			{
				UE_LOG(LogMALUpperBodySetup, Warning, TEXT("%s has no output pose node."), Layer.FunctionName);
				continue;
			}

			const int32 RootX = RootNodes[0]->NodePosX;
			const int32 RootY = RootNodes[0]->NodePosY;

			// Aiming branch: the speed/direction blend space.
			TArray<UAnimGraphNode_BlendSpacePlayer*> BlendSpacePlayers;
			LayerGraph->GetNodesOfClass<UAnimGraphNode_BlendSpacePlayer>(BlendSpacePlayers);
			UAnimGraphNode_BlendSpacePlayer* AimPlayer = BlendSpacePlayers.Num() > 0
				? BlendSpacePlayers[0]
				: CreateGraphNode<UAnimGraphNode_BlendSpacePlayer>(LayerGraph, RootX - 900, RootY - 200);

			AimPlayer->Node.SetBlendSpace(BlendSpace);
			AimPlayer->Node.SetPlayRate(1.f);
			AimPlayer->Node.SetLoop(true);

			// Planted branch: the forward sequence this layer used before the aim set existed.
			TArray<UAnimGraphNode_SequencePlayer*> SequencePlayers;
			LayerGraph->GetNodesOfClass<UAnimGraphNode_SequencePlayer>(SequencePlayers);
			UAnimGraphNode_SequencePlayer* PlantedPlayer = SequencePlayers.Num() > 0
				? SequencePlayers[0]
				: CreateGraphNode<UAnimGraphNode_SequencePlayer>(LayerGraph, RootX - 900, RootY + 200);

			UAnimSequence* PlantedSequence = LoadObject<UAnimSequence>(nullptr, Layer.SequencePath);
			if (!PlantedSequence)
			{
				UE_LOG(LogMALUpperBodySetup, Error, TEXT("%s: planted animation %s not found."),
					Layer.FunctionName, Layer.SequencePath);
				continue;
			}

			PlantedPlayer->Node.SetSequence(PlantedSequence);
			PlantedPlayer->Node.SetLoopAnimation(true);

			// The switch itself.
			TArray<UAnimGraphNode_BlendListByBool*> Switches;
			LayerGraph->GetNodesOfClass<UAnimGraphNode_BlendListByBool>(Switches);
			UAnimGraphNode_BlendListByBool* SwitchNode = Switches.Num() > 0
				? Switches[0]
				: CreateGraphNode<UAnimGraphNode_BlendListByBool>(LayerGraph, RootX - 400, RootY);

			// Index 0 is the false pose and index 1 the true one, so the aim set sits on the true
			// branch and an unset flag can never leave the character strafing at rest.
			UEdGraphPin* FalseInput = FindPosePinNamed(SwitchNode, TEXT("BlendPoses_0"), EGPD_Input, 0);
			UEdGraphPin* TrueInput = FindPosePinNamed(SwitchNode, TEXT("BlendPoses_1"), EGPD_Input, 1);
			UEdGraphPin* SwitchOutput = FindPosePin(SwitchNode, EGPD_Output);
			UEdGraphPin* AimingInput = FindBoolPin(SwitchNode);

			UK2Node_VariableGet* AimingGetter = FindVariableGetter(LayerGraph, AimingVariable);
			if (!AimingGetter)
			{
				AimingGetter = CreateVariableGetter(LayerGraph, AimingVariable, RootX - 1250, RootY - 420);
			}

			UEdGraphPin* AimingOutput = AimingGetter ? AimingGetter->FindPin(AimingVariable, EGPD_Output) : nullptr;

			if (!FalseInput || !TrueInput || !SwitchOutput || !AimingInput || !AimingOutput)
			{
				UE_LOG(LogMALUpperBodySetup, Error,
					TEXT("%s: the aiming switch is missing a pin (false %d, true %d, out %d, flag pin %d, flag getter %d)."),
					Layer.FunctionName, FalseInput ? 1 : 0, TrueInput ? 1 : 0, SwitchOutput ? 1 : 0,
					AimingInput ? 1 : 0, AimingOutput ? 1 : 0);
				continue;
			}

			RootInput->BreakAllPinLinks();
			FalseInput->BreakAllPinLinks();
			TrueInput->BreakAllPinLinks();
			AimingInput->BreakAllPinLinks();

			bool bWired = ConnectPins(FindPosePin(PlantedPlayer, EGPD_Output), FalseInput);
			bWired &= ConnectPins(FindPosePin(AimPlayer, EGPD_Output), TrueInput);
			bWired &= ConnectPins(SwitchOutput, RootInput);
			bWired &= ConnectPins(AimingOutput, AimingInput);

			// The blend space axes only matter while aiming, but they are bound on every pass so the
			// aiming branch is never left sampling (0, 0).
			UK2Node_VariableGet* SpeedGetter = FindVariableGetter(LayerGraph, SpeedVariable);
			if (!SpeedGetter)
			{
				SpeedGetter = CreateVariableGetter(LayerGraph, SpeedVariable, RootX - 1250, RootY - 200);
			}

			UK2Node_VariableGet* DirectionGetter = FindVariableGetter(LayerGraph, DirectionVariable);
			if (!DirectionGetter)
			{
				DirectionGetter = CreateVariableGetter(LayerGraph, DirectionVariable, RootX - 1250, RootY + 20);
			}

			UEdGraphPin* SpeedOutput = SpeedGetter ? SpeedGetter->FindPin(SpeedVariable, EGPD_Output) : nullptr;
			UEdGraphPin* DirectionOutput = DirectionGetter ? DirectionGetter->FindPin(DirectionVariable, EGPD_Output) : nullptr;
			UEdGraphPin* XInput = AimPlayer->FindPin(TEXT("X"), EGPD_Input);
			UEdGraphPin* YInput = AimPlayer->FindPin(TEXT("Y"), EGPD_Input);

			bWired &= ConnectPins(SpeedOutput, XInput);
			bWired &= ConnectPins(DirectionOutput, YInput);

			if (!bWired)
			{
				UE_LOG(LogMALUpperBodySetup, Error, TEXT("%s: could not wire the aiming switch."), Layer.FunctionName);
				continue;
			}

			bChanged = true;
			UE_LOG(LogMALUpperBodySetup, Display,
				TEXT("%s: aims with %s on (Speed, Direction), otherwise plays %s."),
				Layer.FunctionName, *BlendSpace->GetName(), *PlantedSequence->GetName());
		}

		return bChanged;
	}
}

using namespace MALUpperBodySetup;

UMALUpperBodyLayerSetupCommandlet::UMALUpperBodyLayerSetupCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UMALUpperBodyLayerSetupCommandlet::Main(const FString& Params)
{
	FString Mode = TEXT("apply");
	FParse::Value(*Params, TEXT("Mode="), Mode);

	const bool bDumpOnly = Mode.Equals(TEXT("dump"), ESearchCase::IgnoreCase);
	const bool bCompileOnly = Mode.Equals(TEXT("compile"), ESearchCase::IgnoreCase);

	// Bisect switches: the graph is written to disk before it is compiled, so a compile error can
	// be narrowed down without losing the edit.
	const bool bSkipCompile = FParse::Param(*Params, TEXT("SkipCompile"));
	const bool bSkipHostBlend = FParse::Param(*Params, TEXT("SkipHostBlend"));
	const bool bSkipMontage = FParse::Param(*Params, TEXT("SkipMontage"));
	const bool bSkipBowLayer = FParse::Param(*Params, TEXT("SkipBowLayer"));

	UAnimBlueprint* HostBlueprint = LoadObject<UAnimBlueprint>(nullptr, HostBlueprintPath);
	if (!HostBlueprint)
	{
		UE_LOG(LogMALUpperBodySetup, Error, TEXT("%s not found."), HostBlueprintPath);
		return 1;
	}

	UAnimBlueprint* BowBlueprint = LoadObject<UAnimBlueprint>(nullptr, BowLayerPath);
	if (!BowBlueprint)
	{
		UE_LOG(LogMALUpperBodySetup, Error, TEXT("%s not found."), BowLayerPath);
		return 1;
	}

	USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, SkeletonPath);
	if (!Skeleton)
	{
		UE_LOG(LogMALUpperBodySetup, Error, TEXT("%s is missing."), SkeletonPath);
		return 1;
	}

	UBlendSpace* BlendSpace = LoadObject<UBlendSpace>(nullptr,
		*FString::Printf(TEXT("%s/%s.%s"), LayerFolderPath, BlendSpaceAsset, BlendSpaceAsset));
	UAnimMontage* ShotMontage = LoadObject<UAnimMontage>(nullptr, BowMontagePath);

	DumpHost(HostBlueprint);
	DumpShotMontage(ShotMontage);
	DumpBowLayer(BowBlueprint);
	DumpBlendSpace(BlendSpace);

	if (bDumpOnly)
	{
		// The compiled class, not the graph, is what plays.
		ValidateUpperBodySplit(HostBlueprint, Skeleton, ShotMontage);
		ValidateBowLayerAimingSwitch(BowBlueprint, BlendSpace);
		return 0;
	}

	if (bCompileOnly)
	{
		CompileBlueprint(HostBlueprint);
		SaveAssetPackage(HostBlueprint);

		CompileBlueprint(BowBlueprint);
		SaveAssetPackage(BowBlueprint);

		return ValidateUpperBodySplit(HostBlueprint, Skeleton, ShotMontage)
			&& ValidateBowLayerAimingSwitch(BowBlueprint, BlendSpace) ? 0 : 1;
	}

	if (!bSkipHostBlend)
	{
		InsertUpperBodyBlendStack(HostBlueprint);
		SaveAssetPackage(HostBlueprint);
	}

	if (!bSkipMontage)
	{
		MoveShotMontageToUpperBodySlot();
	}

	UBlendSpace* ResolvedBlendSpace = BlendSpace;
	bool bBowLayerChanged = false;

	if (!bSkipBowLayer)
	{
		// Reparenting has to happen before the layer graph is rewritten: the variable getters the
		// blend space needs only resolve once the class derives from UMALAnimInstance.
		bBowLayerChanged |= EnsureBowLayerParent(BowBlueprint);

		ResolvedBlendSpace = EnsureBowMoveBlendSpace(Skeleton);
		bBowLayerChanged |= WireBowLayerAimingSwitch(BowBlueprint, ResolvedBlendSpace);

		if (bBowLayerChanged)
		{
			SaveAssetPackage(BowBlueprint);
		}
	}

	if (!bSkipCompile)
	{
		CompileBlueprint(HostBlueprint);
		SaveAssetPackage(HostBlueprint);

		if (bBowLayerChanged)
		{
			CompileBlueprint(BowBlueprint);
			SaveAssetPackage(BowBlueprint);
		}
	}

	const bool bValid = ValidateUpperBodySplit(HostBlueprint, Skeleton, ShotMontage)
		&& ValidateBowLayerAimingSwitch(BowBlueprint, ResolvedBlendSpace);

	UE_LOG(LogMALUpperBodySetup, Display,
		TEXT("Done: host split on %s, shot montage in %s, bow layer aims with %s only while aiming%s."),
		UpperBodyRootBone, UpperBodySlotName,
		ResolvedBlendSpace ? *ResolvedBlendSpace->GetName() : TEXT("no blend space"),
		bValid ? TEXT("") : TEXT(" (validation FAILED)"));

	return bValid ? 0 : 1;
}

#else

UMALUpperBodyLayerSetupCommandlet::UMALUpperBodyLayerSetupCommandlet()
{
	IsEditor = false;
}

int32 UMALUpperBodyLayerSetupCommandlet::Main(const FString& Params)
{
	return 0;
}

#endif // WITH_EDITOR
