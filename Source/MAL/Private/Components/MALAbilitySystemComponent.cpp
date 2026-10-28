// Copyright Epic Games, Inc., All Rights Reserved.

#include "Components/MALAbilitySystemComponent.h"
#include "GameAbilities/MALGameplayAbility.h"
#include "Data/MALAbilitySet.h"
#include "AbilitySystem/MALGameplayEffectContext.h"
#include "GameplayTagContainer.h"

UMALAbilitySystemComponent::UMALAbilitySystemComponent()
{
}

void UMALAbilitySystemComponent::InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor)
{
	Super::InitAbilityActorInfo(InOwnerActor, InAvatarActor);

	UE_LOG(LogTemp, Verbose, TEXT("MALASC::InitAbilityActorInfo owner=%s avatar=%s spawnedAttributes=%d"),
		*GetNameSafe(InOwnerActor), *GetNameSafe(InAvatarActor), GetSpawnedAttributes().Num());

	// NOTE: Do NOT call InitStats here. The AttributeSet is a default subobject of the
	// owner and UAbilitySystemComponent::InitializeComponent discovers it automatically.
	// Calling InitStats at this point would create a *second* AttributeSet because this
	// virtual is invoked before the owner's subobjects are scanned.
}

FGameplayEffectContextHandle UMALAbilitySystemComponent::MakeEffectContext() const
{
	// Mirrors UAbilitySystemComponent::MakeEffectContext but allocates the MAL context so
	// executions can write crit / channel metadata that replicates to clients.
	FGameplayEffectContextHandle Context = FGameplayEffectContextHandle(new FMALGameplayEffectContext());

	if (ensureMsgf(AbilityActorInfo.IsValid(), TEXT("MALASC::MakeEffectContext - AbilityActorInfo is not valid.")))
	{
		// By default use the owner and avatar as the instigator and causer.
		Context.AddInstigator(AbilityActorInfo->OwnerActor.Get(), AbilityActorInfo->AvatarActor.Get());
	}

	return Context;
}

TArray<FGameplayAbilitySpecHandle> UMALAbilitySystemComponent::GrantAbilitySet(const UMALAbilitySet* AbilitySet)
{
	TArray<FGameplayAbilitySpecHandle> GrantedHandles;

	if (!AbilitySet || !IsOwnerActorAuthoritative())
	{
		return GrantedHandles;
	}

	UE_LOG(LogTemp, Verbose, TEXT("MALASC::GrantAbilitySet %s -> %s"), *GetName(), *GetNameSafe(AbilitySet));

	for (const FMALAbilitySetEntry& Entry : AbilitySet->Abilities)
	{
		if (!Entry.AbilityClass)
		{
			continue;
		}

		// An override on the set entry wins over the ability's own tag.
		FGameplayTag InputTag = Entry.OverrideInputTag;
		if (!InputTag.IsValid())
		{
			if (const UMALGameplayAbility* MALAbility =
				Cast<UMALGameplayAbility>(Entry.AbilityClass->GetDefaultObject()))
			{
				InputTag = MALAbility->InputActivatedTag;
			}
		}

		const FGameplayAbilitySpecHandle Handle = GrantAbility(Entry.AbilityClass, InputTag, Entry.StartingLevel);
		if (Handle.IsValid())
		{
			GrantedHandles.Add(Handle);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("MALASC::GrantAbilitySet %s: granted %d abilities"), *GetName(), GrantedHandles.Num());

	return GrantedHandles;
}

FGameplayAbilitySpecHandle UMALAbilitySystemComponent::GrantAbility(
	TSubclassOf<UGameplayAbility> AbilityClass, FGameplayTag InputTag, int32 Level)
{
	if (!AbilityClass || !IsOwnerActorAuthoritative())
	{
		return FGameplayAbilitySpecHandle();
	}

	FGameplayAbilitySpec AbilitySpec(AbilityClass, FMath::Max(Level, 1));

	// Set BEFORE GiveAbility so the tag is part of the spec's initial replication payload
	// (DynamicAbilityTags are replicated with the spec).
	if (InputTag.IsValid())
	{
		AbilitySpec.GetDynamicSpecSourceTags().AddTag(InputTag);
	}

	const FGameplayAbilitySpecHandle Handle = GiveAbility(AbilitySpec);
	if (Handle.IsValid())
	{
		GrantedAbilityHandles.Add(Handle);
	}

	return Handle;
}

void UMALAbilitySystemComponent::ClearGrantedAbilities()
{
	if (!IsOwnerActorAuthoritative())
	{
		return;
	}

	for (const FGameplayAbilitySpecHandle& Handle : GrantedAbilityHandles)
	{
		ClearAbility(Handle);
	}

	GrantedAbilityHandles.Empty();
}

bool UMALAbilitySystemComponent::UpgradeAbilityByInputTag(const FGameplayTag& InputTag)
{
	if (!IsOwnerActorAuthoritative())
	{
		return false;
	}

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

void UMALAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& InputTag, const FInputActionValue& Value)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	LastInputValues.Add(InputTag, Value);

	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			InputPressedSpecHandles.AddUnique(Spec.Handle);
		}
	}
}

void UMALAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag& InputTag, const FInputActionValue& Value)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	LastInputValues.Add(InputTag, Value);

	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			InputReleasedSpecHandles.AddUnique(Spec.Handle);
		}
	}
}

FInputActionValue UMALAbilitySystemComponent::GetLastInputValue(const FGameplayTag& InputTag) const
{
	if (const FInputActionValue* Found = LastInputValues.Find(InputTag))
	{
		return *Found;
	}

	return FInputActionValue();
}

void UMALAbilitySystemComponent::ProcessAbilityInput(float DeltaTime, bool bGamePaused)
{
	if (bGamePaused)
	{
		// Drop whatever was buffered while the world was paused. Holding the buffers across the
		// pause would replay them the moment time resumes: a press made through the pause menu
		// would activate an ability the player never meant to use in gameplay.
		ClearAbilityInput();
		return;
	}

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
			// Ability already active, forward press event (wakes WaitInputPress / combo chaining)
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

			// WhileInputActive means "only while the key is held", so the release ends it. The
			// engine only forwards InputReleased and relies on the ability ending itself; an
			// ability that does not listen to the event would otherwise stay active forever.
			const UMALGameplayAbility* MALAbility = Cast<UMALGameplayAbility>(Spec->Ability);
			if (MALAbility && MALAbility->ActivationPolicy == EMALAbilityActivationPolicy::WhileInputActive
				&& Spec->IsActive())
			{
				CancelAbilityHandle(Handle);
			}
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
	LastInputValues.Empty();
}

void UMALAbilitySystemComponent::AbilitySpecInputPressed(FGameplayAbilitySpec& Spec)
{
	// Replicate the press to the server for abilities that opt in. The engine's native
	// AbilityLocalInputPressed does this too, but MAL feeds input through ProcessAbilityInput
	// and therefore has to mirror the behaviour here.
	if (Spec.Ability && Spec.Ability->bReplicateInputDirectly && !IsOwnerActorAuthoritative())
	{
		ServerSetInputPressed(Spec.Handle);
	}

	Super::AbilitySpecInputPressed(Spec);

	if (Spec.IsActive())
	{
		FPredictionKey PredictionKey;
		if (UGameplayAbility* Instance = Spec.GetPrimaryInstance())
		{
			PredictionKey = Instance->GetCurrentActivationInfoRef().GetActivationPredictionKey();
		}

		InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed,
			Spec.Handle,
			PredictionKey);
	}
}

void UMALAbilitySystemComponent::AbilitySpecInputReleased(FGameplayAbilitySpec& Spec)
{
	if (Spec.Ability && Spec.Ability->bReplicateInputDirectly && !IsOwnerActorAuthoritative())
	{
		ServerSetInputReleased(Spec.Handle);
	}

	Super::AbilitySpecInputReleased(Spec);

	if (Spec.IsActive())
	{
		FPredictionKey PredictionKey;
		if (UGameplayAbility* Instance = Spec.GetPrimaryInstance())
		{
			PredictionKey = Instance->GetCurrentActivationInfoRef().GetActivationPredictionKey();
		}

		InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased,
			Spec.Handle,
			PredictionKey);
	}
}

FGameplayAbilitySpec* UMALAbilitySystemComponent::FindAbilitySpecByInputTag(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return nullptr;
	}

	for (FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			return &Spec;
		}
	}

	return nullptr;
}
