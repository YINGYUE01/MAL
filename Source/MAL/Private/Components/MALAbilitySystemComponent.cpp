// Copyright Epic Games, Inc., All Rights Reserved.

#include "Components/MALAbilitySystemComponent.h"
#include "GameAbilities/MALGameplayAbility.h"
#include "Data/MALAbilitySet.h"
#include "GameplayTagContainer.h"
#include "AbilitySystem/Attributes/MALAttributeSet.h"

UMALAbilitySystemComponent::UMALAbilitySystemComponent()
{
}

void UMALAbilitySystemComponent::InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor)
{
	Super::InitAbilityActorInfo(InOwnerActor, InAvatarActor);

	// Initialize attribute sets so GE modifiers can reference them
	InitStats(UMALAttributeSet::StaticClass(), nullptr);
}

void UMALAbilitySystemComponent::GrantAbilitySet(const UMALAbilitySet* AbilitySet)
{
	if (!AbilitySet)
	{
		return;
	}

	for (const FMALAbilitySetEntry& Entry : AbilitySet->Abilities)
	{
		if (!Entry.AbilityClass)
		{
			continue;
		}

		FGameplayAbilitySpec AbilitySpec(Entry.AbilityClass, Entry.StartingLevel);
		FGameplayAbilitySpecHandle Handle = GiveAbility(AbilitySpec);

		if (Handle.IsValid())
		{
			// Determine the InputTag: use override if set, otherwise read from CDO
			FGameplayTag InputTag = Entry.OverrideInputTag;
			if (!InputTag.IsValid())
			{
				FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
				if (Spec && Spec->Ability)
				{
					if (const UMALGameplayAbility* MALAbility = Cast<UMALGameplayAbility>(Spec->Ability))
					{
						InputTag = MALAbility->InputActivatedTag;
					}
				}
			}

			// Store InputTag in DynamicAbilityTags for tag-based lookup
			if (InputTag.IsValid())
			{
				FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
				if (Spec)
				{
					Spec->DynamicAbilityTags.AddTag(InputTag);
				}
			}
		}
	}
}

bool UMALAbilitySystemComponent::UpgradeAbilityByInputTag(const FGameplayTag& InputTag)
{
	FGameplayAbilitySpec* Spec = FindAbilitySpecByInputTag(InputTag);
	if (!Spec)
	{
		return false;
	}

	const UMALGameplayAbility* MALAbility = Cast<UMALGameplayAbility>(Spec->Ability);
	if (!MALAbility)
	{
		return false;
	}

	const int32 CurrentLevel = Spec->Level;
	if (CurrentLevel >= MALAbility->MaxLevel)
	{
		return false;
	}

	Spec->Level = CurrentLevel + 1;
	MarkAbilitySpecDirty(*Spec);
	return true;
}

void UMALAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (Spec.DynamicAbilityTags.HasTagExact(InputTag))
		{
			InputPressedSpecHandles.AddUnique(Spec.Handle);
		}
	}
}

void UMALAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (Spec.DynamicAbilityTags.HasTagExact(InputTag))
		{
			InputReleasedSpecHandles.AddUnique(Spec.Handle);
		}
	}
}

void UMALAbilitySystemComponent::ProcessAbilityInput(float DeltaTime, bool bGamePaused)
{
	// 1. Process pressed inputs
	for (const FGameplayAbilitySpecHandle& Handle : InputPressedSpecHandles)
	{
		FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
		if (!Spec)
		{
			continue;
		}

		Spec->InputPressed = true;

		if (Spec->IsActive())
		{
			// Ability already active, forward press event (wakes WaitInputPress)
			AbilitySpecInputPressed(*Spec);
		}
		else
		{
			// Try to activate based on policy
			const UMALGameplayAbility* MALAbility = Cast<UMALGameplayAbility>(Spec->Ability);
			if (MALAbility && MALAbility->ActivationPolicy == EMALAbilityActivationPolicy::OnInputTriggered)
			{
				TryActivateAbility(Handle);
			}
		}

		// Add to held set
		if (!InputHeldSpecHandles.Contains(Handle))
		{
			InputHeldSpecHandles.Add(Handle);
		}
	}

	// 2. Process held inputs (WhileInputActive)
	for (const FGameplayAbilitySpecHandle& Handle : InputHeldSpecHandles)
	{
		FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
		if (!Spec || Spec->IsActive())
		{
			continue;
		}

		const UMALGameplayAbility* MALAbility = Cast<UMALGameplayAbility>(Spec->Ability);
		if (MALAbility && MALAbility->ActivationPolicy == EMALAbilityActivationPolicy::WhileInputActive)
		{
			TryActivateAbility(Handle);
		}
	}

	// 3. Process released inputs
	for (const FGameplayAbilitySpecHandle& Handle : InputReleasedSpecHandles)
	{
		FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
		if (!Spec)
		{
			continue;
		}

		Spec->InputPressed = false;

		if (Spec->IsActive())
		{
			// Forward release event (wakes WaitInputRelease)
			AbilitySpecInputReleased(*Spec);
		}

		// Remove from held set
		InputHeldSpecHandles.Remove(Handle);
	}

	// 4. Clear buffers
	InputPressedSpecHandles.Empty();
	InputReleasedSpecHandles.Empty();
}

void UMALAbilitySystemComponent::ClearAbilityInput()
{
	InputPressedSpecHandles.Empty();
	InputReleasedSpecHandles.Empty();
	InputHeldSpecHandles.Empty();
}

void UMALAbilitySystemComponent::AbilitySpecInputPressed(FGameplayAbilitySpec& Spec)
{
	Super::AbilitySpecInputPressed(Spec);

	if (Spec.IsActive())
	{
		InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed,
			Spec.Handle,
			Spec.ActivationInfo.GetActivationPredictionKey());
	}
}

void UMALAbilitySystemComponent::AbilitySpecInputReleased(FGameplayAbilitySpec& Spec)
{
	Super::AbilitySpecInputReleased(Spec);

	if (Spec.IsActive())
	{
		InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased,
			Spec.Handle,
			Spec.ActivationInfo.GetActivationPredictionKey());
	}
}

void UMALAbilitySystemComponent::OnRep_ActivateAbilities()
{
	Super::OnRep_ActivateAbilities();
	// Using DynamicAbilityTags instead of InputTagToAbilityMap - no rebuild needed
}

FGameplayAbilitySpec* UMALAbilitySystemComponent::FindAbilitySpecByInputTag(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return nullptr;
	}

	for (FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (Spec.DynamicAbilityTags.HasTagExact(InputTag))
		{
			return &Spec;
		}
	}

	return nullptr;
}
