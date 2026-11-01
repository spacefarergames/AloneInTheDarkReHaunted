///////////////////////////////////////////////////////////////////////////////
// Pickup prompt suppression for dropped/foundable world objects.
///////////////////////////////////////////////////////////////////////////////

#ifndef PICKUP_PROMPT_GUARD_H
#define PICKUP_PROMPT_GUARD_H

// Clear transient guard state when a world is loaded or restarted.
void pickupPromptGuardReset();

// A freshly dropped object must not immediately reopen its pickup prompt while
// the player is still inside the pickup volume inherited from the drop point.
void pickupPromptGuardOnDrop(int objectId);

// Save files deliberately retain their original format. Reconstruct a safe
// until-separation state for foundable actors already overlapping the player
// after the saved actor table has been restored.
void pickupPromptGuardOnSaveLoaded(int playerActorId);

// Return true when a foundable actor should be omitted from the player's
// collision results. Calling this with intersects == false records that the
// player has completely separated from the actor's pickup volume.
bool pickupPromptGuardShouldSuppressCollision(int objectId, bool intersects);

// Prompt cooldown is deliberately stored outside tWorldObject::trackNumber:
// that field is signed 16-bit game data and is also used by TRACK scripts.
bool pickupPromptGuardIsCoolingDown(int objectId);
void pickupPromptGuardOnPromptDeclined(int objectId, bool requireSeparation);
void pickupPromptGuardOnObjectTaken(int objectId);

#endif
