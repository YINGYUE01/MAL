// Copyright Epic Games, Inc. All Rights Reserved.

#include "MALBlendSpaceSetup.h"

#if WITH_EDITOR

#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Misc/Parse.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogMALBlendSpaceSetup, Log, All);

namespace
{
	/** Direction token in the clip name and the X value it should occupy. */
	struct FDirectionalSample
	{
		const TCHAR* Token;
		float Direction;
	};

	// X = 0 forward, +90 right, +/-180 backward, matching UMALAnimInstance::Direction
	// (atan2 of forward x velocity).
	const FDirectionalSample DirectionalSamples[] = {
		{ TEXT("F"), 0.0f },
		{ TEXT("FR45"), 45.0f },
		{ TEXT("R90"), 90.0f },
		{ TEXT("BR45"), 135.0f },
		{ TEXT("B"), 180.0f },
		{ TEXT("BL45"), -135.0f },
		{ TEXT("L90"), -90.0f },
		{ TEXT("FL45"), -45.0f },
	};

	const TCHAR* JogClipPrefix =
		TEXT("/Game/GrruzamSwordPack/Animations/Great_Sword/1_Movements/3__Jogging/A/GreatSword_Jogging_8Way_verA_");

	const TCHAR* IdleClipPath =
		TEXT("/Game/GrruzamSwordPack/Animations/Great_Sword/1_Movements/1__Idle/GreatSword_Idle");

	bool SaveBlendSpaceAsset(UPackage* Package, UObject* Asset)
	{
		if (!Package || !Asset)
		{
			return false;
		}

		Package->MarkPackageDirty();

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;

		const FString FileName = FPackageName::LongPackageNameToFilename(
			Package->GetName(), FPackageName::GetAssetPackageExtension());

		const bool bSaved = UPackage::SavePackage(Package, Asset, *FileName, SaveArgs);
		UE_CLOG(!bSaved, LogMALBlendSpaceSetup, Error, TEXT("Failed to save %s."), *Package->GetName());
		return bSaved;
	}
}

UMALBlendSpaceSetupCommandlet::UMALBlendSpaceSetupCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UMALBlendSpaceSetupCommandlet::Main(const FString& Params)
{
	FString BlendSpacePath = TEXT("/Game/Blueprint/Characters/MALCharacter/Locomotion/BS_Locomotion");
	FParse::Value(*Params, TEXT("BlendSpace="), BlendSpacePath);

	float Speed = 300.0f;
	FParse::Value(*Params, TEXT("Speed="), Speed);

	UBlendSpace* BlendSpace = LoadObject<UBlendSpace>(nullptr, *BlendSpacePath);
	if (!BlendSpace)
	{
		UE_LOG(LogMALBlendSpaceSetup, Error, TEXT("Blend space not found: %s"), *BlendSpacePath);
		return 1;
	}

	// Clear first: a second run must not add a second copy of every clip.
	while (BlendSpace->GetNumberOfBlendSamples() > 0)
	{
		BlendSpace->DeleteSample(0);
	}

	int32 Added = 0;

	if (UAnimSequence* Idle = LoadObject<UAnimSequence>(nullptr, IdleClipPath))
	{
		BlendSpace->AddSample(Idle, FVector(0.0f, 0.0f, 0.0f));
		++Added;
	}
	else
	{
		UE_LOG(LogMALBlendSpaceSetup, Warning, TEXT("Idle clip missing: %s"), IdleClipPath);
	}

	for (const FDirectionalSample& Sample : DirectionalSamples)
	{
		const FString ClipPath = FString(JogClipPrefix) + Sample.Token;
		UAnimSequence* Sequence = LoadObject<UAnimSequence>(nullptr, *ClipPath);
		if (!Sequence)
		{
			UE_LOG(LogMALBlendSpaceSetup, Warning, TEXT("Clip missing: %s"), *ClipPath);
			continue;
		}

		BlendSpace->AddSample(Sequence, FVector(Sample.Direction, Speed, 0.0f));
		++Added;
	}

	// Rebuild the grid used at runtime, then save.
	BlendSpace->ResampleData();
	BlendSpace->ValidateSampleData();

	if (!SaveBlendSpaceAsset(BlendSpace->GetOutermost(), BlendSpace))
	{
		return 1;
	}

	UE_LOG(LogMALBlendSpaceSetup, Display, TEXT("%s: %d samples at jog speed %.0f"),
		*BlendSpacePath, Added, Speed);

	return 0;
}

#endif // WITH_EDITOR
