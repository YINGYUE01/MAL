// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Widgets/MALHUDWidget.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Character/MALEnemyCharacter.h"
#include "Combat/MALLockOnComponent.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/Widgets/MALBossBarItemWidget.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Player/MALPlayerController.h"
#include "Rendering/DrawElements.h"
#include "Tags/MALGameplayTags.h"
#include "TimerManager.h"
#include "UI/ViewModel/MALAbilityBarViewModel.h"
#include "UI/ViewModel/MALProgressionViewModel.h"
#include "UI/ViewModel/MALStatViewModel.h"

namespace
{
	/** Default bar layout, used when the blueprint leaves AbilitySlotTags empty. */
	void AppendDefaultSlotTags(TArray<FGameplayTag>& OutTags)
	{
		const UMALGameplayTags* Tags = UMALGameplayTags::Get();
		OutTags.Add(Tags->Input_Attack_LMB);
		OutTags.Add(Tags->Input_Attack_Buff);
	}
}

void UMALHUDWidget::InitializeForPlayer(APlayerController* InPlayerController)
{
	OwningPlayerController = InPlayerController;

	// The marker follows a controller-owned component, so it is bound here rather than at paint
	// time: a lock can start while the HUD is not being redrawn for any other reason.
	if (AMALPlayerController* MALPlayerController = Cast<AMALPlayerController>(InPlayerController))
	{
		if (UMALLockOnComponent* LockOnComponent = MALPlayerController->GetLockOnComponent())
		{
			LockOnComponent->OnLockChanged.AddDynamic(this, &UMALHUDWidget::HandleLockChanged);
		}

		// Bind before the first Refresh so a sync that lands during construction is not missed.
		MALPlayerController->OnBossBarsChanged.RemoveAll(this);
		MALPlayerController->OnBossBarRemoved.RemoveAll(this);
		MALPlayerController->OnBossBarsReset.RemoveAll(this);
		MALPlayerController->OnBossBarsChanged.AddUObject(this, &UMALHUDWidget::HandleBossBarsChanged);
		MALPlayerController->OnBossBarRemoved.AddUObject(this, &UMALHUDWidget::HandleBossBarRemoved);
		MALPlayerController->OnBossBarsReset.AddUObject(this, &UMALHUDWidget::HandleBossBarsReset);
	}

	if (!StatViewModel)
	{
		StatViewModel = NewObject<UMALStatViewModel>(this);
	}

	// Publish the view model before the ASC exists: the widget's bindings then show the view
	// model's default values instead of logging a missing-source error on every construct.
	if (StatViewModelName.IsNone())
	{
		SetViewModelByClass(StatViewModel);
	}
	else
	{
		SetViewModelByName(StatViewModelName, StatViewModel);
	}

	if (!AbilityBarViewModel)
	{
		AbilityBarViewModel = NewObject<UMALAbilityBarViewModel>(this);

		TArray<FGameplayTag> SlotTags = AbilitySlotTags;
		if (SlotTags.Num() == 0)
		{
			AppendDefaultSlotTags(SlotTags);
		}

		// Built once: rebuilding on every InitializeForPlayer would reallocate the slots the
		// widget blueprint is already bound to.
		AbilityBarViewModel->InitializeSlots(SlotTags);
	}

	if (AbilityBarViewModelName.IsNone())
	{
		SetViewModelByClass(AbilityBarViewModel);
	}
	else
	{
		SetViewModelByName(AbilityBarViewModelName, AbilityBarViewModel);
	}

	if (!ProgressionViewModel)
	{
		ProgressionViewModel = NewObject<UMALProgressionViewModel>(this);
	}

	if (ProgressionViewModelName.IsNone())
	{
		SetViewModelByClass(ProgressionViewModel);
	}
	else
	{
		SetViewModelByName(ProgressionViewModelName, ProgressionViewModel);
	}

	PublishProgressionToNestedWidgets();

	TryBindAbilitySystem();

	// The controller may already hold a synced list (late join): draw it immediately.
	RefreshBossBars();
}

