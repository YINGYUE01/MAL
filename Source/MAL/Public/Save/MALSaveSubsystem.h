// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MALSaveSubsystem.generated.h"

class AMALPlayerState;

/**
 * UMALSaveSubsystem
 *
 * Reads and writes the single player profile.
 *
 * A game instance subsystem rather than a component: the profile outlives the map and the
 * PlayerState, and both ends of the load -- the authority writing the attributes and the pawn
 * that owns them -- can reach it through the world without either holding a pointer to the
 * other.
 *
 * Server side only in practice. Loading writes attributes and grants abilities, and both of
 * those are authority-only; a client calling in would be overwritten by the next replication.
 */
UCLASS()
class MAL_API UMALSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Convenience lookup; null outside a game instance, which is also when saving is pointless. */
	static UMALSaveSubsystem* Get(const UObject* Context);

	/**
	 * Write the current profile of PlayerState to its slot.
	 *
	 * Returns false when there is nothing to write from (no progression set) or the disk write
	 * failed. Callers treat this as best effort: a failed autosave must not interrupt gameplay.
	 */
	bool SaveForPlayer(AMALPlayerState* PlayerState);

	/**
	 * Read the profile into PlayerState, if a slot exists.
	 *
	 * Writes the replicated attributes and the unlocked skill set only. Putting the spent points
	 * back on the primaries is left to the caller, which is the one that knows whether the
	 * baseline has already been written -- see AMALCharacterBase::ApplyDefaultAttributes.
	 *
	 * Returns false when no save exists, which is the normal first-run case and not an error.
	 */
	bool LoadForPlayer(AMALPlayerState* PlayerState);

	/** Remove the profile. The next load then behaves like a first run. */
	void DeleteSave();

	bool HasSave() const;

private:
	/** Single slot: one character, one profile. */
	static const TCHAR* SlotName;

	/** Local user the slot belongs to. */
	static constexpr int32 UserIndex = 0;
};
