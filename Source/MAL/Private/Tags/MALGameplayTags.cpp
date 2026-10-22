// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tags/MALGameplayTags.h"
#include "GameplayTagsManager.h"

// Initialize static singleton pointer
UMALGameplayTags* UMALGameplayTags::TagsSingleton = nullptr;

UMALGameplayTags* UMALGameplayTags::Get()
{
	if (!TagsSingleton)
	{
		TagsSingleton = NewObject<UMALGameplayTags>();
		TagsSingleton->AddToRoot();
		TagsSingleton->InitializeNativeTags();
	}

	return TagsSingleton;
}

void UMALGameplayTags::InitializeNativeTags()
{
	UGameplayTagsManager& TagManager = UGameplayTagsManager::Get();

	// Status Tags
	Status_Dead = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Status.Dead")), TEXT("Entity is dead"));
	Status_Stunned = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Status.Stunned")), TEXT("Entity is stunned and cannot act"));
	Status_Shielded = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Status.Shielded")), TEXT("Entity has an active shield"));
	Status_Attacking = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Status.Attacking")), TEXT("Entity is performing an attack"));
	Status_Defending = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Status.Defending")), TEXT("Entity is defending"));
	Status_Dashing = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Status.Dashing")), TEXT("Entity is dashing"));
	Status_Casting = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Status.Casting")), TEXT("Entity is casting and cannot move"));
	Status_Buff_DamageUp = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Status.Buff.DamageUp")), TEXT("Entity is under a damage-boosting buff"));

	// Event Tags
	Event_Hit = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Event.Hit")), TEXT("Entity was hit or damaged"));
	Event_Death = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Event.Death")), TEXT("Entity died"));
	Event_ShieldBreak = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Event.ShieldBreak")), TEXT("Entity's shield was broken"));
	Event_WeaponHit = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Event.WeaponHit")), TEXT("Weapon collision detected a hit"));

	// Data Tags
	Data_Health = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Data.Health")), TEXT("Health attribute"));
	Data_Shield = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Data.Shield")), TEXT("Shield attribute"));
	Data_Damage = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Data.Damage")), TEXT("Damage attribute"));
	Data_DamageBonus = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Data.DamageBonus")), TEXT("Damage bonus percentage carried by buff GEs"));

	// Damage Type Tags
	Damage_Type_Physical = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Damage.Type.Physical")), TEXT("Physical damage channel"));
	Damage_Type_Magic = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Damage.Type.Magic")), TEXT("Magic damage channel"));

	// Ability Tags
	Ability_Attack = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Ability.Attack")), TEXT("Attack ability"));
	Ability_Defend = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Ability.Defend")), TEXT("Defend ability"));
	Ability_Dash = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Ability.Dash")), TEXT("Dash ability"));
	Ability_Buff = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Ability.Buff")), TEXT("Self-buff ability"));

	// Input Tags
	Input_Move = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Input.Move")), TEXT("Move input"));
	Input_Look = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Input.Look")), TEXT("Look input"));
	Input_Jump = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Input.Jump")), TEXT("Jump input"));
	Input_Attack = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Input.Attack")), TEXT("Attack input"));
	Input_Defend = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Input.Defend")), TEXT("Defend input"));
	Input_Dash = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Input.Dash")), TEXT("Dash input"));
	Input_Buff = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Input.Buff")), TEXT("Buff input"));
	Input_AbilityActivated = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Input.AbilityActivated")), TEXT("Ability activated - used by ASC for input forwarding routing"));

	// Gameplay Cue Tags
	GameplayCue_Buff_DamageUp = TagManager.AddNativeGameplayTag(FName(TEXT("GameplayCue.Buff.DamageUp")), TEXT("Looping aura shown while the damage buff is active"));

	// Primary Attribute Tags
	Attribute_Primary_Vigor = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Attribute.Primary.Vigor")), TEXT("Vigor - affects health and regen"));
	Attribute_Primary_Strength = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Attribute.Primary.Strength")), TEXT("Strength - affects physical attack"));
	Attribute_Primary_Dexterity = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Attribute.Primary.Dexterity")), TEXT("Dexterity - affects crit and attack speed"));
	Attribute_Primary_Endurance = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Attribute.Primary.Endurance")), TEXT("Endurance - affects defense and stamina"));
	Attribute_Primary_Spirit = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Attribute.Primary.Spirit")), TEXT("Spirit - affects mana and magic attack"));

	// Secondary Attribute Tags
	Attribute_Secondary_MaxHealth = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Attribute.Secondary.MaxHealth")), TEXT("Max health, derived from Vigor"));
	Attribute_Secondary_HealthRegen = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Attribute.Secondary.HealthRegen")), TEXT("Health regen per second, derived from Vigor"));
	Attribute_Secondary_MaxStamina = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Attribute.Secondary.MaxStamina")), TEXT("Max stamina, derived from Endurance"));
	Attribute_Secondary_StaminaRegen = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Attribute.Secondary.StaminaRegen")), TEXT("Stamina regen per second, derived from Endurance"));
	Attribute_Secondary_MaxMana = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Attribute.Secondary.MaxMana")), TEXT("Max mana, derived from Spirit"));
	Attribute_Secondary_ManaRegen = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Attribute.Secondary.ManaRegen")), TEXT("Mana regen per second, derived from Spirit"));
	Attribute_Secondary_PhysicalAttack = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Attribute.Secondary.PhysicalAttack")), TEXT("Physical attack power, derived from Strength"));
	Attribute_Secondary_MagicAttack = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Attribute.Secondary.MagicAttack")), TEXT("Magic attack power, derived from Spirit"));
	Attribute_Secondary_PhysicalDefense = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Attribute.Secondary.PhysicalDefense")), TEXT("Physical defense/armor, derived from Endurance"));
	Attribute_Secondary_MagicDefense = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Attribute.Secondary.MagicDefense")), TEXT("Magic defense/resist, derived from Spirit"));
	Attribute_Secondary_CritChance = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Attribute.Secondary.CritChance")), TEXT("Critical hit chance %, derived from Dexterity"));
	Attribute_Secondary_CritDamage = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Attribute.Secondary.CritDamage")), TEXT("Critical damage multiplier %, derived from Dexterity"));
	Attribute_Secondary_AttackSpeed = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Attribute.Secondary.AttackSpeed")), TEXT("Attack speed multiplier, derived from Dexterity"));

	// Standalone Attribute Tags
	Attribute_Standalone_Health = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Attribute.Standalone.Health")), TEXT("Current health pool"));
	Attribute_Standalone_Mana = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Attribute.Standalone.Mana")), TEXT("Current mana pool"));
	Attribute_Standalone_Stamina = TagManager.AddNativeGameplayTag(FName(TEXT("MAL.Attribute.Standalone.Stamina")), TEXT("Current stamina pool"));
}
