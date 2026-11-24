// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Widgets/MALBossBarItemWidget.h"

#include "AbilitySystemComponent.h"
#include "Character/MALEnemyCharacter.h"
#include "UI/ViewModel/MALBossBarViewModel.h"

void UMALBossBarItemWidget::InitializeForBoss(AActor* Boss)
{
	if (!Boss)
	{
		return;
	}

	BossActor = Boss;

	if (!ViewModel)
	{
		ViewModel = NewObject<UMALBossBarViewModel>(this);
	}

	FText BossName = FText::FromString(TEXT("Boss"));
	if (const AMALEnemyCharacter* Enemy = Cast<AMALEnemyCharacter>(Boss))
	{
		BossName = Enemy->GetBossDisplayName();
	}

	// Set the name before the bindings are pushed, so the first frame already shows the right
	// title instead of flashing the previous widget's text.
	ViewModel->SetBossName(BossName);
	SetViewModelByClass(ViewModel);

	ResolveBossAbilitySystem();

	SetRenderOpacity(1.0f);
	bFading = false;
	FadeElapsed = 0.0f;
	FadeDuration = 0.0f;
	FadeFinished = nullptr;
}

void UMALBossBarItemWidget::ResolveBossAbilitySystem()
{
	if (!ViewModel)
	{
		return;
	}

	const AMALEnemyCharacter* Enemy = Cast<AMALEnemyCharacter>(BossActor.Get());
	if (!Enemy)
	{
		return;
	}

	if (UAbilitySystemComponent* AbilitySystemComponent = Enemy->GetAbilitySystemComponent())
	{
		ViewModel->SetAbilitySystemComponent(AbilitySystemComponent);
		BindRetryPoll.Reset(GetWorld());
		return;
	}

	// Enemy ASCs are subobjects on the pawn, so this normally succeeds on the first try. The
	// poll covers a client that received the actor before its ability system finished setting up.
	BindRetryPoll.OnMiss(GetWorld(),
		FTimerDelegate::CreateUObject(this, &UMALBossBarItemWidget::ResolveBossAbilitySystem),
		0.1f, 30, 1.0f);
}

void UMALBossBarItemWidget::FadeOutAndRemove(float Duration, TFunction<void()> OnFinished)
{
	if (bFading)
	{
		return;
	}

	if (Duration <= 0.0f || !IsInViewport())
	{
		if (OnFinished)
		{
			OnFinished();
		}

		RemoveFromParent();
		return;
	}

	bFading = true;
	FadeElapsed = 0.0f;
	FadeDuration = FMath::Max(Duration, 0.01f);
	FadeFinished = MoveTemp(OnFinished);
}

void UMALBossBarItemWidget::CancelFade()
{
	bFading = false;
	FadeElapsed = 0.0f;
	FadeDuration = 0.0f;
	FadeFinished = nullptr;
	SetRenderOpacity(1.0f);
}

void UMALBossBarItemWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bFading)
	{
		return;
	}

	FadeElapsed += InDeltaTime;
	const float Alpha = FMath::Clamp(1.0f - FadeElapsed / FadeDuration, 0.0f, 1.0f);
	SetRenderOpacity(Alpha);

	if (Alpha <= 0.0f)
	{
		bFading = false;

		TFunction<void()> Finished = MoveTemp(FadeFinished);
		FadeFinished = nullptr;

		RemoveFromParent();

		if (Finished)
		{
			Finished();
		}
	}
}

void UMALBossBarItemWidget::NativeDestruct()
{
	BindRetryPoll.Reset(GetWorld());

	if (ViewModel)
	{
		ViewModel->SetAbilitySystemComponent(nullptr);
	}

	FadeFinished = nullptr;

	Super::NativeDestruct();
}