// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/MALCharacterBase.h"
#include "Player/MALPlayerState.h"
#include "Components/MALAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/MALAttributeSet.h"
#include "Data/MALClassDefinition.h"
#include "Data/MALAbilitySet.h"
#include "GameplayEffect.h"

AMALCharacterBase::AMALCharacterBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
}

UAbilitySystemComponent* AMALCharacterBase::GetAbilitySystemComponent() const
{
	if (AbilitySystemComponent)
	{
		return AbilitySystemComponent;
	}

	// Players: the ASC lives on the PlayerState. Fall back before InitializeAbilitySystem runs.
	if (const AMALPlayerState* MALPS = GetPlayerState<AMALPlayerState>())
	{
		return MALPS->GetAbilitySystemComponent();
	}

	return nullptr;
}

UMALAbilitySystemComponent* AMALCharacterBase::GetMALAbilitySystemComponent() const
{
	return Cast<UMALAbilitySystemComponent>(GetAbilitySystemComponent());
}

UMALAttributeSet* AMALCharacterBase::GetMALAttributeSet() const
{
	if (AttributeSet)
	{
		return AttributeSet;
	}

	if (const AMALPlayerState* MALPS = GetPlayerState<AMALPlayerState>())
	{
		return MALPS->GetAttributeSet();
	}

	return nullptr;
}

void AMALCharacterBase::InitializeAbilitySystem()
{
	// Players: pull the ASC/AttributeSet from the PlayerState. Enemies already own them
	// as default subobjects (assigned in AMALEnemyCharacter's constructor).
	if (!AbilitySystemComponent)
	{
		if (AMALPlayerState* MALPS = GetPlayerState<AMALPlayerState>())
		{
			AbilitySystemComponent = Cast<UMALAbilitySystemComponent>(MALPS->GetAbilitySystemComponent());
		}
	}

	if (!AttributeSet)
	{
		if (const AMALPlayerState* MALPS = GetPlayerState<AMALPlayerState>())
		{
			AttributeSet = MALPS->GetAttributeSet();
		}
	}

	if (AbilitySystemComponent)
	{
		// PlayerState-owned ASC -> OwnerActor is the PlayerState, Avatar is this character.
		// Character-owned ASC  -> OwnerActor is this character, Avatar is this character.
		if (AActor* OwnerActor = AbilitySystemComponent->GetOwner())
		{
			AbilitySystemComponent->InitAbilityActorInfo(OwnerActor, this);
		}
	}
}

void AMALCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	InitializeAbilitySystem();

	if (HasAuthority())
	{
		ApplyDefaultAttributes();
		GrantAbilities();
	}
}

void AMALCharacterBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	InitializeAbilitySystem();

	if (HasAuthority())
	{
		ApplyDefaultAttributes();
		GrantAbilities();
	}
}

void AMALCharacterBase::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	InitializeAbilitySystem();
}

void AMALCharacterBase::OnRep_Controller()
{
	Super::OnRep_Controller();

	InitializeAbilitySystem();
}

bool AMALCharacterBase::AreDefaultAttributesApplied() const
{
	return bDefaultAttributesApplied;
}

void AMALCharacterBase::MarkDefaultAttributesApplied()
{
	bDefaultAttributesApplied = true;
}

void AMALCharacterBase::ApplyDefaultAttributes()
{
	if (!HasAuthority() || AreDefaultAttributesApplied())
	{
		return;
	}

	UMALAbilitySystemComponent* MALASC = GetMALAbilitySystemComponent();
	if (!MALASC)
	{
		return;
	}

	auto ApplyDefaultGE = [this, MALASC](TSubclassOf<UGameplayEffect> GEClass)
	{
		if (!GEClass)
		{
			return;
		}

		FGameplayEffectContextHandle Context = MALASC->MakeEffectContext();
		Context.AddSourceObject(this);

		FGameplayEffectSpecHandle SpecHandle = MALASC->MakeOutgoingSpec(GEClass, 1.0f, Context);
		if (SpecHandle.IsValid())
		{
			MALASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		}
	};

	// Order matters: Primary -> Secondary -> Standalone (Standalone may read Max* values).
	ApplyDefaultGE(DefaultPrimaryAttribute);
	ApplyDefaultGE(DefaultSecondaryAttribute);
	ApplyDefaultGE(DefaultStandaloneAttribute);

	MarkDefaultAttributesApplied();

	UE_LOG(LogTemp, Verbose, TEXT("MALCharacterBase::ApplyDefaultAttributes %s: done"), *GetName());
}

void AMALCharacterBase::GrantAbilities()
{
	if (!HasAuthority() || bAbilitiesGranted)
	{
		return;
	}

	UMALAbilitySystemComponent* MALASC = GetMALAbilitySystemComponent();
	if (!MALASC)
	{
		return;
	}

	// Clear first so respawn/re-possession cannot duplicate specs or input tags.
	MALASC->ClearGrantedAbilities();

	if (ClassDefinition)
	{
		for (const TObjectPtr<UMALAbilitySet>& AbilitySet : ClassDefinition->GrantedAbilitySets)
		{
			if (AbilitySet)
			{
				MALASC->GrantAbilitySet(AbilitySet);
			}
		}
	}

	bAbilitiesGranted = true;

	UE_LOG(LogTemp, Verbose, TEXT("MALCharacterBase::GrantAbilities %s: done"), *GetName());
}
