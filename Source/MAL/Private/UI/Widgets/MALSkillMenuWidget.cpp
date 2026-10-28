// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Widgets/MALSkillMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Data/MALSkillTreeDefinition.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "Player/MALPlayerController.h"
#include "Player/MALPlayerState.h"
#include "UI/ViewModel/MALSkillMenuViewModel.h"
#include "UI/ViewModel/MALSkillNodeViewModel.h"
#include "UI/Widgets/MALSkillNodeWidget.h"

UMALSkillMenuWidget::UMALSkillMenuWidget()
{
	// Not pausable: enhanced input stops ticking on a paused world, so a panel that paused would
	// swallow the very key that opened it and could never be closed with that key again.
	bPausesGame = false;
}

UWidget* UMALSkillMenuWidget::CreateTierHeader_Implementation(int32 Tier)
{
	// No heading unless the blueprint builds one.
	return nullptr;
}

void UMALSkillMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Construct runs again every time the widget is re-added to the viewport. The player state
	// and the tree behind it do not change while the panel is alive, so one build is enough;
	// a build that found the blueprint unconfigured is retried next time.
	if (!bPanelBuilt)
	{
		bPanelBuilt = BuildPanel();
	}
	else
	{
		HandleMenuRefreshed();
	}

	// AddUniqueDynamic semantics: NativeConstruct runs per visit, and a plain AddDynamic would
	// stack one more handler each time.
	if (UnlockButton)
	{
		UnlockButton->OnClicked.AddUniqueDynamic(this, &UMALSkillMenuWidget::RequestUnlock);
	}
}

void UMALSkillMenuWidget::NativeDestruct()
{
	if (MenuViewModel)
	{
		MenuViewModel->OnMenuRefreshed.Remove(MenuRefreshedHandle);

		// The lambda captures this widget weakly, but dropping it makes the panel's intent clear
		// and costs nothing.
		MenuViewModel->UnlockRequested = nullptr;
	}

	MenuRefreshedHandle.Reset();

	Super::NativeDestruct();
}

bool UMALSkillMenuWidget::BuildPanel()
{
	if (!NodesBox)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UMALSkillMenuWidget: %s binds no NodesBox, so there is nowhere to put the tiles."),
			*GetNameSafe(this));
		return false;
	}

	if (!NodeWidgetClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UMALSkillMenuWidget: %s has no NodeWidgetClass. Assign the tile widget blueprint to fill the list."),
			*GetNameSafe(this));
		return false;
	}

	AMALPlayerState* PlayerState = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<AMALPlayerState>() : nullptr;
	if (!PlayerState)
	{
		// Normal on the first construct of a client whose player state has not replicated in
		// yet; the retry happens the next time the panel is shown.
		UE_LOG(LogTemp, Warning,
			TEXT("UMALSkillMenuWidget: %s has no MAL player state yet, so the panel stays empty for now."),
			*GetNameSafe(this));
		return false;
	}

	MenuViewModel = NewObject<UMALSkillMenuViewModel>(this);

	TWeakObjectPtr<UMALSkillMenuWidget> WeakThis(this);
	MenuViewModel->UnlockRequested = [WeakThis](FGameplayTag SkillTag)
	{
		if (UMALSkillMenuWidget* Menu = WeakThis.Get())
		{
			Menu->HandleUnlockRequested(SkillTag);
		}
	};

	// Bound before Initialize: that call ends in a RefreshAll, and the detail column has to be
	// wired by then or the first paint shows an empty panel.
	MenuRefreshedHandle = MenuViewModel->OnMenuRefreshed.AddUObject(this, &UMALSkillMenuWidget::HandleMenuRefreshed);

	MenuViewModel->InitializeFromPlayerState(PlayerState);

	if (!BuildNodeRows())
	{
		return false;
	}

	// The rows did not exist when Initialize refreshed, so paint once with everything in place.
	HandleMenuRefreshed();

	// Offered to the blueprint as well, so a design that wants to bind the point counter or the
	// selection straight to the view model can do that instead of relying on the C++ mirrors.
	SetViewModelByClass(MenuViewModel);

	return true;
}

