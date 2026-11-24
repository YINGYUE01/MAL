// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"
#include "UI/Framework/MALRetryPoll.h"
#include "UI/Widgets/MALUserWidget.h"
#include "MALBossBarItemWidget.generated.h"

class AActor;
class UAbilitySystemComponent;
class UMALBossBarViewModel;

/**
 * UMALBossBarItemWidget
 *
 * One row of the top-screen Boss bar list.
 *
 * Owns its own view model and binds it to the Boss's ASC, so health and the trailing bar take the
 * same replicated path the enemy nameplate uses. The HUD adds the item to its VerticalBox before
 * calling InitializeForBoss: View Binding only materializes once the widget is in the tree.
 *
 * The item owns its fade-out because a death is presentation, not a state change: the server
 * sends one remove with bFadeOut=true and the item decides how long the row stays visible.
 */
UCLASS()
class MAL_API UMALBossBarItemWidget : public UMALUserWidget
{
	GENERATED_BODY()

public:
	/** Bind the item to Boss and resolve its ASC. Call after the item is parented. */
	void InitializeForBoss(AActor* Boss);

	/** Fade to transparent over Duration, then remove and invoke OnFinished. */
	void FadeOutAndRemove(float Duration, TFunction<void()> OnFinished = nullptr);

	/** Cancel an in-flight fade; the same Boss re-entered combat. */
	void CancelFade();

	UFUNCTION(BlueprintPure, Category = "MAL|UI|Boss")
	UMALBossBarViewModel* GetBossBarViewModel() const { return ViewModel; }

	UFUNCTION(BlueprintPure, Category = "MAL|UI|Boss")
	bool IsFading() const { return bFading; }

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, Category = "MAL|UI|Boss")
	TObjectPtr<UMALBossBarViewModel> ViewModel;

private:
	/** Resolve the Boss ASC; a client can see the actor before its ability system is ready. */
	void ResolveBossAbilitySystem();

	TWeakObjectPtr<AActor> BossActor;

	FMALRetryPoll BindRetryPoll;

	bool bFading = false;
	float FadeElapsed = 0.0f;
	float FadeDuration = 0.0f;
	TFunction<void()> FadeFinished;
};