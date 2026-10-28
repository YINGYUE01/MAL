// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/MALCharacterBase.h"
#include "Player/MALPlayerState.h"
#include "Combat/MALCombatEvents.h"
#include "Combat/MALCombatEventsSubsystem.h"
#include "Combat/MALWeaponComponent.h"
#include "Combat/MALComboComponent.h"
#include "Components/MALAbilitySystemComponent.h"
#include "Components/MALProgressionComponent.h"
#include "AbilitySystem/Attributes/MALAttributeSet.h"
#include "Data/MALClassDefinition.h"
#include "Data/MALAbilitySet.h"
#include "Engine/World.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameplayEffect.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Tags/MALGameplayTags.h"
#include "TimerManager.h"

AMALCharacterBase::AMALCharacterBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	// Melee hit windows are driven per character, so the component has to exist on both the
	// player pawn and the AI-owned enemy pawn before any attack ability activates.
	WeaponComponent = CreateDefaultSubobject<UMALWeaponComponent>(TEXT("WeaponComponent"));
	ComboComponent = CreateDefaultSubobject<UMALComboComponent>(TEXT("ComboComponent"));

	// Hangs off the body mesh so it follows the animated weapon bone. The attach socket comes from
	// the weapon definition (each weapon has its own), so it is set when a weapon is equipped, not
	// here. Collision is off: the hit volume is the separate weapon_collision box, and a second
	// blocker moving with the blade would push targets around during a swing.
	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(GetMesh());
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetGenerateOverlapEvents(false);
	WeaponMesh->SetCanEverAffectNavigation(false);
	WeaponMesh->SetVisibility(false);

	// Same role as WeaponMesh, for definitions that ship a rigged weapon. A separate component
	// because static and skeletal meshes are rendered through different component classes; only
	// ever one of the two is populated, and the other stays hidden and empty.
	WeaponSkeletalMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponSkeletalMesh"));
	WeaponSkeletalMesh->SetupAttachment(GetMesh());
	WeaponSkeletalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponSkeletalMesh->SetGenerateOverlapEvents(false);
	WeaponSkeletalMesh->SetCanEverAffectNavigation(false);
	WeaponSkeletalMesh->SetVisibility(false);
}

void AMALCharacterBase::SetBuiltInWeaponHidden(bool bHide, FName MaterialSlotName)
{
	USkeletalMeshComponent* BodyMesh = GetMesh();
	if (!BodyMesh || MaterialSlotName.IsNone())
	{
		return;
	}

	// INDEX_NONE means this mesh has no such material section, which is the normal case for
	// characters that do not carry their own blade.
	const int32 MaterialIndex = BodyMesh->GetMaterialIndex(MaterialSlotName);
	if (MaterialIndex == INDEX_NONE)
	{
		return;
	}

	// Visibility is tracked per LOD, so every LOD has to be told: hiding LOD0 only would make the
	// blade reappear the moment the character walks far enough away to drop a level.
	const int32 NumLODs = FMath::Max(BodyMesh->GetNumLODs(), 1);
	for (int32 LODIndex = 0; LODIndex < NumLODs; ++LODIndex)
	{
		// Section index mirrors the material index: ShowMaterialSection only uses it to route
		// through the LOD's material remap table, which is identity on the authored meshes here.
		BodyMesh->ShowMaterialSection(MaterialIndex, MaterialIndex, !bHide, LODIndex);
	}
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

const UMALClassDefinition* AMALCharacterBase::GetClassDefinition() const
{
	if (ClassDefinition)
	{
		return ClassDefinition;
	}

	const AMALCharacterBase* ClassDefault = GetClass()->GetDefaultObject<AMALCharacterBase>();
	return ClassDefault ? ClassDefault->ClassDefinition.Get() : nullptr;
}

int32 AMALCharacterBase::GetExperienceReward() const
{
	if (ExperienceReward > 0)
	{
		return ExperienceReward;
	}

	// Instances were placed, and serialized, while the reward still lived at zero everywhere.
	// Their own copy keeps winning over a later edit on the class default, so read through it.
	const AMALCharacterBase* ClassDefault = GetClass()->GetDefaultObject<AMALCharacterBase>();
	return ClassDefault ? ClassDefault->ExperienceReward : 0;
}

ETeamAttitude::Type AMALCharacterBase::GetTeamAttitudeTowards(const AActor& Other) const
{
	// Deliberately not FGenericTeamId::GetAttitude: the engine's default solver only compares ids,
	// so two unassigned characters come out as friendly and could never hurt each other.
	const FGenericTeamId OtherTeam = FGenericTeamId::GetTeamIdentifier(&Other);
	if (TeamId == FGenericTeamId::NoTeam || OtherTeam == FGenericTeamId::NoTeam)
	{
		return ETeamAttitude::Neutral;
	}

	return TeamId == OtherTeam ? ETeamAttitude::Friendly : ETeamAttitude::Hostile;
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

		// Death attribution comes from the resolved hit: the attribute change delegate carries no
		// effect spec, so it can never name the killer.
		if (UWorld* World = GetWorld())
		{
			if (UMALCombatEventsSubsystem* CombatEvents = World->GetSubsystem<UMALCombatEventsSubsystem>())
			{
				CombatEvents->OnDamageResolved.AddUObject(this, &AMALCharacterBase::HandleDamageResolved);
			}
		}
	}
}

void AMALCharacterBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// The subsystem and a PlayerState-owned ASC both outlive this pawn, so drop both listeners
	// instead of leaving stale entries behind.
	if (UWorld* World = GetWorld())
	{
		if (UMALCombatEventsSubsystem* CombatEvents = World->GetSubsystem<UMALCombatEventsSubsystem>())
		{
			CombatEvents->OnDamageResolved.RemoveAll(this);
		}

		// A death check still queued for next tick would call back into a pawn that is already
		// tearing down.
		World->GetTimerManager().ClearTimer(PendingDeathTimer);
		bPendingDeathCheck = false;
	}

	if (UAbilitySystemComponent* BoundASC = BoundAbilitySystemComponent.Get())
	{
		BoundASC->GetGameplayAttributeValueChangeDelegate(UMALAttributeSet::GetHealthAttribute())
			.Remove(HealthChangedDelegateHandle);
		HealthChangedDelegateHandle.Reset();
	}

	Super::EndPlay(EndPlayReason);
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

	if (!GetMALAbilitySystemComponent())
	{
		// Left unmarked on purpose: PossessedBy runs this again once the ASC exists, and marking it
		// applied here would leave the character with no attributes at all.
		return;
	}

	// Read for the log only. The primaries no longer scale with it: row 1 of the growth curve is
	// the level 1 baseline, and every level after that is paid out as attribute points instead.
	int32 Level = 1;
	if (const AMALPlayerState* MALPS = GetPlayerState<AMALPlayerState>())
	{
		Level = MALPS->GetProgressionLevel();
	}

	// Order matters: Primary -> Secondary -> Standalone (Standalone may read Max* values).
	// The primary effect is pinned to level 1 so the baseline is the same whatever level the
	// character spawns at; the other two are AttributeBased and ignore the spec level.
	ApplyAttributeGE(DefaultPrimaryAttribute, 1.0f);
	ApplyAttributeGE(DefaultSecondaryAttribute, 1.0f);
	ApplyAttributeGE(DefaultStandaloneAttribute, 1.0f);

	// The primary effect just overwrote the base values, so points already spent have to go back
	// on top of the baseline. Enemies have no PlayerState and skip this.
	if (AMALPlayerState* OwningPlayerState = GetPlayerState<AMALPlayerState>())
	{
		if (UMALProgressionComponent* Progression = OwningPlayerState->GetProgressionComponent())
		{
			Progression->ReapplyAttributePoints();
		}
	}

	MarkDefaultAttributesApplied();

	UE_LOG(LogTemp, Verbose, TEXT("MALCharacterBase::ApplyDefaultAttributes %s: done (level %d)"), *GetName(), Level);
}

