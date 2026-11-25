// Copyright Epic Games, Inc. All Rights Reserved.

#include "MALBossSkillKitMigration.h"

#if WITH_EDITOR

#include "Data/MALBossSkillKit.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogMALBossSkillKitMigration, Log, All);

namespace MALBossSkillKitMigration
{
	const TCHAR* KitPath = TEXT("/Game/Blueprint/AI/Boss/DA_BossSkillKit_Default");
	const FName KitAssetName(TEXT("DA_BossSkillKit_Default"));
	const TCHAR* BossBlueprintPath = TEXT("/Game/Blueprint/AI/Boss/BP_Boss.BP_Boss");

	const TCHAR* BossDisplayName = TEXT("Hammer");
	constexpr float BossBarRadius = 1500.0f;

	struct FEntrySeed
	{
		const TCHAR* ClassPath;
		bool bSelectable;
		float MinRange;
		float MaxRange;
		float PreferredRange;
		float RangeTolerance;
		TArray<float> PhaseWeights;
		bool bRequiresLineOfSight;
		bool bRadial;
		float CastingMultiplier;
		float AttackingMultiplier;
		float BehindMultiplier;
		float LowHealthMultiplier;
		float LowHealthThreshold;
		int32 MaxConsecutive;
	};

	/**
	 * Snapshot of the kit as authored after the single-entry refactor. Re-running the commandlet
	 * rewrites the asset from this table, so keep it in sync when the template changes.
	 */
	const TArray<FEntrySeed>& GetSeeds()
	{
		static const TArray<FEntrySeed> Seeds =
		{
			{ TEXT("/Game/Blueprint/AI/BT/Ability/GA_Boos_Attack2Ability.GA_Boos_Attack2Ability_C"), true,
				0.0f, 1000.0f, 160.0f, 120.0f, { 2.0f, 2.0f, 2.0f }, false, false, 1.5f, 1.35f, 1.2f, 1.6f, 0.3f, 100 },
			{ TEXT("/Game/Blueprint/AI/BT/Ability/GA_Boos_Attack3Ability.GA_Boos_Attack3Ability_C"), true,
				300.0f, 2000.0f, 1500.0f, 400.0f, { 3.0f, 2.8f, 2.4f }, false, false, 1.2f, 1.35f, 1.5f, 1.2f, 0.3f, 1000 },
			{ TEXT("/Game/Blueprint/AI/BT/Ability/GA_Boos_Attack1Ability.GA_Boos_Attack1Ability_C"), true,
				0.0f, 1000.0f, 180.0f, 100.0f, { 2.0f, 2.4f, 0.0f }, false, false, 1.3f, 1.35f, 1.2f, 1.2f, 0.3f, 2 },
			{ TEXT("/Game/Blueprint/GameAbilitySyste/Ability/Scythe/GA_Scythe_Combo1.GA_Scythe_Combo1_C"), true,
				350.0f, 1100.0f, 700.0f, 250.0f, { 0.0f, 3.0f, 3.2f }, true, false, 1.5f, 1.0f, 1.0f, 1.2f, 0.3f, 2 },
			{ TEXT("/Game/Blueprint/GameAbilitySyste/Ability/Scythe/GA_WhirlwindAbility.GA_WhirlwindAbility_C"), true,
				0.0f, 1000.0f, 200.0f, 120.0f, { 0.0f, 2.8f, 3.0f }, false, true, 1.2f, 1.35f, 1.0f, 1.2f, 0.3f, 2 },
			// Grant-only: granted but never picked by the selector.
			{ TEXT("/Game/Blueprint/GameAbilitySyste/Ability/Sword/GA_Combo2Ability.GA_Combo2Ability_C"), false,
				0.0f, 350.0f, 200.0f, 120.0f, {}, false, false, 1.5f, 1.35f, 1.5f, 1.6f, 0.3f, 2 },
		};
		return Seeds;
	}

	UBlueprint* LoadBossBlueprint()
	{
		return LoadObject<UBlueprint>(nullptr, BossBlueprintPath);
	}

	bool SavePackage(UObject* Asset)
	{
		if (!Asset)
		{
			return false;
		}

		UPackage* Package = Asset->GetOutermost();
		Package->MarkPackageDirty();

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;

		const FString FileName = FPackageName::LongPackageNameToFilename(
			Package->GetName(), FPackageName::GetAssetPackageExtension());
		const bool bSaved = UPackage::SavePackage(Package, Asset, *FileName, SaveArgs);
		UE_CLOG(!bSaved, LogMALBossSkillKitMigration, Error, TEXT("Failed to save %s."), *Package->GetName());
		return bSaved;
	}
}