void UMALHUDWidget::PublishProgressionToNestedWidgets()
{
	if (!WidgetTree)
	{
		return;
	}

	// Show() runs before InitializeForPlayer and is what realises the widget tree, so a nested
	// widget builds its view while this HUD's sources are still empty. A child that names the
	// progression source as a parent-context reference resolves that link against an empty
	// parent and never re-resolves, leaving its bindings pushing nothing. Publishing the view
	// model straight into the child gives it a source to read; a child without a source of this
	// type rejects the call and keeps its own assignment.
	TArray<UWidget*> NestedWidgets;
	WidgetTree->GetAllWidgets(NestedWidgets);

	for (UWidget* NestedWidget : NestedWidgets)
	{
		UMALUserWidget* MALWidget = Cast<UMALUserWidget>(NestedWidget);
		if (!MALWidget || MALWidget == this)
		{
			continue;
		}

		if (MALWidget->SetViewModelByClass(ProgressionViewModel))
		{
			UE_LOG(LogTemp, Log, TEXT("UMALHUDWidget: published the progression view model to %s."),
				*GetNameSafe(MALWidget));
		}
	}
}

void UMALHUDWidget::PublishAbilitySystemToNestedWidgets(UAbilitySystemComponent* InAbilitySystemComponent)
{
	if (!WidgetTree || !InAbilitySystemComponent)
	{
		return;
	}

	// Same sweep as PublishProgressionToNestedWidgets, different payload: a nested widget that
	// follows attributes gets the ASC here instead of polling for it on its own.
	TArray<UWidget*> NestedWidgets;
	WidgetTree->GetAllWidgets(NestedWidgets);

	for (UWidget* NestedWidget : NestedWidgets)
	{
		UMALUserWidget* MALWidget = Cast<UMALUserWidget>(NestedWidget);
		if (!MALWidget || MALWidget == this)
		{
			continue;
		}

		MALWidget->NativeAbilitySystemReady(InAbilitySystemComponent);
	}
}

void UMALHUDWidget::TryBindAbilitySystem()
{
	UAbilitySystemComponent* AbilitySystemComponent = nullptr;

	if (APlayerController* PlayerController = OwningPlayerController.Get())
	{
		// The pawn's IAbilitySystemInterface already forwards to the PlayerState-owned ASC for
		// player characters, so this covers both ownership layouts.
		if (APawn* Pawn = PlayerController->GetPawn())
		{
			AbilitySystemComponent = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn);
		}

		// Fallback for the window where the pawn is not spawned yet but the PlayerState is.
		if (!AbilitySystemComponent)
		{
			if (APlayerState* PlayerState = PlayerController->PlayerState)
			{
				AbilitySystemComponent = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PlayerState);
			}
		}
	}

	if (AbilitySystemComponent)
	{
		StatViewModel->SetAbilitySystemComponent(AbilitySystemComponent);
		AbilityBarViewModel->SetAbilitySystemComponent(AbilitySystemComponent);
		ProgressionViewModel->SetAbilitySystemComponent(AbilitySystemComponent);
		PublishAbilitySystemToNestedWidgets(AbilitySystemComponent);
		BindRetryPoll.Reset(GetWorld());
		return;
	}

	// On a client the PlayerState replicates in after the HUD is created, so keep polling.
	// Past the fast window the poll drops to one attempt per second and never stops, so a later
	// possession can still bind.
	BindRetryPoll.OnMiss(GetWorld(),
		FTimerDelegate::CreateUObject(this, &UMALHUDWidget::TryBindAbilitySystem),
		AbilitySystemBindRetryInterval, AbilitySystemBindFastRetryCount, AbilitySystemBindSlowRetryInterval);
}

void UMALHUDWidget::HandleBossBarsChanged()
{
	RefreshBossBars();
}

void UMALHUDWidget::HandleBossBarRemoved(AActor* Boss, bool bFadeOut)
{
	RemoveBossBarItem(Boss, bFadeOut);
}

void UMALHUDWidget::HandleBossBarsReset()
{
	ClearBossBarItems();
	RefreshBossBars();
}