bool UMALSkillMenuWidget::BuildNodeRows()
{
	NodeRows.Reset();
	NodesBox->ClearChildren();

	// Rows belong to the panel, not to the blueprint: the tree asset decides how many there are,
	// so whatever the designer left in the scroll box is replaced rather than mixed in.
	int32 LastTier = INDEX_NONE;

	for (UMALSkillNodeViewModel* NodeViewModel : MenuViewModel->Nodes)
	{
		if (!NodeViewModel)
		{
			continue;
		}

		const int32 Tier = NodeViewModel->GetTier();
		if (Tier != LastTier)
		{
			LastTier = Tier;

			if (UWidget* Header = CreateTierHeader(Tier))
			{
				NodesBox->AddChild(Header);

				if (UScrollBoxSlot* HeaderSlot = Cast<UScrollBoxSlot>(Header->Slot))
				{
					HeaderSlot->SetPadding(FMargin(0.0f, TierSpacing, 0.0f, RowSpacing));
					HeaderSlot->SetHorizontalAlignment(HAlign_Fill);
				}
			}
		}

		UMALSkillNodeWidget* Row = CreateWidget<UMALSkillNodeWidget>(this, NodeWidgetClass);
		if (!Row)
		{
			// BuildPanel checked NodeWidgetClass; a failure here is a class that is not a widget
			// and would fail for every node, so there is nothing to retry.
			break;
		}

		NodesBox->AddChild(Row);

		// The slot only exists once the child is in the panel.
		if (UScrollBoxSlot* RowSlot = Cast<UScrollBoxSlot>(Row->Slot))
		{
			RowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, RowSpacing));
			RowSlot->SetHorizontalAlignment(HAlign_Fill);
		}

		TWeakObjectPtr<UMALSkillMenuWidget> WeakThis(this);
		Row->SelectionRequested = [WeakThis](UMALSkillNodeViewModel* Selected)
		{
			if (UMALSkillMenuWidget* Menu = WeakThis.Get())
			{
				Menu->SelectNode(Selected);
			}
		};

		Row->SetNodeViewModel(NodeViewModel);
		NodeRows.Add(Row);
	}

	return true;
}

void UMALSkillMenuWidget::HandleMenuRefreshed()
{
	// Every tile, not just the changed one: a single unlock can move several nodes at once --
	// the bought one and whatever it unlocked as a prerequisite for something else.
	for (UMALSkillNodeWidget* Row : NodeRows)
	{
		if (Row)
		{
			Row->RefreshFromNodeViewModel();
		}
	}

	RefreshDetails();
}

void UMALSkillMenuWidget::RefreshDetails()
{
	const UMALSkillNodeViewModel* Node = MenuViewModel ? MenuViewModel->SelectedNode.Get() : nullptr;
	const bool bHasNode = Node != nullptr;

	if (SkillPointsText)
	{
		SkillPointsText->SetText(FText::AsNumber(MenuViewModel ? MenuViewModel->SkillPoints : 0));
	}

	if (SelectedNameText)
	{
		SelectedNameText->SetText(bHasNode ? Node->DisplayName : FText::GetEmpty());
	}

	if (SelectedDescriptionText)
	{
		SelectedDescriptionText->SetText(bHasNode ? Node->Description : FText::GetEmpty());
	}

	if (SelectedStateText)
	{
		SelectedStateText->SetText(bHasNode ? Node->StateText : FText::GetEmpty());
	}

	if (SelectedRequirementText)
	{
		SelectedRequirementText->SetText(bHasNode ? Node->RequirementText : FText::GetEmpty());
	}

	if (SelectedIconImage)
	{
		if (bHasNode && Node->Icon)
		{
			SelectedIconImage->SetBrushFromTexture(Node->Icon.Get(), /*bMatchSize*/ false);
		}

		SelectedIconImage->SetVisibility(bHasNode && Node->Icon
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Hidden);
	}

	if (UnlockButton)
	{
		// An owned node has nothing to buy, so the button leaves rather than sitting there dead;
		// a node the player cannot afford keeps the button visible but disabled, which is what
		// says "this is what the next point goes into".
		const bool bAlreadyOwned = bHasNode && Node->NodeState == EMALSkillNodeState::Unlocked;
		const bool bCanUnlock = MenuViewModel && MenuViewModel->bCanUnlockSelected;

		UnlockButton->SetVisibility(bAlreadyOwned || !bHasNode
			? ESlateVisibility::Collapsed
			: ESlateVisibility::Visible);
		UnlockButton->SetIsEnabled(bCanUnlock);
	}
}

void UMALSkillMenuWidget::SelectNode(UMALSkillNodeViewModel* Node)
{
	if (MenuViewModel)
	{
		MenuViewModel->SelectNode(Node);
	}
}

void UMALSkillMenuWidget::RequestUnlock()
{
	if (MenuViewModel)
	{
		MenuViewModel->RequestUnlock();
	}
}

void UMALSkillMenuWidget::HandleUnlockRequested(FGameplayTag SkillTag)
{
	AMALPlayerController* PlayerController = Cast<AMALPlayerController>(GetOwningPlayer());
	if (!PlayerController)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UMALSkillMenuWidget: %s has no MAL player controller, so the unlock request goes nowhere."),
			*GetNameSafe(this));
		return;
	}

	// The panel only asks. Whether the node is affordable, whether it is already owned and what
	// it costs are all decided again on the authority.
	PlayerController->ServerUnlockSkill(SkillTag);
}
