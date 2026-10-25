// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/Attributes/MALAttributeSet.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystem/MALGameplayEffectContext.h"
#include "Combat/MALCombatEvents.h"
#include "Combat/MALCombatEventsSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Tags/MALGameplayTags.h"

namespace
{
	/**
	 * Hand one resolved hit to the presentation layer.
	 *
	 * Only the authority knows the post-mitigation value, so a client-side predicted effect must
	 * not draw a number of its own on top of the replicated one. Routing lives in
	 * UMALCombatEventsSubsystem: the attribute layer publishes an event and never reaches into the
	 * player controller itself.
	 */
	void NotifyDamageNumber(UMALAttributeSet* AttributeSet, const FGameplayEffectSpec& Spec, float Damage, bool bKillingBlow)
	{
		AActor* Victim = AttributeSet ? AttributeSet->GetOwningActor() : nullptr;
		if (!Victim || !Victim->HasAuthority())
		{
			return;
		}

		UMALCombatEventsSubsystem* CombatEvents = Victim->GetWorld()
			? Victim->GetWorld()->GetSubsystem<UMALCombatEventsSubsystem>()
			: nullptr;
		if (!CombatEvents)
		{
			return;
		}

		const FGameplayEffectContext* EffectContext = Spec.GetContext().Get();

		// A plain GE (no MAL context) still gets a number; it just has no crit or channel metadata.
		const FMALGameplayEffectContext* MALContext = nullptr;
		if (EffectContext && EffectContext->GetScriptStruct() &&
			EffectContext->GetScriptStruct()->IsChildOf(FMALGameplayEffectContext::StaticStruct()))
		{
			MALContext = static_cast<const FMALGameplayEffectContext*>(EffectContext);
		}

		// Impact point when the hit carried one, otherwise above the victim's pelvis so the number
		// clears the character mesh instead of spawning inside it.
		//
		// The set lives on the PlayerState for player characters and on the pawn for enemies, so the
		// owning actor is not always what the number should hang over: a PlayerState keeps its own
		// location and never follows the pawn. Resolve it before measuring anything.
		AActor* LocationActor = Victim;
		if (const APlayerState* PlayerState = Cast<APlayerState>(Victim))
		{
			if (APawn* PlayerStatePawn = PlayerState->GetPawn())
			{
				LocationActor = PlayerStatePawn;
			}
		}

		FVector WorldLocation = LocationActor->GetActorLocation();
		WorldLocation.Z += LocationActor->GetSimpleCollisionHalfHeight() * 0.5f;
		if (EffectContext)
		{
			if (const FHitResult* HitResult = EffectContext->GetHitResult())
			{
				// A context can carry a hit result that never had a point filled in. Trusting it
				// blindly would park the number at the world origin.
				if (!HitResult->ImpactPoint.IsNearlyZero())
				{
					WorldLocation = HitResult->ImpactPoint;
				}
			}
		}

		FMALDamageResolvedEvent Event;
		Event.Victim = Victim;
		Event.Instigator = EffectContext ? EffectContext->GetInstigator() : nullptr;
		Event.WorldLocation = WorldLocation;
		Event.Amount = Damage;
		Event.bCritical = MALContext && MALContext->IsCriticalHit();
		Event.bMagic = MALContext && MALContext->IsMagicDamage();
		Event.bKillingBlow = bKillingBlow;

		CombatEvents->BroadcastDamageResolved(Event);
	}
}

UMALAttributeSet::UMALAttributeSet()
{
	// ========== Primary Stats ==========
	InitVigor(10.0f);
	InitStrength(10.0f);
	InitDexterity(10.0f);
	InitEndurance(10.0f);
	InitSpirit(10.0f);

	// ========== Secondary Stats ==========
	InitMaxHealth(350.0f);
	InitHealthRegen(6.0f);
	InitMaxStamina(230.0f);
	InitStaminaRegen(15.0f);
	InitMaxMana(250.0f);
	InitManaRegen(10.0f);
	InitPhysicalAttack(40.0f);
	InitMagicAttack(30.0f);
	InitPhysicalDefense(25.0f);
	InitMagicDefense(25.0f);
	InitCritChance(8.0f);
	InitCritDamage(150.0f);
	InitAttackSpeed(1.2f);
	InitDamageBonus(0.0f);

	// ========== Standalone ==========
	InitHealth(350.0f);
	InitMana(250.0f);
	InitStamina(230.0f);

	// ========== Meta ==========
	InitDamage(0.0f);
}

void UMALAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Everything here is public, deliberately.
	//
	// COND_OwnerOnly means "the owning connection", and the condition is baked per class rather
	// than per instance. A player's set lives on the PlayerState and has one, but an enemy's set
	// lives on the character, whose AI controller owns no connection: OwnerOnly would send its
	// combat stats to nobody and every client would read 0 from AttackSpeed, PhysicalDefense and
	// the rest. REPNOTIFY_OnChanged keeps the cost at one update per actual stat change.

	// Primary
	DOREPLIFETIME_CONDITION_NOTIFY(UMALAttributeSet, Vigor, COND_None, REPNOTIFY_OnChanged);
	DOREPLIFETIME_CONDITION_NOTIFY(UMALAttributeSet, Strength, COND_None, REPNOTIFY_OnChanged);
	DOREPLIFETIME_CONDITION_NOTIFY(UMALAttributeSet, Dexterity, COND_None, REPNOTIFY_OnChanged);
	DOREPLIFETIME_CONDITION_NOTIFY(UMALAttributeSet, Endurance, COND_None, REPNOTIFY_OnChanged);
	DOREPLIFETIME_CONDITION_NOTIFY(UMALAttributeSet, Spirit, COND_None, REPNOTIFY_OnChanged);

	// Secondary
	DOREPLIFETIME_CONDITION_NOTIFY(UMALAttributeSet, MaxHealth, COND_None, REPNOTIFY_OnChanged);
	DOREPLIFETIME_CONDITION_NOTIFY(UMALAttributeSet, HealthRegen, COND_None, REPNOTIFY_OnChanged);
	DOREPLIFETIME_CONDITION_NOTIFY(UMALAttributeSet, MaxStamina, COND_None, REPNOTIFY_OnChanged);
	DOREPLIFETIME_CONDITION_NOTIFY(UMALAttributeSet, StaminaRegen, COND_None, REPNOTIFY_OnChanged);
	DOREPLIFETIME_CONDITION_NOTIFY(UMALAttributeSet, MaxMana, COND_None, REPNOTIFY_OnChanged);
	DOREPLIFETIME_CONDITION_NOTIFY(UMALAttributeSet, ManaRegen, COND_None, REPNOTIFY_OnChanged);
	DOREPLIFETIME_CONDITION_NOTIFY(UMALAttributeSet, PhysicalAttack, COND_None, REPNOTIFY_OnChanged);
	DOREPLIFETIME_CONDITION_NOTIFY(UMALAttributeSet, MagicAttack, COND_None, REPNOTIFY_OnChanged);
	DOREPLIFETIME_CONDITION_NOTIFY(UMALAttributeSet, PhysicalDefense, COND_None, REPNOTIFY_OnChanged);
	DOREPLIFETIME_CONDITION_NOTIFY(UMALAttributeSet, MagicDefense, COND_None, REPNOTIFY_OnChanged);
	DOREPLIFETIME_CONDITION_NOTIFY(UMALAttributeSet, CritChance, COND_None, REPNOTIFY_OnChanged);
	DOREPLIFETIME_CONDITION_NOTIFY(UMALAttributeSet, CritDamage, COND_None, REPNOTIFY_OnChanged);
	DOREPLIFETIME_CONDITION_NOTIFY(UMALAttributeSet, AttackSpeed, COND_None, REPNOTIFY_OnChanged);
	DOREPLIFETIME_CONDITION_NOTIFY(UMALAttributeSet, DamageBonus, COND_None, REPNOTIFY_OnChanged);

	// Standalone
	// Current vitals are read by the enemy nameplates on every client.
	DOREPLIFETIME_CONDITION_NOTIFY(UMALAttributeSet, Health, COND_None, REPNOTIFY_OnChanged);
	DOREPLIFETIME_CONDITION_NOTIFY(UMALAttributeSet, Mana, COND_None, REPNOTIFY_OnChanged);
	DOREPLIFETIME_CONDITION_NOTIFY(UMALAttributeSet, Stamina, COND_None, REPNOTIFY_OnChanged);

	// Damage is a meta attribute and intentionally not replicated.
}

void UMALAttributeSet::OnRep_Vigor(const FGameplayAttributeData& OldVigor)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMALAttributeSet, Vigor, OldVigor);
}

void UMALAttributeSet::OnRep_Strength(const FGameplayAttributeData& OldStrength)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMALAttributeSet, Strength, OldStrength);
}

void UMALAttributeSet::OnRep_Dexterity(const FGameplayAttributeData& OldDexterity)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMALAttributeSet, Dexterity, OldDexterity);
}

void UMALAttributeSet::OnRep_Endurance(const FGameplayAttributeData& OldEndurance)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMALAttributeSet, Endurance, OldEndurance);
}

void UMALAttributeSet::OnRep_Spirit(const FGameplayAttributeData& OldSpirit)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMALAttributeSet, Spirit, OldSpirit);
}

void UMALAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMALAttributeSet, MaxHealth, OldMaxHealth);
}

void UMALAttributeSet::OnRep_HealthRegen(const FGameplayAttributeData& OldHealthRegen)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMALAttributeSet, HealthRegen, OldHealthRegen);
}

void UMALAttributeSet::OnRep_MaxStamina(const FGameplayAttributeData& OldMaxStamina)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMALAttributeSet, MaxStamina, OldMaxStamina);
}

void UMALAttributeSet::OnRep_StaminaRegen(const FGameplayAttributeData& OldStaminaRegen)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMALAttributeSet, StaminaRegen, OldStaminaRegen);
}

void UMALAttributeSet::OnRep_MaxMana(const FGameplayAttributeData& OldMaxMana)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMALAttributeSet, MaxMana, OldMaxMana);
}

