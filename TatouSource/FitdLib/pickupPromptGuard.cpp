///////////////////////////////////////////////////////////////////////////////
// Pickup prompt suppression for dropped/foundable world objects.
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "pickupPromptGuard.h"

#include <unordered_map>

namespace
{
constexpr u32 kPromptCooldownTicks = 300; // Original behavior: about 5 seconds.

struct PickupPromptState
{
    bool requireSeparation = false;
    bool cooldownActive = false;
    u32 cooldownStartedAt = 0;
};

std::unordered_map<int, PickupPromptState> g_pickupPromptStates;

bool updateCooldown(PickupPromptState& state)
{
    if (!state.cooldownActive)
        return false;

    // Unsigned subtraction intentionally handles the engine timer wrapping.
    if (timer - state.cooldownStartedAt < kPromptCooldownTicks)
        return true;

    state.cooldownActive = false;
    return false;
}

void eraseIfInactive(int objectId, const PickupPromptState& state)
{
    if (!state.requireSeparation && !state.cooldownActive)
        g_pickupPromptStates.erase(objectId);
}
}

void pickupPromptGuardReset()
{
    g_pickupPromptStates.clear();
}

void pickupPromptGuardOnDrop(int objectId)
{
    if (objectId < 0)
        return;

    PickupPromptState& state = g_pickupPromptStates[objectId];
    state.requireSeparation = true;
    state.cooldownActive = false;
}

void pickupPromptGuardOnSaveLoaded(int playerActorId)
{
    if (playerActorId < 0 || playerActorId >= NUM_MAX_OBJECT)
        return;

    const tObject& player = ListObjets[playerActorId];
    if (player.indexInWorld < 0)
        return;

    for (int actorId = 0; actorId < NUM_MAX_OBJECT; ++actorId)
    {
        if (actorId == playerActorId)
            continue;

        const tObject& actor = ListObjets[actorId];
        if (actor.indexInWorld < 0 || !(actor.objectType & AF_FOUNDABLE))
            continue;

        ZVStruct playerZv = player.zv;
        if (player.room != actor.room)
            AdjustZV(&playerZv, player.room, actor.room);

        ZVStruct actorZv = actor.zv;
        if (CubeIntersect(&playerZv, &actorZv))
            pickupPromptGuardOnDrop(actor.indexInWorld);
    }
}

bool pickupPromptGuardShouldSuppressCollision(int objectId, bool intersects)
{
    const auto found = g_pickupPromptStates.find(objectId);
    if (found == g_pickupPromptStates.end())
        return false;

    PickupPromptState& state = found->second;

    if (state.requireSeparation)
    {
        if (intersects)
            return true;

        state.requireSeparation = false;
    }

    const bool coolingDown = updateCooldown(state);
    const bool suppress = intersects && coolingDown;
    eraseIfInactive(objectId, state);
    return suppress;
}

bool pickupPromptGuardIsCoolingDown(int objectId)
{
    const auto found = g_pickupPromptStates.find(objectId);
    if (found == g_pickupPromptStates.end())
        return false;

    PickupPromptState& state = found->second;
    const bool coolingDown = updateCooldown(state);
    eraseIfInactive(objectId, state);
    return coolingDown;
}

void pickupPromptGuardOnPromptDeclined(int objectId, bool requireSeparation)
{
    if (objectId < 0)
        return;

    PickupPromptState& state = g_pickupPromptStates[objectId];
    state.requireSeparation = requireSeparation;
    state.cooldownActive = true;
    state.cooldownStartedAt = timer;
}

void pickupPromptGuardOnObjectTaken(int objectId)
{
    g_pickupPromptStates.erase(objectId);
}
