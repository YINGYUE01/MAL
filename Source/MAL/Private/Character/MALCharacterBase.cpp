// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/MALCharacterBase.h"
#include "Player/MALPlayerState.h"
#include "Components/MALAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/MALAttributeSet.h"
#include "Data/MALClassDefinition.h"
#include "Data/MALAbilitySet.h"
#include "GameplayEffect.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Tags/MALGameplayTags.h"

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

	BindHealthChange();
}

void AMALCharacterBase::BindHealthChange()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC)
	{
		// On a client the PlayerState (and with it the ASC) replicates in later; OnRep_PlayerState
		// runs this again once it does.
		return;
	}

	if (BoundAbilitySystemComponent.Get() == ASC && HealthChangedDelegateHandle.IsValid())
	{
		return;
	}

	// Re-possession can hand the character a different ASC. Drop the listener on the old one
	// first, otherwise the old component keeps calling into a character that no longer owns it.
	if (UAbilitySystemComponent* Previous = BoundAbilitySystemComponent.Get())
	{
		Previous->GetGameplayAttributeValueChangeDelegate(UMALAttributeSet::GetHealthAttribute())
			.Remove(HealthChangedDelegateHandle);
	}

	BoundAbilitySystemComponent = ASC;
	HealthChangedDelegateHandle = ASC->GetGameplayAttributeValueChangeDelegate(UMALAttributeSet::GetHealthAttribute())
		.AddUObject(this, &AMALCharacterBase::HandleHealthChanged);
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

void AMALCharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMALCharacterBase, bIsDead);
}

void AMALCharacterBase::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	// Clients receive the replicated health change too, but the death decision belongs to the
	// authority; their side is driven by OnRep_IsDead.
	if (bIsDead || Data.NewValue > 0.0f || !HasAuthority())
	{
		return;
	}

	// Health sitting at zero with MaxHealth also at zero means the attribute GameplayEffects have
	// not been applied yet. That is "not initialised", not "dead".
	const UMALAttributeSet* CurrentAttributeSet = GetMALAttributeSet();
	if (!CurrentAttributeSet || CurrentAttributeSet->GetMaxHealth() <= 0.0f)
	{
		return;
	}

	// The killing blow's instigator lives in the GameplayEffectSpec, which this delegate does not
	// carry. Call HandleDeath yourself from the damage pipeline if the killer is needed.
	HandleDeath(nullptr);
}

void AMALCharacterBase::HandleDeath(AActor* Killer)
{
	if (bIsDead || !HasAuthority())
	{
		return;
	}

	bIsDead = true;

	// The loose tag is what blocks abilities that declare Status.Dead in their activation tags;
	// cancelling clears the ones already running, such as an attack montage mid-swing.
	if (UMALAbilitySystemComponent* MALASC = GetMALAbilitySystemComponent())
	{
		MALASC->AddLooseGameplayTag(UMALGameplayTags::Get()->Status_Dead);
		MALASC->CancelAllAbilities();
	}

	// OnRep does not run on the authority, so the shared path is invoked explicitly.
	ApplyDeathState(Killer);
}

void AMALCharacterBase::ApplyDeathState(AActor* Killer)
{
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		// DisableMovement rather than StopMovement: the latter only zeroes the velocity, and the
		// corpse would resume falling or sliding as soon as something pushes it.
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}

	// Enemies: stop the behaviour tree, otherwise the dead pawn keeps patrolling or chasing.
	if (AAIController* AIController = Cast<AAIController>(GetController()))
	{
		if (UBrainComponent* BrainComponent = AIController->GetBrainComponent())
		{
			BrainComponent->StopLogic(TEXT("Dead"));
		}
	}

	// Players: cut input so the corpse stops buffering moves and attacks. The controller is not
	// replicated to other clients, so this only ever runs where it matters.
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		PlayerController->DisableInput(PlayerController);
	}

	OnDeath(Killer);
}

void AMALCharacterBase::OnRep_IsDead()
{
	if (bIsDead)
	{
		// The spec that carried the killer never leaves the server.
		ApplyDeathState(nullptr);
	}
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
