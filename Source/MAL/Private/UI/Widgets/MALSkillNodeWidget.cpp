// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Widgets/MALSkillNodeWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "UI/ViewModel/MALSkillNodeViewModel.h"

bool UMALSkillNodeWidget::SetNodeViewModel(UMALSkillNodeViewModel* InViewModel)
{
	NodeViewModel = InViewModel;

	if (!NodeViewModel)
	{
		RefreshFromNodeViewModel();
		return false;
	}

	// Pushed into a view binding as well, when the blueprint declares one: the two are not
	// exclusive, and a tile that binds SelectedNode's scalar fields gets the same values the
	// refresh writes, without waiting for the menu's next sweep.
	SetViewModelByClass(NodeViewModel);

	RefreshFromNodeViewModel();
	return true;
}

void UMALSkillNodeWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// AddUniqueDynamic keeps a re-add to the viewport from stacking handlers. A checked button
	// would also swallow the click on the second bind, so the uniqueness matters twice over.
	if (SelectButton)
	{
		SelectButton->OnClicked.AddUniqueDynamic(this, &UMALSkillNodeWidget::HandleSelectClicked);
	}

	// The menu seeds the model before the tile is added in some paths, and after it in others;
	// repainting here covers both orders.
	RefreshFromNodeViewModel();
}

void UMALSkillNodeWidget::RefreshFromNodeViewModel()
{
	const bool bHasModel = NodeViewModel != nullptr;
	const EMALSkillNodeState State = bHasModel ? NodeViewModel->NodeState : EMALSkillNodeState::Locked;

	if (NameText)
	{
		NameText->SetText(bHasModel ? NodeViewModel->DisplayName : FText::GetEmpty());
	}

	if (CostText)
	{
		// Number only: whether it reads "3", "3 pts" or "3 points" is a design decision, so the
		// unit lives in the blueprint around this block rather than in the number itself. A node
		// that costs nothing prints no number at all, because a "0" reads as a price rather than
		// as "free".
		const bool bHasCost = bHasModel && NodeViewModel->Cost > 0;
		CostText->SetText(bHasCost ? FText::AsNumber(NodeViewModel->Cost) : FText::GetEmpty());
	}

	if (StateText)
	{
		StateText->SetText(bHasModel ? NodeViewModel->StateText : FText::GetEmpty());
	}

	if (IconImage)
	{
		UTexture2D* Icon = bHasModel ? NodeViewModel->Icon.Get() : nullptr;
		if (Icon)
		{
			IconImage->SetBrushFromTexture(Icon, /*bMatchSize*/ false);
		}

		// Kept on screen with a null brush rather than collapsed: a tile with a missing icon
		// should still hold its slot so the row does not shift when the icon arrives.
		IconImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	if (LockedOverlay)
	{
		LockedOverlay->SetVisibility(State == EMALSkillNodeState::Locked
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
	}

	if (UnlockedOverlay)
	{
		UnlockedOverlay->SetVisibility(State == EMALSkillNodeState::Unlocked
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
	}

	// Two mechanisms for three readings: the overlays say what the node is, the fade says how
	// far the player still is from using it. Available takes neither overlay -- the lock going
	// away while the tile stays dim is its whole message. Opacity rather than a third veil so a
	// tile author does not have to hand-paint one, and so a blueprint tint still applies.
	const float TargetOpacity =
		State == EMALSkillNodeState::Unlocked ? 1.0f
		: State == EMALSkillNodeState::Available ? AvailableOpacity
		: LockedOpacity;
	SetRenderOpacity(TargetOpacity);

	if (SelectionFrame)
	{
		const bool bSelected = bHasModel && NodeViewModel->bIsSelected;
		SelectionFrame->SetVisibility(bSelected
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
	}

	OnNodeRefreshed(State, bHasModel && NodeViewModel->CanBeUnlocked());
}

void UMALSkillNodeWidget::HandleSelectClicked()
{
	if (SelectionRequested && NodeViewModel)
	{
		SelectionRequested(NodeViewModel);
	}
}