void UMALAttributeSet::OnRep_ManaRegen(const FGameplayAttributeData& OldManaRegen)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMALAttributeSet, ManaRegen, OldManaRegen);
}

void UMALAttributeSet::OnRep_PhysicalAttack(const FGameplayAttributeData& OldPhysicalAttack)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMALAttributeSet, PhysicalAttack, OldPhysicalAttack);
}

void UMALAttributeSet::OnRep_MagicAttack(const FGameplayAttributeData& OldMagicAttack)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMALAttributeSet, MagicAttack, OldMagicAttack);
}

void UMALAttributeSet::OnRep_PhysicalDefense(const FGameplayAttributeData& OldPhysicalDefense)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMALAttributeSet, PhysicalDefense, OldPhysicalDefense);
}

void UMALAttributeSet::OnRep_MagicDefense(const FGameplayAttributeData& OldMagicDefense)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMALAttributeSet, MagicDefense, OldMagicDefense);
}

void UMALAttributeSet::OnRep_CritChance(const FGameplayAttributeData& OldCritChance)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMALAttributeSet, CritChance, OldCritChance);
}

void UMALAttributeSet::OnRep_CritDamage(const FGameplayAttributeData& OldCritDamage)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMALAttributeSet, CritDamage, OldCritDamage);
}

void UMALAttributeSet::OnRep_AttackSpeed(const FGameplayAttributeData& OldAttackSpeed)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMALAttributeSet, AttackSpeed, OldAttackSpeed);
}

void UMALAttributeSet::OnRep_DamageBonus(const FGameplayAttributeData& OldDamageBonus)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMALAttributeSet, DamageBonus, OldDamageBonus);
}

void UMALAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMALAttributeSet, Health, OldHealth);
}

void UMALAttributeSet::OnRep_Mana(const FGameplayAttributeData& OldMana)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMALAttributeSet, Mana, OldMana);
}

void UMALAttributeSet::OnRep_Stamina(const FGameplayAttributeData& OldStamina)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMALAttributeSet, Stamina, OldStamina);
}

void UMALAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	// ---------- Standalone: current resource values, clamped ----------
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
	else if (Attribute == GetManaAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxMana());
	}
	else if (Attribute == GetStaminaAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxStamina());
	}
	// ---------- Secondary: cap attributes, minimum 1 ----------
	else if (Attribute == GetMaxHealthAttribute()
		|| Attribute == GetMaxManaAttribute()
		|| Attribute == GetMaxStaminaAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.0f);
	}
	// ---------- Secondary: regen rates, minimum 0 ----------
	else if (Attribute == GetHealthRegenAttribute()
		|| Attribute == GetManaRegenAttribute()
		|| Attribute == GetStaminaRegenAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
	// ---------- Secondary: crit chance 0~100 ----------
	else if (Attribute == GetCritChanceAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, 100.0f);
	}
	// ---------- Secondary: attack speed 0.1~5.0 ----------
	else if (Attribute == GetAttackSpeedAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.1f, 5.0f);
	}
	// ---------- Secondary: damage bonus -90% ~ +500% ----------
	// The lower bound prevents stacking to permanent immunity at -100%; the upper bound only
	// guards a misconfigured curve.
	else if (Attribute == GetDamageBonusAttribute())
	{
		NewValue = FMath::Clamp(NewValue, -90.0f, 500.0f);
	}
	// ---------- Meta: damage cannot be negative ----------
	else if (Attribute == GetDamageAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
}

void UMALAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// Damage arrives as a meta value written by the GE; fold it into Health once and reset it.
	if (Data.EvaluatedData.Attribute == GetDamageAttribute())
	{
		const float LocalDamage = GetDamage();
		SetDamage(0.0f);

		// Verbose so damage math can be inspected on demand without spamming the log.
		UE_LOG(LogTemp, Verbose, TEXT("MALAttributeSet: %s Damage=%.2f Health=%.2f MaxHealth=%.2f"),
			*GetNameSafe(GetOwningActor()), LocalDamage, GetHealth(), GetMaxHealth());

		if (LocalDamage > 0.0f)
		{
			const float OldHealth = GetHealth();
			const float NewHealth = FMath::Clamp(OldHealth - LocalDamage, 0.0f, GetMaxHealth());
			SetHealth(NewHealth);

			const bool bKillingBlow = (OldHealth > 0.0f && NewHealth <= 0.0f);

			// Floating damage number: the same path serves enemies and the player, the recipients
			// are decided by who owns the victim and who landed the blow.
			NotifyDamageNumber(this, Data.EffectSpec, LocalDamage, bKillingBlow);

			if (bKillingBlow)
			{
				if (AActor* Owner = GetOwningActor())
				{
					FGameplayEventData Payload;
					Payload.Instigator = Data.EffectSpec.GetContext().GetInstigator();
					Payload.Target = Owner;
					Payload.EventMagnitude = LocalDamage;
					UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
						Owner, UMALGameplayTags::Get()->Event_Death, Payload);
				}
			}
		}
	}
	// A GE changed MaxHealth: keep the current Health from exceeding the new cap.
	else if (Data.EvaluatedData.Attribute == GetMaxHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
	}
}
