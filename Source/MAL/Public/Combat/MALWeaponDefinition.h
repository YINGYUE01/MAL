// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MALWeaponDefinition.generated.h"

class UStaticMesh;
class USkeletalMesh;
class UAnimInstance;
class UNiagaraSystem;
class UAnimMontage;

/**
 * UMALWeaponDefinition
 *
 * Everything that changes when a character swaps to a different weapon: the mesh and how it
 * sits in the hand, the size of the hit volume, and how hard its swings hit.
 *
 * Why the definition also talks about a built-in weapon: the character meshes in this project
 * carry their own blade (the Paragon Kwang mesh holds it as the M05_Weapon material section,
 * rigidly skinned to the weapon_r bone). A definition that brings a mesh of its own therefore
 * has to ask for that section to be hidden, or the character renders holding two weapons.
 * Skins - same blade, different material - leave the flag off and change nothing but looks.
 */
UCLASS(BlueprintType)
class MAL_API UMALWeaponDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Shown in UI and in logs. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Weapon")
	FText DisplayName;

	// --- Visuals ---

	/**
	 * Mesh drawn in the character's hand. Null means the definition changes looks or numbers
	 * only, and the character keeps whatever blade its own mesh carries.
	 *
	 * Fills WeaponSkeletalMesh's slot instead when the weapon ships a rigged mesh; set one of
	 * the two, not both.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Weapon|Visual")
	TObjectPtr<UStaticMesh> WeaponMesh;

	/**
	 * Rigged weapon mesh, for packs that ship one instead of a static mesh.
	 *
	 * Takes precedence over WeaponMesh when both are set. Shares the socket and offset below,
	 * so a static and a skeletal version of the same weapon need identical placement values.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Weapon|Visual")
	TObjectPtr<USkeletalMesh> WeaponSkeletalMesh;

	/**
	 * Animation for WeaponSkeletalMesh. Empty leaves the mesh in its reference pose, which is
	 * what a weapon held in a hand wants: the swing comes from the character.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Weapon|Visual")
	TSubclassOf<UAnimInstance> WeaponAnimClass;

	/**
	 * Socket on the character mesh the weapon attaches to.
	 * weapon_rSocket sits on the weapon_r bone and is the hand-authored attach point; plain
	 * weapon_r works too but leaves no room to nudge the weapon without touching the skeleton.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Weapon|Visual")
	FName AttachSocketName = TEXT("weapon_rSocket");

	/** Offset from the socket. Tuned per weapon so the grip lands in the palm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Weapon|Visual")
	FVector MeshRelativeLocation = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Weapon|Visual")
	FRotator MeshRelativeRotation = FRotator::ZeroRotator;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Weapon|Visual")
	FVector MeshRelativeScale = FVector::OneVector;

	/** Material slot of the character mesh that holds its own weapon. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Weapon|Visual")
	FName BuiltInWeaponMaterialSlot = TEXT("M05_Weapon");

	/**
	 * Hide the character's own blade while this weapon is equipped.
	 *
	 * On for any definition that brings a WeaponMesh; off for skins, which want the built-in
	 * blade visible because they only replace its material.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Weapon|Visual")
	bool bHideBuiltInWeapon = true;

	/** Optional trail attached while this weapon is equipped. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Weapon|Visual")
	TObjectPtr<UNiagaraSystem> TrailEffect;

	/** Socket on the weapon mesh the trail starts at. Empty spawns it at the mesh origin. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Weapon|Visual")
	FName TrailSocketName;

	// --- Hit volume ---

	/**
	 * Half-size of the box that sweeps targets during a swing.
	 *
	 * The box is the primitive named UMALWeaponComponent::HitBoxComponentName on the character,
	 * not the weapon mesh: a box gives a predictable blade length where the mesh's own collision
	 * would be as detailed as its geometry.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Weapon|Hit", meta = (ClampMin = "1.0"))
	FVector HitBoxExtent = FVector(32.0f, 32.0f, 32.0f);

	/** Box centre relative to the weapon socket, so the volume covers the blade, not the hilt. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Weapon|Hit")
	FVector HitBoxRelativeLocation = FVector(0.0f, -28.0f, -2.0f);

	// --- Damage ---

	/**
	 * Scales the ability's BaseDamage.
	 *
	 * Kept here rather than on the ability so a greatsword and a dagger can share one swing
	 * ability asset: the animation differs, the hit volume and the damage do too, but the code
	 * path that resolves a hit stays the same.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Weapon|Damage", meta = (ClampMin = "0.0"))
	float DamageMultiplier = 1.0f;

	// --- Animation ---

	/**
	 * Montage this weapon swings with, replacing the melee ability's own.
	 *
	 * Sections have to match the ones the ability enters at (PrimaryAttack_A, _B, ...): the
	 * ability asks whatever montage it is given for a section by name. Empty keeps the ability's
	 * montage, which is right for a reskin of the same blade.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Weapon|Animation")
	TObjectPtr<UAnimMontage> AttackMontage;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
