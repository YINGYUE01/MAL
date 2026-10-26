// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameAbilities/MALCooldownGameplayEffect.h"

#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UMALCooldownGameplayEffect::UMALCooldownGameplayEffect()
{
	// A cooldown is a window, not an instant: an instant effect is removed as soon as it is
	// applied, which would leave the ability ready again on the same frame it was used.
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
}

void UMALCooldownGameplayEffect::PostLoad()
{
	Super::PostLoad();

	SyncGrantedTags();
}

#if WITH_EDITOR
void UMALCooldownGameplayEffect::PostCDOCompiled(const FPostCDOCompiledContext& Context)
{
	Super::PostCDOCompiled(Context);

	SyncGrantedTags();
}

void UMALCooldownGameplayEffect::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// The sync rewrites a component, so only run it when the tags themselves moved. Syncing on
	// every edit would dirty the asset each time an unrelated field is touched in the details panel.
	if (!PropertyChangedEvent.Property ||
		PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UMALCooldownGameplayEffect, CooldownTags))
	{
		SyncGrantedTags();
	}
}
#endif // WITH_EDITOR

void UMALCooldownGameplayEffect::SyncGrantedTags()
{
	if (CooldownTags.IsEmpty())
	{
		return;
	}

	UTargetTagsGameplayEffectComponent& TargetTags = FindOrAddComponent<UTargetTagsGameplayEffectComponent>();

	FInheritedTagContainer TagChanges;
	TagChanges.Added = CooldownTags;
	TargetTags.SetAndApplyTargetTagChanges(TagChanges);

	// The component stages the tags; this rebuilds the effect's cached containers so a freshly
	// loaded asset reports them before anything queries it.
	//
	// Skipped while the object is still loading. PostCDOCompiled also runs as part of loading a
	// blueprint (Preload, Serialize, Init, CDO Compile, PostLoad), so the CDO carries
	// RF_NeedPostLoad at that point and OnGameplayEffectChanged treats it as an error -- it asserts
	// that the effect is fully loaded. UGameplayEffect::PostCDOCompiled guards its own call the
	// same way. Nothing is lost: the PostLoad that follows re-runs this and rebuilds the caches.
	if (!HasAnyFlags(RF_NeedPostLoad))
	{
		OnGameplayEffectChanged();
	}
}
