// Copyright Epic Games, Inc. All Rights Reserved.

#include "MALUpperBodyVerify.h"

#if WITH_EDITOR

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/MALAnimInstance.h"
#include "Character/MALCharacterBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/Parse.h"

DEFINE_LOG_CATEGORY_STATIC(LogMALUpperBodyVerify, Log, All);

namespace MALUpperBodyVerify
{
	const TCHAR* CharacterClassPath = TEXT("/Game/Blueprint/Characters/MALCharacter/BP_MALCharacter.BP_MALCharacter_C");

	/** The shot, playing in UpperBodySlot. */
	const TCHAR* ShotMontagePath = TEXT("/Game/Blueprint/GameAbilitySyste/Ability/Archer/AM_ShotMontage");

	/**
	 * A full-body montage in DefaultSlot, used as the moving base.
	 *
	 * Both characters play it, so both legs are genuinely animated and any difference between them
	 * can only come from the shot montage. Driving the legs with a montage instead of locomotion
	 * keeps the comparison independent of the locomotion state machine, which needs input, a
	 * controller and a ticking game loop to select a state.
	 */
	const TCHAR* FullBodyMontagePath = TEXT("/Game/Blueprint/Characters/MALCharacter/AM_ComboMontage");

	/** One bone on each side of the split. */
	const FName LowerBodyBone = TEXT("thigh_l");
	const FName UpperBodyBone = TEXT("spine_03");

	/** The full-body montage has to move the legs at least this much, or the comparison is empty. */
	constexpr float BaseLegMotionDegrees = 2.f;

	/** Degrees that count as a real difference between the two characters. */
	constexpr float LegToleranceDegrees = 5.f;
	constexpr float UpperBodyMinimumDegrees = 5.f;

	constexpr float TickDelta = 1.f / 30.f;

	struct FPose
	{
		FRotator Lower = FRotator::ZeroRotator;
		FRotator Upper = FRotator::ZeroRotator;
	};

	UWorld* CreateVerifyWorld()
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false);
		if (!World)
		{
			UE_LOG(LogMALUpperBodyVerify, Error, TEXT("Could not create a world to verify in."));
			return nullptr;
		}

		FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
		WorldContext.SetCurrentWorld(World);

		World->InitializeActorsForPlay(FURL());
		World->BeginPlay();
		return World;
	}

	void DestroyVerifyWorld(UWorld* World)
	{
		if (!World)
		{
			return;
		}

		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	}

	/**
	 * Spawns a character whose mesh ticks its pose without a viewport.
	 *
	 * Nothing is ever "recently rendered" without one, and the default tick option reads that flag
	 * before ticking the pose; without AlwaysTickPoseAndRefreshBones the graph is never evaluated
	 * and every sample would be the reference pose.
	 */
	AMALCharacterBase* SpawnCharacter(UWorld* World, const TCHAR* Label)
	{
		UClass* CharacterClass = LoadClass<AMALCharacterBase>(nullptr, CharacterClassPath);
		if (!CharacterClass)
		{
			UE_LOG(LogMALUpperBodyVerify, Error, TEXT("%s not found."), CharacterClassPath);
			return nullptr;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AMALCharacterBase* Character = World->SpawnActor<AMALCharacterBase>(
			CharacterClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);

		if (!Character || !Character->GetMesh() || !Character->GetMesh()->GetAnimInstance())
		{
			UE_LOG(LogMALUpperBodyVerify, Error, TEXT("Could not spawn a usable character for %s."), Label);
			return nullptr;
		}

		USkeletalMeshComponent* Mesh = Character->GetMesh();
		Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		Mesh->bNoSkeletonUpdate = false;

		return Character;
	}

	/** Advances one pose tick and returns the two bones in component space. */
	FPose TickAndSample(USkeletalMeshComponent* Mesh)
	{
		Mesh->TickPose(TickDelta, /*bNeedsValidRootMotion=*/false);
		Mesh->RefreshBoneTransforms();

		FPose Pose;
		Pose.Lower = Mesh->GetSocketTransform(LowerBodyBone, RTS_Component).Rotator();
		Pose.Upper = Mesh->GetSocketTransform(UpperBodyBone, RTS_Component).Rotator();
		return Pose;
	}

	float AngularDistanceDegrees(const FRotator& A, const FRotator& B)
	{
		return static_cast<float>(A.Quaternion().AngularDistance(B.Quaternion())) * 180.f / PI;
	}
}

