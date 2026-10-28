// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ViewModel/MALSkillMenuViewModel.h"

#include "AbilitySystem/Attributes/MALProgressionAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Algo/StableSort.h"
#include "Components/MALProgressionComponent.h"
#include "Data/MALSkillTreeDefinition.h"
#include "Player/MALPlayerState.h"
#include "UI/ViewModel/MALSkillNodeViewModel.h"

void UMALSkillMenuViewModel::InitializeFromPlayerState(AMALPlayerState* InPlayerState)
{
	CachedPlayerState = InPlayerState;

	// Before the ability system: the ready callback below refreshes every node, which needs the
	// rows to exist already.
	BuildNodes();

	SetAbilitySystemComponent(InPlayerState ? InPlayerState->GetAbilitySystemComponent() : nullptr);

	// Also covers the case where the ability system was already the one being handed in and
	// SetAbilitySystemComponent therefore returned without firing its callbacks.
	RefreshAll();

	// Open on something rather than on an empty detail column: the first node the player can
	// actually buy, or the first row when nothing is affordable yet.
	if (!SelectedNode && Nodes.Num() > 0)
	{
		TObjectPtr<UMALSkillNodeViewModel> Opening = Nodes[0];
		for (UMALSkillNodeViewModel* Node : Nodes)
		{
			if (Node && Node->CanBeUnlocked())
			{
				Opening = Node;
				break;
			}
		}

		SelectNode(Opening);
	}
}

void UMALSkillMenuViewModel::RefreshAll()
{
	const AMALPlayerState* PlayerState = CachedPlayerState.Get();
	const UMALProgressionComponent* Progression = PlayerState ? PlayerState->GetProgressionComponent() : nullptr;

	// Zero rather than the last known value when there is no progression yet: a panel opened
	// before the PlayerState arrives should read as "nothing to spend", not as stale numbers.
	UE_MVVM_SET_PROPERTY_VALUE(SkillPoints, Progression ? Progression->GetSkillPoints() : 0);

	for (UMALSkillNodeViewModel* Node : Nodes)
	{
		if (!Node)
		{
			continue;
		}

		if (!Progression)
		{
			// No authority-side answer to give; the node keeps its authored data and reads as
			// unavailable until the player state shows up.
			Node->ApplyState(EMALSkillNodeState::Locked, FText::GetEmpty());
			continue;
		}

		FText Reason;
		const EMALSkillNodeState State = Progression->EvaluateSkillState(Node->GetSkillTag(), Reason);
		Node->ApplyState(State, Reason);
	}

	RefreshSelection();

	OnMenuRefreshed.Broadcast();
}

void UMALSkillMenuViewModel::SelectNode(UMALSkillNodeViewModel* Node)
{
	if (SelectedNode == Node)
	{
		return;
	}

	if (SelectedNode)
	{
		SelectedNode->SetSelected(false);
	}

	UE_MVVM_SET_PROPERTY_VALUE(SelectedNode, Node);

	if (Node)
	{
		Node->SetSelected(true);
	}

	RefreshSelection();

	// The detail column follows the selection, so the widget has to hear about it separately
	// from a RefreshAll.
	OnMenuRefreshed.Broadcast();
}

void UMALSkillMenuViewModel::RequestUnlock()
{
	if (!SelectedNode || !SelectedNode->CanBeUnlocked())
	{
		return;
	}

	if (!UnlockRequested)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UMALSkillMenuViewModel: no UnlockRequested handler is set, so the unlock never leaves the panel."));
		return;
	}

	UnlockRequested(SelectedNode->GetSkillTag());
}

void UMALSkillMenuViewModel::OnAbilitySystemReady(UAbilitySystemComponent* InAbilitySystemComponent)
{
	Super::OnAbilitySystemReady(InAbilitySystemComponent);

	// The level and the point bank are replicated attributes, and binding them is what makes the
	// panel work for a client: the progression component's own level delegate fires on the
	// authority only, so a client would never learn that a requirement had just been met.
	BindAttribute(UMALProgressionAttributeSet::GetLevelAttribute(),
		[this](const FOnAttributeChangeData&)
		{
			RefreshAll();
		});

	BindAttribute(UMALProgressionAttributeSet::GetSkillPointsAttribute(),
		[this](const FOnAttributeChangeData&)
		{
			RefreshAll();
		});

	// The unlocked set is not an attribute, so it arrives through the component instead. The
	// component broadcasts from both ends of the wire: on the authority when a node is bought,
	// and on the client from the rep notify.
	BindProgression();
}

