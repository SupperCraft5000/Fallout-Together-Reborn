#include "PlayerCharacter.h"

#include <Games/References.h>
#include <Games/TES.h>
#include <Forms/TESObjectCELL.h>

int32_t PlayerCharacter::LastUsedCombatSkill = -1;

void PlayerCharacter::SetDifficulty(const int32_t aDifficulty, bool aForceUpdate, bool aExpectGameDataLoaded) noexcept
{
    if (aDifficulty > 5 || aDifficulty < 0)
        return;

    TP_THIS_FUNCTION(TSetDifficulty, void, PlayerCharacter, const int32_t aDifficulty, bool aForceUpdate, bool aExpectGameDataLoaded);
    POINTER_FALLOUT4(TSetDifficulty, setDifficulty, 657344);
    TiltedPhoques::ThisCall(setDifficulty, this, aDifficulty, aForceUpdate, aExpectGameDataLoaded);
}

void PlayerCharacter::PayCrimeGoldToAllFactions() noexcept
{
    // TODO: fallout 4 doesn't have "bounties".
    // Still, aggro should be removed somehow on respawn.
}

// TODO: ft (verify)
NiPoint3 PlayerCharacter::RespawnPlayer() noexcept
{
    // Make bleedout state recoverable
    SetNoBleedoutRecovery(false);

    DispelAllSpells();

    // Reset health to max
    // TODO(cosideci): there's a cleaner way to do this
    ForceActorValue(ActorValueOwner::ForceMode::DAMAGE, ActorValueInfo::kHealth, 1000000);

    // Health alone doesn't end the essential down state in Fallout 4
    actorState.ResetLifeState();

    // Stay where the player went down: the placement of an exterior cell can be below the ground, which dropped players
    // through the world, and in an interior it means walking back from the entrance.

    // Make bleedout state unrecoverable again for when the player goes down the next time
    SetNoBleedoutRecovery(true);

    return position;
}
