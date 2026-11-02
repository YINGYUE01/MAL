// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "MALAnimNotifyState_SpawnWeaponFX.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class USkeletalMeshComponent;

/** One live effect, keyed by the mesh that is playing the montage. */
USTRUCT()
struct FMALWeaponFXInstance
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TWeakObjectPtr<USkeletalMeshComponent> Mesh;

	UPROPERTY(Transient)
	TWeakObjectPtr<UNiagaraComponent> Effect;
};

/**
 * UMALAnimNotifyState_SpawnWeaponFX
 *
 * Duration version of the weapon FX notify: the Niagara system is spawned on NotifyBegin and
 * destroyed on NotifyEnd, so an effect can run for exactly the span authored in the timeline
 * (a charged blade glow, a sustained muzzle flare) instead of a single frame.
 *
 * Attach target resolution, in order:
 *   1. AttachmentComponentName, when set
 *   2. the character's visible weapon visual (skeletal first, then static)
 *   3. the animating character mesh, when bFallbackToCharacterMesh is true
 *
 * Notify objects are shared by every actor playing the same montage, so the spawned component
 * is tracked per playing mesh; two enemies swinging the same animation get two effects.
 */
UCLASS(meta = (DisplayName = "MAL Weapon FX State"))
class MAL_API UMALAnimNotifyState_SpawnWeaponFX : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName_Implementation() const override;

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	/** Niagara system to spawn. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MAL|Weapon FX")
	TObjectPtr<UNiagaraSystem> SystemTemplate;

	/** Socket on the weapon mesh (e.g. the spear tip or blade edge). None attaches to the mesh root. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MAL|Weapon FX")
	FName SocketName = NAME_None;

	/** Optional explicit component name; empty reads the character's equipped weapon visual. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MAL|Weapon FX")
	FName AttachmentComponentName = NAME_None;

	/** Prefer the skeletal weapon visual when the character has both kinds of weapon mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MAL|Weapon FX")
	bool bPreferSkeletalWeapon = true;

	/** Attach to the animating character mesh when no weapon visual is equipped. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MAL|Weapon FX")
	bool bFallbackToCharacterMesh = true;

	/**
	 * Attach to the mesh root and copy only the socket's transform.
	 *
	 * A socket inherits its bone's orientation, which tilts every user parameter authored in
	 * mesh space away from the blade. Turn this on for effects designed in weapon-mesh space.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MAL|Weapon FX")
	bool bUseSocketTransformOnly = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MAL|Weapon FX")
	FVector LocationOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MAL|Weapon FX")
	FRotator RotationOffset = FRotator::ZeroRotator;

	/** Uniform scale applied to the spawned component; 1 keeps the asset default. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MAL|Weapon FX", meta = (ClampMin = "0.01"))
	float Scale = 1.0f;

	/** Destroy the effect on NotifyEnd. Off leaves it running until the system finishes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MAL|Weapon FX")
	bool bDestroyOnEnd = true;

private:
	/** Live effects, one per mesh currently inside this state. */
	UPROPERTY(Transient)
	TArray<FMALWeaponFXInstance> ActiveEffects;
};
