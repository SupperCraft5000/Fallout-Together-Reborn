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

    // Clearing the life state alone leaves the player downed for the rest of the game: no jumping, no AP or ammo display.
    Resurrect(false, false);

    // Health alone doesn't end the essential down state in Fallout 4
    actorState.ResetLifeState();

    // Stay where the player went down: the placement of an exterior cell can be below the ground, which dropped players
    // through the world, and in an interior it means walking back from the entrance. The move itself is still needed,
    // without it the animation graph and character controller stay in the downed state (tilted pose, no jumping).
    NiPoint3 pos = position;
    pos.z += 16.f;
    if (TESObjectCELL* pCell = GetParentCell())
        MoveTo(pCell, pos);

    // Make bleedout state unrecoverable again for when the player goes down the next time
    SetNoBleedoutRecovery(true);

    return pos;
}