void UMALSkillMenuViewModel::OnAbilitySystemReleased()
{
	UnbindProgression();

	Super::OnAbilitySystemReleased();
}

void UMALSkillMenuViewModel::BuildNodes()
{
	TArray<TObjectPtr<UMALSkillNodeViewModel>> BuiltNodes;

	const AMALPlayerState* PlayerState = CachedPlayerState.Get();
	const UMALProgressionComponent* Progression = PlayerState ? PlayerState->GetProgressionComponent() : nullptr;
	const UMALSkillTreeDefinition* Tree = Progression ? Progression->GetSkillTree() : nullptr;

	if (Tree)
	{
		BuiltNodes.Reserve(Tree->Nodes.Num());
		for (const FMALSkillNodeDefinition& NodeDefinition : Tree->Nodes)
		{
			// A node without a tag cannot be unlocked, saved or looked up, so it is not drawn
			// rather than drawn as something that never works.
			if (!NodeDefinition.SkillTag.IsValid())
			{
				continue;
			}

			UMALSkillNodeViewModel* NodeViewModel = NewObject<UMALSkillNodeViewModel>(this);
			NodeViewModel->Initialize(NodeDefinition);
			BuiltNodes.Add(NodeViewModel);
		}

		// Stable: inside one tier the order the tree asset lists the nodes in survives, which is
		// an author's only way to say what comes first.
		Algo::StableSortBy(BuiltNodes, [](const TObjectPtr<UMALSkillNodeViewModel>& Node)
		{
			return Node ? Node->GetTier() : 0;
		});
	}
	else if (PlayerState)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UMALSkillMenuViewModel: %s has no skill tree. Assign one on its class definition to fill the panel."),
			*GetNameSafe(PlayerState));
	}

	// Assigned through the macro so an already bound view rebuilds its rows.
	UE_MVVM_SET_PROPERTY_VALUE(Nodes, BuiltNodes);

	// The old selection points at a view model that is about to be garbage, and its tier band
	// and identity no longer mean anything.
	if (SelectedNode)
	{
		SelectedNode->SetSelected(false);
	}

	TObjectPtr<UMALSkillNodeViewModel> NoSelection = nullptr;
	UE_MVVM_SET_PROPERTY_VALUE(SelectedNode, NoSelection);
	UE_MVVM_SET_PROPERTY_VALUE(bCanUnlockSelected, false);
}

void UMALSkillMenuViewModel::BindProgression()
{
	UnbindProgression();

	AMALPlayerState* PlayerState = CachedPlayerState.Get();
	UMALProgressionComponent* Progression = PlayerState ? PlayerState->GetProgressionComponent() : nullptr;
	if (!Progression)
	{
		return;
	}

	// AddUObject rather than a lambda: the component outlives the panel, so the listener has to
	// be bound weakly to the view model.
	BoundProgression = Progression;
	SkillsChangedHandle = Progression->OnSkillsChanged.AddUObject(this, &UMALSkillMenuViewModel::HandleSkillsChanged);
}

void UMALSkillMenuViewModel::UnbindProgression()
{
	if (UMALProgressionComponent* Progression = BoundProgression.Get())
	{
		Progression->OnSkillsChanged.Remove(SkillsChangedHandle);
	}

	BoundProgression.Reset();
	SkillsChangedHandle.Reset();
}

void UMALSkillMenuViewModel::RefreshSelection()
{
	UE_MVVM_SET_PROPERTY_VALUE(bCanUnlockSelected, SelectedNode && SelectedNode->CanBeUnlocked());
}

void UMALSkillMenuViewModel::HandleSkillsChanged()
{
	RefreshAll();
}
