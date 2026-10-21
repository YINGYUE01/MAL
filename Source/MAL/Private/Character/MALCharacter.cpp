// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/MALCharacter.h"
#include "Player/MALPlayerState.h"
#include "Components/MALAbilitySystemComponent.h"
#include "Data/MALClassDefinition.h"
#include "Data/MALAbilitySet.h"
#include "GameplayEffect.h"

AMALCharacter::AMALCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
}

UAbilitySystemComponent* AMALCharacter::GetAbilitySystemComponent() const
{
	if (const AMALPlayerState* MALPS = GetPlayerState<AMALPlayerState>())
	{
		return MALPS->GetAbilitySystemComponent();
	}
	return nullptr;
}

void AMALCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (AMALPlayerState* MALPS = GetPlayerState<AMALPlayerState>())
	{
		if (UMALAbilitySystemComponent* MALASC = Cast<UMALAbilitySystemComponent>(MALPS->GetAbilitySystemComponent()))
		{
			MALASC->InitAbilityActorInfo(MALPS, this);

			// Initialize default attributes via GameplayEffects (order matters: Primary -> Secondary -> Standalone)
			if (DefaultPrimaryAttribute)
			{
				UGameplayEffect* PrimaryGE = GetMutableDefault<UGameplayEffect>(DefaultPrimaryAttribute);
				MALASC->ApplyGameplayEffectToSelf(PrimaryGE, 1.0f, MALASC->MakeEffectContext());
			}
			if (DefaultSecondaryAttribute)
			{
				UGameplayEffect* SecondaryGE = GetMutableDefault<UGameplayEffect>(DefaultSecondaryAttribute);
				MALASC->ApplyGameplayEffectToSelf(SecondaryGE, 1.0f, MALASC->MakeEffectContext());
			}
			if (DefaultStandaloneAttribute)
			{
				UGameplayEffect* StandaloneGE = GetMutableDefault<UGameplayEffect>(DefaultStandaloneAttribute);
				MALASC->ApplyGameplayEffectToSelf(StandaloneGE, 1.0f, MALASC->MakeEffectContext());
			}

			// Grant abilities from ClassDefinition (Server only)
			if (ClassDefinition && MALASC->IsOwnerActorAuthoritative())
			{
				for (const TObjectPtr<UMALAbilitySet>& AbilitySet : ClassDefinition->GrantedAbilitySets)
				{
					if (AbilitySet)
					{
						MALASC->GrantAbilitySet(AbilitySet);
					}
				}
			}
		}
	}
}