void UMALHUDWidget::RefreshBossBars()
{
	AMALPlayerController* MALPlayerController = Cast<AMALPlayerController>(OwningPlayerController.Get());
	if (!MALPlayerController)
	{
		return;
	}

	if (!BossBarList || !BossBarItemClass)
	{
		if (!bWarnedMissingBossBarSetup)
		{
			bWarnedMissingBossBarSetup = true;
			UE_LOG(LogTemp, Warning,
				TEXT("UMALHUDWidget: BossBarList/BossBarItemClass not set on %s; boss bars are disabled."),
				*GetNameSafe(this));
		}

		return;
	}

	const TArray<TWeakObjectPtr<AActor>>& ActiveBosses = MALPlayerController->GetActiveBossBars();

	TArray<AActor*> DesiredBosses;
	DesiredBosses.Reserve(ActiveBosses.Num());
	for (const TWeakObjectPtr<AActor>& WeakBoss : ActiveBosses)
	{
		if (AActor* Boss = WeakBoss.Get())
		{
			DesiredBosses.Add(Boss);
		}
	}

	// The server keeps the full list; the HUD only shows the most recent rows.
	if (DesiredBosses.Num() > MaxBossBars)
	{
		DesiredBosses.RemoveAt(0, DesiredBosses.Num() - MaxBossBars);
	}

	TSet<AActor*> DesiredSet;
	for (AActor* Boss : DesiredBosses)
	{
		DesiredSet.Add(Boss);
	}

	// Drop rows that fell outside the cap. A fading row is left alone so the death fade is not
	// cut short by the next refresh.
	TArray<TWeakObjectPtr<AActor>> KeysToRemove;
	for (const TPair<TWeakObjectPtr<AActor>, TObjectPtr<UMALBossBarItemWidget>>& Pair : BossBarItems)
	{
		AActor* Boss = Pair.Key.Get();
		UMALBossBarItemWidget* Item = Pair.Value.Get();
		if (!Item || !Boss)
		{
			if (Item)
			{
				Item->RemoveFromParent();
			}
			KeysToRemove.Add(Pair.Key);
			continue;
		}

		if (DesiredSet.Contains(Boss) || Item->IsFading())
		{
			continue;
		}

		const AMALEnemyCharacter* Enemy = Cast<AMALEnemyCharacter>(Boss);
		if (Enemy && Enemy->IsDead())
		{
			TWeakObjectPtr<AActor> Key = Pair.Key;
			Item->FadeOutAndRemove(BossBarDeathFadeDuration, [this, Key]()
			{
				BossBarItems.Remove(Key);
			});
		}
		else
		{
			Item->RemoveFromParent();
			KeysToRemove.Add(Pair.Key);
		}
	}

	for (const TWeakObjectPtr<AActor>& Key : KeysToRemove)
	{
		BossBarItems.Remove(Key);
	}

	// Add rows in server order. The widget is parented before InitializeForBoss because View
	// Binding only materializes once the widget is in the tree.
	for (AActor* Boss : DesiredBosses)
	{
		const TWeakObjectPtr<AActor> Key(Boss);
		TObjectPtr<UMALBossBarItemWidget>* Existing = BossBarItems.Find(Key);
		if (Existing && Existing->Get())
		{
			if (Existing->Get()->IsFading())
			{
				Existing->Get()->CancelFade();
			}
			continue;
		}

		UMALBossBarItemWidget* Item = CreateWidget<UMALBossBarItemWidget>(MALPlayerController, BossBarItemClass);
		if (!Item)
		{
			continue;
		}

		UVerticalBoxSlot* BarSlot = BossBarList->AddChildToVerticalBox(Item);
		if (BarSlot)
		{
			BarSlot->SetPadding(FMargin(0.0f, 4.0f));
			BarSlot->SetHorizontalAlignment(HAlign_Fill);
		}

		Item->InitializeForBoss(Boss);
		BossBarItems.Add(Key, Item);
	}
}

void UMALHUDWidget::RemoveBossBarItem(AActor* Boss, bool bFadeOut)
{
	if (!Boss)
	{
		return;
	}

	const TWeakObjectPtr<AActor> Key(Boss);
	TObjectPtr<UMALBossBarItemWidget>* Found = BossBarItems.Find(Key);
	if (!Found || !Found->Get())
	{
		BossBarItems.Remove(Key);
		return;
	}

	UMALBossBarItemWidget* Item = Found->Get();
	if (bFadeOut)
	{
		if (!Item->IsFading())
		{
			Item->FadeOutAndRemove(BossBarDeathFadeDuration, [this, Key]()
			{
				BossBarItems.Remove(Key);
			});
		}
	}
	else
	{
		Item->RemoveFromParent();
		BossBarItems.Remove(Key);
	}
}

void UMALHUDWidget::ClearBossBarItems()
{
	for (const TPair<TWeakObjectPtr<AActor>, TObjectPtr<UMALBossBarItemWidget>>& Pair : BossBarItems)
	{
		if (UMALBossBarItemWidget* Item = Pair.Value.Get())
		{
			Item->RemoveFromParent();
		}
	}

	BossBarItems.Reset();
}

