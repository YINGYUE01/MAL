// Copyright Epic Games, Inc., All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"
#include "InputActionValue.h"
#include "MALAbilitySystemComponent.generated.h"

class UMALAbilitySet;
class UGameplayAbility;

UCLASS()
class MAL_API UMALAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UMALAbilitySystemComponent();

	/**
	 * Source key the equipped weapon files its abilities under.
	 *
	 * One key for every weapon rather than one per definition: a character holds one weapon at a
	 * time, so a swap clears this key and re-grants into it. Everything the character owns itself
	 * -- class sets, bought skills -- stays under NAME_None and is untouched by a swap.
	 */
	static const FName WeaponAbilitySourceKey;

	virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) override;

	/**
	 * Allocate a FMALGameplayEffectContext instead of the engine default so damage
	 * executions can publish crit / damage-channel info to presentation code.
	 */
	virtual FGameplayEffectContextHandle MakeEffectContext() const override;

	/**
	 * Grant all abilities from an AbilitySet, storing the InputTag in DynamicAbilityTags.
	 * Returns the granted spec handles so the caller can remove them later.
	 *
	 * SourceKey names who is handing the abilities out. ClearGrantedAbilities works one key at a
	 * time, so a caller with a key of its own -- an equipped weapon, say -- can take its abilities
	 * back without disturbing the character's kit.
	 *
	 * Server only.
	 */
	TArray<FGameplayAbilitySpecHandle> GrantAbilitySet(
		const UMALAbilitySet* AbilitySet,
		FName SourceKey = NAME_None);

	/**
	 * Grant one ability, tagging its spec with InputTag and filing the handle under SourceKey.
	 *
	 * The single-skill path behind both GrantAbilitySet and the skill tree. Sharing it is what
	 * keeps the handle registry complete: ClearGrantedAbilities only clears what was recorded
	 * here, so a skill granted around this function would survive a re-grant and stack up.
	 *
	 * Server only; returns an invalid handle when the class is null or this is not the authority.
	 * An invalid InputTag grants the ability with no input binding.
	 */
	FGameplayAbilitySpecHandle GrantAbility(
		TSubclassOf<UGameplayAbility> AbilityClass,
		FGameplayTag InputTag = FGameplayTag(),
		int32 Level = 1,
		FName SourceKey = NAME_None);

	/**
	 * Remove every ability filed under SourceKey. Server only.
	 *
	 * Scoped to one source rather than "clear everything" on purpose. The character rebuilds its
	 * whole kit after every skill purchase and every save load; against a flat list, that rebuild
	 * also deleted whatever an equipped weapon had granted, and GrantAbilities -- which knows
	 * nothing about weapons -- had no way to hand it back. The abilities vanished silently.
	 *
	 * NAME_None is the character's own kit: the class ability sets and the bought skill nodes.
	 */
	void ClearGrantedAbilities(FName SourceKey = NAME_None);

	/** Upgrade an ability by its InputTag (Server only) */
	bool UpgradeAbilityByInputTag(const FGameplayTag& InputTag);

	/** Buffer an input press event by tag (called from PC HandleInputActionEvent) */
	void AbilityInputTagPressed(const FGameplayTag& InputTag, const FInputActionValue& Value);

	/** Buffer an input release event by tag (called from PC HandleInputActionEvent) */
	void AbilityInputTagReleased(const FGameplayTag& InputTag, const FInputActionValue& Value);

	/** Last input value seen for a tag (zeroed FInputActionValue when none was recorded). */
	FInputActionValue GetLastInputValue(const FGameplayTag& InputTag) const;

	/** Process all buffered input (called from PC PostProcessInput) */
	void ProcessAbilityInput(float DeltaTime, bool bGamePaused);

	/** Clear all buffered input */
	void ClearAbilityInput();

protected:
	/**
	 * Records the input value and appends the handles it matches to OutHandles. Shared by the
	 * press and release entry points, which differ only in which buffer they fill.
	 */
	void BufferInputByTag(const FGameplayTag& InputTag, const FInputActionValue& Value, TArray<FGameplayAbilitySpecHandle>& OutHandles);

	/** Buffered handles for input pressed this frame */
	TArray<FGameplayAbilitySpecHandle> InputPressedSpecHandles;

	/** Buffered handles for input released this frame */
	TArray<FGameplayAbilitySpecHandle> InputReleasedSpecHandles;

	/** Buffered handles for input held (active between press and release) */
	TArray<FGameplayAbilitySpecHandle> InputHeldSpecHandles;

	/** Last value per input tag, used by UMALGameplayAbility::InputPressed/Released. */
	TMap<FGameplayTag, FInputActionValue> LastInputValues;

	/**
	 * Handles this component granted, filed by who asked for them.
	 *
	 * Grouped rather than flat because the two kinds of caller re-grant on unrelated schedules:
	 * buying a skill rebuilds the character's kit and must leave a weapon's abilities alone.
	 */
	TMap<FName, TArray<FGameplayAbilitySpecHandle>> GrantedAbilityHandles;

	/** Override to replicate direct input for abilities with bReplicateInputDirectly. */
	virtual void AbilitySpecInputPressed(FGameplayAbilitySpec& Spec) override;

	/** Override to replicate direct input for abilities with bReplicateInputDirectly. */
	virtual void AbilitySpecInputReleased(FGameplayAbilitySpec& Spec) override;

	/** Find a spec by its InputTag in DynamicAbilityTags */
	FGameplayAbilitySpec* FindAbilitySpecByInputTag(const FGameplayTag& InputTag);
};