UMALBossSkillKitMigrationCommandlet::UMALBossSkillKitMigrationCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UMALBossSkillKitMigrationCommandlet::Main(const FString& Params)
{
	using namespace MALBossSkillKitMigration;

	// 1. Skill kit asset.
	UMALBossSkillKit* Kit = LoadObject<UMALBossSkillKit>(nullptr, KitPath);
	if (!Kit)
	{
		UPackage* Package = CreatePackage(KitPath);
		Kit = NewObject<UMALBossSkillKit>(Package, KitAssetName, RF_Public | RF_Standalone);
		UE_LOG(LogMALBossSkillKitMigration, Display, TEXT("Created %s."), KitPath);
	}
	else
	{
		UE_LOG(LogMALBossSkillKitMigration, Display, TEXT("%s exists; rewriting contents."), KitPath);
	}

	Kit->Skills.Reset();
	for (const FEntrySeed& Seed : GetSeeds())
	{
		UClass* AbilityClass = LoadClass<UGameplayAbility>(nullptr, Seed.ClassPath);
		if (!AbilityClass)
		{
			UE_LOG(LogMALBossSkillKitMigration, Error, TEXT("Ability class not found: %s"), Seed.ClassPath);
			return 1;
		}

		FMALBossSkillEntry& Entry = Kit->Skills.AddDefaulted_GetRef();
		Entry.AbilityClass = AbilityClass;
		Entry.bSelectable = Seed.bSelectable;
		Entry.MinRange = Seed.MinRange;
		Entry.MaxRange = Seed.MaxRange;
		Entry.PreferredRange = Seed.PreferredRange;
		Entry.RangeTolerance = Seed.RangeTolerance;
		Entry.PhaseWeights = Seed.PhaseWeights;
		Entry.bRequiresLineOfSight = Seed.bRequiresLineOfSight;
		Entry.bRadial = Seed.bRadial;
		Entry.CastingMultiplier = Seed.CastingMultiplier;
		Entry.AttackingMultiplier = Seed.AttackingMultiplier;
		Entry.BehindMultiplier = Seed.BehindMultiplier;
		Entry.LowHealthMultiplier = Seed.LowHealthMultiplier;
		Entry.LowHealthThreshold = Seed.LowHealthThreshold;
		Entry.MaxConsecutive = Seed.MaxConsecutive;
	}

	// Phase thresholds keep the retired asset's authored values.
	Kit->Phase2Threshold = 0.66f;
	Kit->Phase3Threshold = 0.33f;
	Kit->PhaseTransitionDuration = 1.0f;
	Kit->PhaseTransitionGrace = 0.8f;

	// 2. Boss blueprint defaults.
	UBlueprint* BossBlueprint = LoadBossBlueprint();
	if (!BossBlueprint || !BossBlueprint->GeneratedClass)
	{
		UE_LOG(LogMALBossSkillKitMigration, Error, TEXT("Failed to load %s."), BossBlueprintPath);
		return 1;
	}

	UObject* BossDefaults = BossBlueprint->GeneratedClass->GetDefaultObject();
	auto SetProperty = [BossDefaults](const TCHAR* PropertyName, auto Setter) -> bool
	{
		FProperty* Property = BossDefaults->GetClass()->FindPropertyByName(PropertyName);
		if (!Property)
		{
			UE_LOG(LogMALBossSkillKitMigration, Error, TEXT("Property '%s' not found on %s."),
				PropertyName, *BossDefaults->GetClass()->GetName());
			return false;
		}
		Setter(BossDefaults, Property);
		return true;
	};

	const bool bKitSet = SetProperty(TEXT("BossSkillKit"), [Kit](UObject* Object, FProperty* Property)
	{
		if (FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
		{
			ObjectProperty->SetObjectPropertyValue_InContainer(Object, Kit);
		}
	});
	const bool bNameSet = SetProperty(TEXT("BossDisplayName"), [](UObject* Object, FProperty* Property)
	{
		if (FTextProperty* TextProperty = CastField<FTextProperty>(Property))
		{
			TextProperty->SetPropertyValue_InContainer(Object, FText::FromString(BossDisplayName));
		}
	});
	const bool bRadiusSet = SetProperty(TEXT("BossBarActivationRadius"), [](UObject* Object, FProperty* Property)
	{
		if (FFloatProperty* FloatProperty = CastField<FFloatProperty>(Property))
		{
			FloatProperty->SetPropertyValue_InContainer(Object, BossBarRadius);
		}
	});

	// The kit is now the only grant source for the Boss; InitialAbilities would be a second one.
	if (FArrayProperty* InitialAbilities = FindFProperty<FArrayProperty>(
		BossDefaults->GetClass(), TEXT("InitialAbilities")))
	{
		FScriptArrayHelper Helper(InitialAbilities, InitialAbilities->ContainerPtrToValuePtr<void>(BossDefaults));
		Helper.EmptyValues();
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(BossBlueprint);
	FKismetEditorUtilities::CompileBlueprint(BossBlueprint);

	UE_LOG(LogMALBossSkillKitMigration, Display,
		TEXT("BP_Boss updated: kit=%d name=%d radius=%d skills=%d (selectable=%d)"),
		bKitSet, bNameSet, bRadiusSet, Kit->Skills.Num(),
		Kit->Skills.FilterByPredicate([](const FMALBossSkillEntry& Entry) { return Entry.bSelectable; }).Num());

	// 3. Save both assets.
	const bool bKitSaved = SavePackage(Kit);
	const bool bBossSaved = SavePackage(BossBlueprint);

	UE_LOG(LogMALBossSkillKitMigration, Display, TEXT("Done (kit saved=%d, BP saved=%d)."),
		bKitSaved, bBossSaved);
	return (bKitSaved && bBossSaved) ? 0 : 1;
}

#endif // WITH_EDITOR