void UMALHUDWidget::HandleLockChanged(AActor* LockedTarget)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (LockedTarget)
	{
		// The marker tracks a moving pawn, and Slate caches the paint between frames. A poll
		// that only runs while a lock is held costs nothing in the common case.
		World->GetTimerManager().SetTimer(LockMarkerPollTimer, this,
			&UMALHUDWidget::TickLockMarker, 1.0f / 60.0f, true);
	}
	else
	{
		World->GetTimerManager().ClearTimer(LockMarkerPollTimer);
		LockMarkerPollTimer.Invalidate();
	}

	InvalidateLayoutAndVolatility();
}

void UMALHUDWidget::TickLockMarker()
{
	InvalidateLayoutAndVolatility();
}

int32 UMALHUDWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	const int32 PaintedLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	const AMALPlayerController* MALPlayerController = Cast<AMALPlayerController>(GetOwningPlayer());
	AActor* Target = MALPlayerController ? MALPlayerController->GetLockedTarget() : nullptr;
	if (!Target)
	{
		return PaintedLayer;
	}

	// Aim at the torso rather than the pivot, which sits at the feet on most character meshes.
	const FVector WorldLocation = Target->GetActorLocation() + FVector(0.0f, 0.0f, 90.0f);

	FVector2D ScreenPosition = FVector2D::ZeroVector;
	if (!UGameplayStatics::ProjectWorldToScreen(GetOwningPlayer(), WorldLocation, ScreenPosition, true))
	{
		// Behind the camera: projecting would fold the marker onto the wrong side of the screen.
		return PaintedLayer;
	}

	// ProjectWorldToScreen returns viewport pixels; the widget paints in local slate units, which
	// differ by the DPI scale. Without this the marker drifts away from the target on 4K.
	const float ViewportScale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);
	const FVector2D Center = ScreenPosition / ViewportScale;

	const float HalfSize = 22.0f;
	const float ArmLength = 8.0f;
	const FLinearColor MarkerColor(1.0f, 0.82f, 0.25f, 0.95f);
	const FPaintGeometry PaintGeometry = AllottedGeometry.ToPaintGeometry();

	auto DrawLine = [&](const FVector2D& From, const FVector2D& To)
	{
		TArray<FVector2D> Points;
		Points.Add(From);
		Points.Add(To);

		FSlateDrawElement::MakeLines(OutDrawElements, PaintedLayer + 1, PaintGeometry, Points,
			ESlateDrawEffect::None, MarkerColor, true, 2.0f);
	};

	// Four corner brackets, drawn as two strokes each so they stay corners and not a box.
	auto DrawCorner = [&](float SignX, float SignY)
	{
		const FVector2D Corner = Center + FVector2D(SignX * HalfSize, SignY * HalfSize);
		DrawLine(Corner, Corner + FVector2D(-SignX * ArmLength, 0.0f));
		DrawLine(Corner, Corner + FVector2D(0.0f, -SignY * ArmLength));
	};

	DrawCorner(-1.0f, -1.0f);
	DrawCorner(1.0f, -1.0f);
	DrawCorner(-1.0f, 1.0f);
	DrawCorner(1.0f, 1.0f);

	return PaintedLayer + 1;
}

void UMALHUDWidget::NativeDestruct()
{
	BindRetryPoll.Reset(GetWorld());

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LockMarkerPollTimer);
	}
	LockMarkerPollTimer.Invalidate();

	if (AMALPlayerController* MALPlayerController = Cast<AMALPlayerController>(OwningPlayerController.Get()))
	{
		MALPlayerController->OnBossBarsChanged.RemoveAll(this);
		MALPlayerController->OnBossBarRemoved.RemoveAll(this);
		MALPlayerController->OnBossBarsReset.RemoveAll(this);

		if (UMALLockOnComponent* LockOnComponent = MALPlayerController->GetLockOnComponent())
		{
			LockOnComponent->OnLockChanged.RemoveDynamic(this, &UMALHUDWidget::HandleLockChanged);
		}
	}

	ClearBossBarItems();

	// Unbind before the widget goes away so the view model cannot push into half-destroyed
	// widget bindings.
	if (StatViewModel)
	{
		StatViewModel->SetAbilitySystemComponent(nullptr);
	}

	if (AbilityBarViewModel)
	{
		AbilityBarViewModel->SetAbilitySystemComponent(nullptr);
	}

	if (ProgressionViewModel)
	{
		ProgressionViewModel->SetAbilitySystemComponent(nullptr);
	}

	Super::NativeDestruct();
}
