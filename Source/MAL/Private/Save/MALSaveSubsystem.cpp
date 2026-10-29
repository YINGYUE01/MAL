// Copyright Epic Games, Inc. All Rights Reserved.

#include "Save/MALSaveSubsystem.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/MALProgressionAttributeSet.h"
#include "Components/MALProgressionComponent.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Player/MALPlayerState.h"
#include "Save/MALSaveGame.h"

const TCHAR* UMALSaveSubsystem::SlotName = TEXT("MAL_Profile");

UMALSaveSubsystem* UMALSaveSubsystem::Get(const UObject* Context)
{
	if (!Context)
	{
		return nullptr;
	}

	UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(Context);
	return GameInstance ? GameInstance->GetSubsystem<UMALSaveSubsystem>() : nullptr;
}

bool UMALSaveSubsystem::HasSave() const
{
	return UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex);
}

bool UMALSaveSubsystem::SaveForPlayer(AMALPlayerState* PlayerState)
{
	if (!PlayerState || !PlayerState->HasAuthority())
	{
		return false;
	}

	UMALProgressionComponent* Progression = PlayerState->GetProgressionComponent();
	UMALProgressionAttributeSet* ProgressionSet = PlayerState->GetProgressionAttributeSet();
	if (!Progression || !ProgressionSet)
	{
		return false;
	}

	UMALSaveGame* Save = Cast<UMALSaveGame>(UGameplayStatics::CreateSaveGameObject(UMALSaveGame::StaticClass()));
	if (!Save)
	{
		return false;
	}

	Save->Level = FMath::Max(FMath::RoundToInt(ProgressionSet->GetLevel()), 1);
	Save->Experience = FMath::Max(FMath::RoundToInt(ProgressionSet->GetExperience()), 0);
	Save->AttributePoints = Progression->GetAttributePoints();
	Save->SkillPoints = Progression->GetSkillPoints();
	Save->UnlockedSkills = Progression->GetUnlockedSkills();
	Save->AllocatedPoints = Progression->GetAllocatedPointsArray();

	const bool bSaved = UGameplayStatics::SaveGameToSlot(Save, SlotName, UserIndex);

	UE_LOG(LogTemp, Log, TEXT("UMALSaveSubsystem: %s profile for %s (level %d, %d skills)"),
		bSaved ? TEXT("wrote") : TEXT("FAILED to write"), *GetNameSafe(PlayerState),
		Save->Level, Save->UnlockedSkills.Num());

	return bSaved;
}

bool UMALSaveSubsystem::LoadForPlayer(AMALPlayerState* PlayerState)
{
	if (!PlayerState || !PlayerState->HasAuthority())
	{
		return false;
	}

	// No slot is the normal first-run case, not an error.
	if (!UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex))
	{
		return false;
	}

	UMALSaveGame* Save = Cast<UMALSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex));
	UMALProgressionComponent* Progression = PlayerState->GetProgressionComponent();
	UAbilitySystemComponent* AbilitySystemComponent = PlayerState->GetAbilitySystemComponent();
	if (!Save || !Progression || !AbilitySystemComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("UMALSaveSubsystem: profile found but could not be applied to %s."),
			*GetNameSafe(PlayerState));
		return false;
	}

	// Written through the ASC for the same reason every other progression write is: a direct
	// write to the set skips the change notification the HUD is bound to.
	AbilitySystemComponent->SetNumericAttributeBase(
		UMALProgressionAttributeSet::GetLevelAttribute(), static_cast<float>(FMath::Max(Save->Level, 1)));
	AbilitySystemComponent->SetNumericAttributeBase(
		UMALProgressionAttributeSet::GetExperienceAttribute(), static_cast<float>(FMath::Max(Save->Experience, 0)));
	AbilitySystemComponent->SetNumericAttributeBase(
		UMALProgressionAttributeSet::GetAttributePointsAttribute(), static_cast<float>(FMath::Max(Save->AttributePoints, 0)));
	AbilitySystemComponent->SetNumericAttributeBase(
		UMALProgressionAttributeSet::GetSkillPointsAttribute(), static_cast<float>(FMath::Max(Save->SkillPoints, 0)));

	// Order matters: the allocation has to be in place before ApplyDefaultAttributes hands
	// control back and calls ReapplyAttributePoints, which is what puts those points on the
	// primaries. The skill set is restored here too, so the possession that follows grants them.
	Progression->SetAllocatedPoints(Save->AllocatedPoints);
	Progression->SetUnlockedSkills(Save->UnlockedSkills);

	// The denominator is derived, not stored: recompute it from the restored level.
	Progression->RefreshExperienceToNextLevel();

	UE_LOG(LogTemp, Log, TEXT("UMALSaveSubsystem: loaded profile for %s (level %d, %d skills)"),
		*GetNameSafe(PlayerState), Save->Level, Save->UnlockedSkills.Num());

	return true;
}

void UMALSaveSubsystem::DeleteSave()
{
	if (UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex))
	{
		UGameplayStatics::DeleteGameInSlot(SlotName, UserIndex);
	}
}
