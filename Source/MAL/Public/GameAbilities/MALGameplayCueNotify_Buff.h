// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Actor.h"
#include "MALGameplayCueNotify_Buff.generated.h"

class UNiagaraSystem;
class UNiagaraComponent;

/**
 * AMALGameplayCueNotify_Buff
 *
 * Generic "persistent buff effect" cue: OnActive spawns a Niagara system on the target's skeletal
 * mesh and attaches it to a socket, OnRemove destroys the component immediately.
 *
 * Its lifetime belongs entirely to the GameplayEffect. Expiry, dispels and the target's death all
 * reach OnRemove, so the visual can never outlive the buff by a frame, and no ability-side timer
 * is needed to switch it off.
 *
 * Blueprint subclasses placed under the GameplayCueNotifyPaths (here: /Game) are picked up by the
 * cue manager; set AuraEffect and the socket on the subclass.
 */
UCLASS()
class MAL_API AMALGameplayCueNotify_Buff : public AGameplayCueNotify_Actor
{
	GENERATED_BODY()

public:
	AMALGameplayCueNotify_Buff();

	virtual bool OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
	virtual bool OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;

protected:
	/** Niagara system spawned on the target while the effect is active. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Cue")
	TObjectPtr<UNiagaraSystem> AuraEffect;

	/** Socket on the target's skeletal mesh. Empty attaches to the mesh origin. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Cue")
	FName AttachSocketName = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Cue")
	FVector AttachLocationOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Cue")
	FRotator AttachRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Cue")
	FVector AttachScale = FVector(1.f);

	/** The spawned aura, destroyed when the effect stops. */
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> ActiveAura;
};