void AMALCharacterBase::ApplyAttributeGE(TSubclassOf<UGameplayEffect> GEClass, float Level)
{
	if (!GEClass)
	{
		return;
	}

	UMALAbilitySystemComponent* MALASC = GetMALAbilitySystemComponent();
	if (!MALASC)
	{
		return;
	}

	FGameplayEffectContextHandle Context = MALASC->MakeEffectContext();
	Context.AddSourceObject(this);

	// A spec level below 1 is not meaningful for ScalableFloat magnitude lookups.
	FGameplayEffectSpecHandle SpecHandle = MALASC->MakeOutgoingSpec(GEClass, FMath::Max(Level, 1.0f), Context);
	if (SpecHandle.IsValid())
	{
		MALASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}
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
	// carry. UMALAttributeSet publishes the resolved hit right after folding damage into Health,
	// so defer the decision by a tick: HandleDamageResolved then kills the character with the
	// attribution, and this fallback only covers deaths that never went through the pipeline.
	if (UWorld* World = GetWorld())
	{
		// One queued check is enough: it reads the final health value, so repeated changes before
		// the tick fires (damage over time, several hits in one frame) must not queue more.
		if (!bPendingDeathCheck)
		{
			bPendingDeathCheck = true;
			PendingDeathTimer = World->GetTimerManager().SetTimerForNextTick(this, &AMALCharacterBase::HandlePendingDeath);
		}
	}
}

void AMALCharacterBase::HandleDamageResolved(const FMALDamageResolvedEvent& Event)
{
	if (!Event.bKillingBlow)
	{
		return;
	}

	// The event names the actor that owns the AttributeSet, which is the PlayerState for players
	// and this character for enemies. Comparing against the avatar would miss every player death.
	const UMALAttributeSet* CurrentAttributeSet = GetMALAttributeSet();
	if (!CurrentAttributeSet || Event.Victim.Get() != CurrentAttributeSet->GetOwningActor())
	{
		return;
	}

	// HandleDeath is guarded, so a second blow landing in the same frame is a no-op.
	HandleDeath(Event.Instigator);
}

void AMALCharacterBase::HandlePendingDeath()
{
	bPendingDeathCheck = false;

	if (bIsDead || !HasAuthority())
	{
		// Attribution already handled it, or a respawn won the race.
		return;
	}

	const UMALAttributeSet* CurrentAttributeSet = GetMALAttributeSet();
	if (!CurrentAttributeSet || CurrentAttributeSet->GetHealth() > 0.0f || CurrentAttributeSet->GetMaxHealth() <= 0.0f)
	{
		return;
	}

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

	// A corpse is scenery, not an obstacle. Movement is already off, so the character does not
	// need its capsule to hold position any more, and leaving it on the character profile makes
	// the player walk into an invisible body at the exact spot the enemy died.
	if (!bKeepCapsuleCollisionOnDeath)
	{
		if (UCapsuleComponent* Capsule = GetCapsuleComponent())
		{
			Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}

	// Death is also where a blueprint turns the mesh into a ragdoll, and this runs before the
	// blueprint hook below. Character meshes sit on the query-only CharacterMesh profile, so a
	// ragdoll built from them has nothing for the floor to push against and drops out of the
	// level. Swapping the profile here means whatever enables physics in OnDeath gets a body the
	// physics scene can resolve, regardless of what each character blueprint remembers to do.
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		if (!MeshComp->GetPhysicsAsset())
		{
			// Without a physics asset SetSimulatePhysics is a no-op, so the corpse just stops in
			// its death pose. Worth a warning: the intent was clearly a limp body.
			UE_LOG(LogTemp, Warning, TEXT("%s: mesh has no physics asset, the corpse cannot go limp"), *GetName());
		}
		else if (!CorpseMeshCollisionProfile.IsNone())
		{
			MeshComp->SetCollisionProfileName(CorpseMeshCollisionProfile);

			// Limbs follow the corpse, not the camera. Ragdoll bodies block traces by default, and a
			// third-person boom that traces against the body lying next to it snaps the view in on
			// every kill.
			MeshComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		}
	}

	// Only the server may own the destroy timer: a client that destroys its own copy would
	// desync from the machines that keep the actor alive.
	if (HasAuthority() && CorpseLifeSpan > 0.0f)
	{
		SetLifeSpan(CorpseLifeSpan);
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

	const UMALClassDefinition* Definition = GetClassDefinition();
	if (!Definition)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("MALCharacterBase::GrantAbilities %s: no ClassDefinition on the instance or the class default, so no abilities were granted"),
			*GetName());
	}
	else
	{
		for (const TObjectPtr<UMALAbilitySet>& AbilitySet : Definition->GrantedAbilitySets)
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