using namespace MALUpperBodyVerify;

UMALUpperBodyVerifyCommandlet::UMALUpperBodyVerifyCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UMALUpperBodyVerifyCommandlet::Main(const FString& Params)
{
	int32 FrameCount = 30;
	FParse::Value(*Params, TEXT("Frames="), FrameCount);

	UAnimMontage* ShotMontage = LoadObject<UAnimMontage>(nullptr, ShotMontagePath);
	if (!ShotMontage)
	{
		UE_LOG(LogMALUpperBodyVerify, Error, TEXT("%s not found."), ShotMontagePath);
		return 1;
	}

	UAnimMontage* FullBodyMontage = LoadObject<UAnimMontage>(nullptr, FullBodyMontagePath);
	if (!FullBodyMontage)
	{
		UE_LOG(LogMALUpperBodyVerify, Error, TEXT("%s not found."), FullBodyMontagePath);
		return 1;
	}

	UWorld* World = CreateVerifyWorld();
	if (!World)
	{
		return 1;
	}

	AMALCharacterBase* BaseCharacter = SpawnCharacter(World, TEXT("the full body reference"));
	AMALCharacterBase* ShotCharacter = SpawnCharacter(World, TEXT("the shot"));

	if (!BaseCharacter || !ShotCharacter)
	{
		DestroyVerifyWorld(World);
		return 1;
	}

	USkeletalMeshComponent* BaseMesh = BaseCharacter->GetMesh();
	USkeletalMeshComponent* ShotMesh = ShotCharacter->GetMesh();

	UAnimInstance* BaseAnimInstance = BaseMesh->GetAnimInstance();
	UAnimInstance* ShotAnimInstance = ShotMesh->GetAnimInstance();

	if (!Cast<UMALAnimInstance>(BaseAnimInstance))
	{
		UE_LOG(LogMALUpperBodyVerify, Error, TEXT("The spawned character is not animating with UMALAnimInstance."));
		DestroyVerifyWorld(World);
		return 1;
	}

	// The full-body montage starts on both characters on the same tick, so their lower bodies sit at
	// the same point of the same animation for every sample. The shot goes on one of them only.
	//
	// bStopAllMontages has to be false on the shot: its default stops whatever montage is already
	// running, which would remove the base pose the comparison is measured against.
	const float BaseWeight = BaseAnimInstance->Montage_Play(FullBodyMontage, 1.f);
	const float ShotBaseWeight = ShotAnimInstance->Montage_Play(FullBodyMontage, 1.f);
	const float ShotWeight = ShotAnimInstance->Montage_Play(
		ShotMontage, 1.f, EMontagePlayReturnType::MontageLength, 0.f, /*bStopAllMontages=*/false);

	UE_LOG(LogMALUpperBodyVerify, Display, TEXT("Montage weights: full body %.2f/%.2f, shot %.2f."),
		BaseWeight, ShotBaseWeight, ShotWeight);

	if (BaseWeight <= 0.f || ShotWeight <= 0.f || !ShotAnimInstance->Montage_IsPlaying(ShotMontage)
		|| !ShotAnimInstance->Montage_IsPlaying(FullBodyMontage))
	{
		UE_LOG(LogMALUpperBodyVerify, Error,
			TEXT("A montage is not running on both slots (shot %d, full body %d), so the comparison proved nothing."),
			ShotAnimInstance->Montage_IsPlaying(ShotMontage) ? 1 : 0,
			ShotAnimInstance->Montage_IsPlaying(FullBodyMontage) ? 1 : 0);
		DestroyVerifyWorld(World);
		return 1;
	}

	float MaxLegDifference = 0.f;
	float MaxUpperDifference = 0.f;
	float BaseLegMotion = 0.f;
	FPose FirstBasePose = TickAndSample(BaseMesh);
	TickAndSample(ShotMesh);

	for (int32 Frame = 0; Frame < FrameCount; ++Frame)
	{
		const FPose BasePose = TickAndSample(BaseMesh);
		const FPose ShotPose = TickAndSample(ShotMesh);

		MaxLegDifference = FMath::Max(MaxLegDifference, AngularDistanceDegrees(BasePose.Lower, ShotPose.Lower));
		MaxUpperDifference = FMath::Max(MaxUpperDifference, AngularDistanceDegrees(BasePose.Upper, ShotPose.Upper));
		BaseLegMotion = FMath::Max(BaseLegMotion, AngularDistanceDegrees(FirstBasePose.Lower, BasePose.Lower));
	}

	bool bSplitHolds = true;

	// Guards against a vacuous pass: if the full-body montage never moved the legs there would be
	// nothing for the split to protect, and the leg comparison would succeed on a still pose.
	if (BaseLegMotion < BaseLegMotionDegrees)
	{
		UE_LOG(LogMALUpperBodyVerify, Error,
			TEXT("The full-body montage moved the legs only %.2f deg (minimum %.1f): nothing to compare against."),
			BaseLegMotion, BaseLegMotionDegrees);
		bSplitHolds = false;
	}
	else
	{
		UE_LOG(LogMALUpperBodyVerify, Display, TEXT("The full-body montage moved the legs %.2f deg, so they are animated."),
			BaseLegMotion);
	}

	if (MaxLegDifference > LegToleranceDegrees)
	{
		UE_LOG(LogMALUpperBodyVerify, Error,
			TEXT("Legs differed by %.1f deg between the two characters (limit %.1f): the shot is still driving the lower body."),
			MaxLegDifference, LegToleranceDegrees);
		bSplitHolds = false;
	}
	else
	{
		UE_LOG(LogMALUpperBodyVerify, Display,
			TEXT("Legs differed by at most %.2f deg across %d frames (limit %.1f): the shot left the lower body alone."),
			MaxLegDifference, FrameCount, LegToleranceDegrees);
	}

	if (MaxUpperDifference < UpperBodyMinimumDegrees)
	{
		UE_LOG(LogMALUpperBodyVerify, Error,
			TEXT("Upper body differed by only %.1f deg (minimum %.1f): the shot never reached the upper body slot."),
			MaxUpperDifference, UpperBodyMinimumDegrees);
		bSplitHolds = false;
	}
	else
	{
		UE_LOG(LogMALUpperBodyVerify, Display,
			TEXT("Upper body differed by up to %.2f deg while the legs matched: the slot drives the spine only."),
			MaxUpperDifference);
	}

	UE_LOG(LogMALUpperBodyVerify, Display,
		TEXT("Reading: legs %.2f deg (max allowed %.1f), upper body %.2f deg (min required %.1f), base leg motion %.2f deg."),
		MaxLegDifference, LegToleranceDegrees, MaxUpperDifference, UpperBodyMinimumDegrees, BaseLegMotion);

	UE_CLOG(bSplitHolds, LogMALUpperBodyVerify, Display,
		TEXT("Upper/lower body split verified: the shot animates the spine and leaves the legs to the base pose."));

	// The bow layer only swaps to the aim set while the stance is on, so the flag it branches on has
	// to actually arrive on every animation instance - including linked ones.
	UMALAnimInstance* ShotMAL = Cast<UMALAnimInstance>(ShotAnimInstance);
	bool bAimingFlagHolds = ShotMAL != nullptr;

	if (ShotMAL)
	{
		ShotCharacter->SetAiming(true);
		TickAndSample(ShotMesh);
		const bool bAimingWhileStance = ShotMAL->bIsAiming;

		ShotCharacter->SetAiming(false);
		TickAndSample(ShotMesh);
		const bool bAimingAfterStance = ShotMAL->bIsAiming;

		UE_LOG(LogMALUpperBodyVerify, Display, TEXT("Aiming flag follows the stance: %d while aiming, %d after."),
			bAimingWhileStance ? 1 : 0, bAimingAfterStance ? 1 : 0);

		if (!bAimingWhileStance || bAimingAfterStance)
		{
			UE_LOG(LogMALUpperBodyVerify, Error,
				TEXT("bIsAiming did not follow SetAiming, so the bow layer would branch on a stale stance."));
			bAimingFlagHolds = false;
		}
	}
	else
	{
		UE_LOG(LogMALUpperBodyVerify, Error, TEXT("The character is not animating with UMALAnimInstance, so bIsAiming cannot be read."));
	}

	bSplitHolds &= bAimingFlagHolds;

	DestroyVerifyWorld(World);

	return bSplitHolds ? 0 : 1;
}

#else

UMALUpperBodyVerifyCommandlet::UMALUpperBodyVerifyCommandlet()
{
	IsEditor = false;
}

int32 UMALUpperBodyVerifyCommandlet::Main(const FString& Params)
{
	return 0;
}

#endif // WITH_EDITOR
