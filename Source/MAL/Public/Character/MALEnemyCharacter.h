// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/MALCharacterBase.h"
#include "MALEnemyCharacter.generated.h"

class UMALWorldWidgetComponent;
class UMALEnemyDefinition;
class UBoxComponent;

/**
 * AMALEnemyCharacter
 *
 * Enemy character. Unlike the player, it has no PlayerState-owned ASC; the ASC and
 * AttributeSet are default subobjects on this character. AMALCharacterBase caches them
 * and initializes actor info with (this, this).
 */
UCLASS()
class MAL_API AMALEnemyCharacter : public AMALCharacterBase
{
	GENERATED_BODY()

public:
	AMALEnemyCharacter();

	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;

	/**
	 * Look, body and swing of this enemy in one asset.
	 *
	 * Optional: left empty the pawn behaves exactly as its blueprint authored it, which is what
	 * keeps the existing enemies working while a replacement is being chosen.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Enemy")
	TObjectPtr<UMALEnemyDefinition> Definition;

	/** World-space health bar shown above the enemy. Set its WidgetClass on the blueprint. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MAL|UI")
	TObjectPtr<UMALWorldWidgetComponent> HealthBarWidget;

	// --- Hit volume defaults ---

	/**
	 * Hit volume used when no Definition is assigned.
	 *
	 * UMALWeaponComponent looks the volume up by name, so an enemy authored without one swings
	 * through its target and never damages it. Creating it here keeps every enemy able to hit
	 * even while its art and definition are still being chosen; a Definition overrides these
	 * values when it is present.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Enemy|Combat")
	bool bAutoCreateHitBox = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Enemy|Combat", meta = (EditCondition = "bAutoCreateHitBox"))
	FName DefaultHitBoxComponentName = TEXT("weapon_collision");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Enemy|Combat", meta = (EditCondition = "bAutoCreateHitBox"))
	FName DefaultHitBoxAttachSocket = TEXT("weapon_r");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Enemy|Combat", meta = (EditCondition = "bAutoCreateHitBox"))
	FVector DefaultHitBoxExtent = FVector(120.0f, 80.0f, 120.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Enemy|Combat", meta = (EditCondition = "bAutoCreateHitBox"))
	FVector DefaultHitBoxRelativeLocation = FVector::ZeroVector;
	
	/**
	 * Smallest the hit volume is allowed to be, whatever the definition asks for.
	 *
	 * The AI stops at its attack range (about two metres) and swings from there; a volume that
	 * only covers the hand cannot reach a target at that distance, so the swing looks like a
	 * hit that does nothing. This is the floor that keeps reach and the AI's stop distance
	 * consistent without having to tune both by hand.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Enemy|Combat", meta = (EditCondition = "bAutoCreateHitBox"))
	FVector MinHitBoxExtent = FVector(220.0f, 220.0f, 220.0f);
	
	/**
	 * Extra offset applied when the requested socket does not exist on the mesh.
	 *
	 * A hit volume at the mesh root sits at the enemy's feet: it can never reach a target that
	 * is one body length away, which reads as "the enemy swings but never hurts me". Pushing
	 * the volume forward and up puts it where the weapon actually is.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MAL|Enemy|Combat", meta = (EditCondition = "bAutoCreateHitBox"))
	FVector MissingSocketHitBoxOffset = FVector(110.0f, 0.0f, 60.0f);

protected:
	/** Push the definition's mesh, animation, size and combat settings onto this pawn. */
	void ApplyDefinition();

	/**
	 * The hit volume the weapon component looks up by name, created when the enemy has none.
	 *
	 * It starts with collision disabled: the swing's notify is what enables it for its window,
	 * so an idle enemy never overlaps anything.
	 */
	UBoxComponent* EnsureHitBox();

	/** Adds the definition's attack ability on top of whatever the class kit granted. */
	virtual void GrantAbilities() override;
};
