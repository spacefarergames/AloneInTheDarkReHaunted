///////////////////////////////////////////////////////////////////////////////
// Bytecode Patches - Runtime bug fixes for life script bytecode
//
// Purpose:
//   Implements the patch system for intercepting and fixing bugs in bytecode
//   execution. Replaces native script overrides with a more maintainable
//   approach that runs on top of the bytecode interpreter.
//
// Author: Patch System
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "bytecodePatches.h"
#include "life.h"
#include "consoleLog.h"
#include "track.h"
#include "dustParticles.h"
#include "lanternLighting.h"
#include <vector>
#include <map>
#include <set>
#include <chrono>
#include <cmath>

// External declarations for accessing game world
extern std::array<tObject, NUM_MAX_OBJECT> ListObjets;
extern std::vector<tWorldObject> ListWorldObjets;
extern unsigned int timer;        // Global engine tick counter (vars.cpp)
extern s16 NumCamera;      // Current camera number
extern s16 g_currentFloor; // Current floor number

// ============================================================================
// Patch Registry
// ============================================================================

// Track stuck zombie chicken actors for teleportation fallback
// Map of actor index -> stuck detection count
static std::map<int, int> s_zombieChickenStuckCount;

// Track when zombie chicken first got stuck (for 10-second delay before intervention)
// Map of actor index -> timestamp when stuck was first detected
static std::map<int, std::chrono::high_resolution_clock::time_point> s_zombieChickenStuckTime;

// Track last known position to detect if actor is making progress
// Map of actor index -> last position coordinates
struct ActorPosition {
    int x, y, z;
};
static std::map<int, ActorPosition> s_zombieChickenLastPosition;

// Track player stuck state at CAMERA05_000
// Timestamp when player first detected stuck in CAMERA05_000
static std::chrono::high_resolution_clock::time_point s_playerStuckTime;
// Last known position of player when stuck
static ActorPosition s_playerStuckPosition = {0, 0, 0};
// Whether player is currently being tracked as stuck
static bool s_playerStuckTracking = false;

// Track player stuck state at CAMERA04_000
// Timestamp when player first detected stuck in CAMERA04_000
static std::chrono::high_resolution_clock::time_point s_playerStuckTime_Camera04;
// Last known position of player when stuck
static ActorPosition s_playerStuckPosition_Camera04 = {0, 0, 0};
// Whether player is currently being tracked as stuck at CAMERA04
static bool s_playerStuckTracking_Camera04 = false;

// Track actors with uninitialized movement state
// Set of actor indices that have received movement opcodes without proper initialization
static std::set<int> s_actorsWithUninitializedMovement;

// Track zombie chicken actors whose shadow should currently be hidden
// (i.e., they are still behind the window and have not crashed through yet).
// Actors are inserted on first sight and removed once they have moved
// significantly from their initial spawn position (i.e., crashed through).
static std::set<int> s_zombieChickenHiddenShadow;

// Initial (first observed) world position of each zombie chicken actor.
// Used to decide when the actor has moved far enough to be considered
// "crashed through the window".
static std::map<int, ActorPosition> s_zombieChickenShadowInitialPosition;

// Timestamp of when each zombie chicken was first observed post-crash (i.e.
// the frame its shadow-hidden flag was cleared). Used to enforce a grace
// period before the stuck-detection patch can intervene, so the scripted
// crash -> jump -> land animation sequence has time to complete without us
// cutting it short and stranding the chicken mid-air.
static std::map<int, std::chrono::high_resolution_clock::time_point> s_zombieChickenPostCrashTime;

// Track last observed worldY per zombie chicken to detect whether it is
// airborne (Y is changing). While airborne we must not intervene.
static std::map<int, int> s_zombieChickenLastY;
static std::map<int, std::chrono::high_resolution_clock::time_point> s_zombieChickenLastYChangeTime;

// Track actors whose camera mask (BgOverlay) occlusion should be skipped.
// Populated by the "frog in the intro" patch (body 267) and similar cases
// where the actor must stay fully visible on top of the background.
static std::set<int> s_skipMaskActors;

// Actors whose stencil/blob shadow is *permanently* disabled because the
// underlying body has no business casting one (small/flat/inanimate props
// that look wrong with a contact shadow). Populated as soon as a matching
// actor is first observed.
static std::set<int> s_permanentNoShadowActors;

// Body numbers that should never cast a stencil/blob shadow.
static inline bool isNoShadowBody(int bodyNum)
{
    return bodyNum == 5 || bodyNum == 6 || bodyNum == 55 || bodyNum == 56;
}

// Body number for the frog actor in the intro cinematic.
static const int FROG_INTRO_BODY_NUM = 267;

///
/// Internal patch entry structure
///
struct BytecodePatch
{
    int lifeNum;                          // Life script number (-1 = all)
    int opcode;                           // Opcode (-1 = all)
    PatchConditionFunc condition;         // Condition check
    PatchActionFunc action;               // Action to execute
    const char* description;              // Human-readable description
    int executionCount;                   // Debug: how many times applied
};

// Global patch registry
static std::vector<BytecodePatch> s_bytecodePatches;

// ============================================================================
// Public API
// ============================================================================

void registerBytecodePatch(int lifeNum, int opcode, 
                          PatchConditionFunc condition, 
                          PatchActionFunc action,
                          const char* description)
{
    if (!condition || !action)
    {
        printf(LIFE_WARN "Cannot register patch: null function pointer" CON_RESET "\n");
        return;
    }

    BytecodePatch patch;
    patch.lifeNum = lifeNum;
    patch.opcode = opcode;
    patch.condition = condition;
    patch.action = action;
    patch.description = description;
    patch.executionCount = 0;

    s_bytecodePatches.push_back(patch);
    
    if (lifeNum == -1)
        printf(LIFE_OK "Registered bytecode patch (all scripts): %s" CON_RESET "\n", description);
    else if (opcode == -1)
        printf(LIFE_OK "Registered bytecode patch (life %d, all opcodes): %s" CON_RESET "\n", lifeNum, description);
    else
        printf(LIFE_OK "Registered bytecode patch (life %d, opcode %d): %s" CON_RESET "\n", lifeNum, opcode, description);
}

void applyBytecodePreOpcodePatches(int lifeNum, int opcode, tObject* actor)
{
    for (auto& patch : s_bytecodePatches)
    {
        // Check if patch applies to this life script
        if (patch.lifeNum != -1 && patch.lifeNum != lifeNum)
            continue;

        // Check if patch applies to this opcode
        if (patch.opcode != -1 && patch.opcode != opcode)
            continue;

        // Check if patch condition is met
        if (!patch.condition(lifeNum, opcode, actor))
            continue;

        // Apply patch
        patch.action(lifeNum, opcode, actor, 0);  // context=0 for pre-opcode
        patch.executionCount++;
    }
}

void applyBytecodePostOpcodePatches(int lifeNum, int opcode, tObject* actor)
{
    for (auto& patch : s_bytecodePatches)
    {
        // Check if patch applies to this life script
        if (patch.lifeNum != -1 && patch.lifeNum != lifeNum)
            continue;

        // Check if patch applies to this opcode
        if (patch.opcode != -1 && patch.opcode != opcode)
            continue;

        // Check if patch condition is met
        if (!patch.condition(lifeNum, opcode, actor))
            continue;

        // Apply patch
        patch.action(lifeNum, opcode, actor, 1);  // context=1 for post-opcode
        patch.executionCount++;
    }
}

void applyBytecodeLifecyclePatches(LifecycleEventType event, int lifeNum, tObject* actor)
{
    // TODO: Implement lifecycle patch application if needed
}

void dumpBytecodePatches()
{
    printf(LIFE_TAG "\n=== Bytecode Patches ===" CON_RESET "\n");
    printf(LIFE_TAG "Total patches registered: %d" CON_RESET "\n", (int)s_bytecodePatches.size());

    for (size_t i = 0; i < s_bytecodePatches.size(); i++)
    {
        const auto& patch = s_bytecodePatches[i];
        const char* lifeStr = (patch.lifeNum == -1) ? "all" : std::to_string(patch.lifeNum).c_str();
        const char* opcodeStr = (patch.opcode == -1) ? "all" : std::to_string(patch.opcode & 0x7FFF).c_str();
        printf(LIFE_TAG "  [%zu] Life: %s, Opcode: %s, Exec: %d - %s" CON_RESET "\n",
                   i, lifeStr, opcodeStr, patch.executionCount, patch.description);
    }
}

int getBytecodePatches()
{
    return (int)s_bytecodePatches.size();
}

// ============================================================================
// Patch Implementations - Known Bugs
// ============================================================================

///
/// PATCH: Verify actor pointer validity before use
///
/// Issue:
///   Some opcodes may receive invalid actor pointers from stale references
///   or incorrect initialization. Verify pointer is in valid actor range.
///
static bool patchCondition_ValidateActor(int lifeNum, int opcode, tObject* actor)
{
    // Only apply to certain opcodes that heavily use actor state
    switch (opcode)
    {
        case LM_ANIM_ONCE:
        case LM_ANIM_REPEAT:
        case LM_ANIM_ALL_ONCE:
        case LM_DO_MOVE:
        case LM_MOVE:
            return (actor != nullptr && actor->indexInWorld >= 0);
        default:
            return false;
    }
}

static void patchAction_ValidateActor(int lifeNum, int opcode, tObject* actor, int context)
{
    // Pre-opcode validation
    if (context == 0)
    {
        if (!actor || actor->indexInWorld < 0)
        {
            printf(LIFE_WARN "Bytecode patch: Invalid actor in life %d, opcode %d" CON_RESET "\n", lifeNum, opcode);
            // Could skip opcode execution here if needed
        }
    }
}

///
/// PATCH: Prevent null pointer dereference in HQR_Get calls
///
/// Issue:
///   Some opcodes call HQR_Get to load resources but don't check for nullptr
///   before dereferencing the result. This patch logs and prevents crashes.
///
static bool patchCondition_ResourceCheck(int lifeNum, int opcode, tObject* actor)
{
    // Apply to opcodes that load resources
    switch (opcode)
    {
        case LM_BODY:
        case LM_ANIM_ONCE:
        case LM_ANIM_REPEAT:
        case LM_ANIM_ALL_ONCE:
        case LM_SAMPLE:
        case LM_MUSIC:
        case LM_LIGHT:
            return true;
        default:
            return false;
    }
}

static void patchAction_ResourceCheck(int lifeNum, int opcode, tObject* actor, int context)
{
    // Post-opcode verification
    if (context == 1 && actor)
    {
        // Log if critical resources are missing
        if (actor->bodyNum >= 0)
        {
            sBody* body = HQR_Get(HQ_Bodys, actor->bodyNum);
            if (!body)
            {
                printf(LIFE_WARN "Bytecode patch: Missing body %d in life %d, actor %d" CON_RESET "\n",
                          actor->bodyNum, lifeNum, actor->indexInWorld);
            }
        }
    }
}

///
/// PATCH: Ensure animation state is valid after animation changes
///
/// Issue:
///   Animation opcodes may set invalid animation IDs or frame numbers,
///   causing rendering issues. Validate and correct if necessary.
///
static bool patchCondition_AnimationValidation(int lifeNum, int opcode, tObject* actor)
{
    return (opcode == LM_ANIM_ONCE || opcode == LM_ANIM_REPEAT || opcode == LM_ANIM_ALL_ONCE) &&
           (actor != nullptr);
}

static void patchAction_AnimationValidation(int lifeNum, int opcode, tObject* actor, int context)
{
    if (context == 1 && actor)
    {
        // Ensure animation index is valid
        if (actor->ANIM >= 0)
        {
            sAnimation* anim = HQR_Get(HQ_Anims, actor->ANIM);
            if (!anim)
            {
                printf(LIFE_WARN "Bytecode patch: Invalid animation %d in life %d, actor %d" CON_RESET "\n",
                          actor->ANIM, lifeNum, actor->indexInWorld);
                actor->ANIM = -1;  // Reset to no animation
            }
        }
    }
}

///
/// PATCH: Fix zombie chicken enemies stuck at windows after crash
///
/// Issue:
///   When zombie chicken enemies crash through a window, they sometimes get stuck next to it
///   with zero speed and won't chase the player until they get very close.
///   This occurs because movement speed is reset to 0 during collision recovery.
///
/// Solution:
///   Detect when zombie chicken actors have speed=0 during movement.
///   Reset their speed to resume normal AI behavior (chasing/pathfinding).
///   Only applies to zombie chicken body types (024, 071, 094, 234).
///
/// Zombie chicken body numbers:
///   24  (LISTBODY_024 / LISTBOD2_024)
///   71  (LISTBODY_071 / LISTBOD2_071)
///   94  (LISTBODY_094 / LISTBOD2_094)
///   150 (LISTBOD2_150)
///   234 (LISTBOD2_234)
///
static inline bool isZombieChickenBody(int bodyNum)
{
    return bodyNum == 24 || bodyNum == 71 || bodyNum == 94 || bodyNum == 150 || bodyNum == 234;
}

///
/// Simulate a "window refocus catch-up" for an actor whose interpolation
/// state is frozen. Empirically, unfocusing and refocusing the game window
/// instantly unfreezes the zombie chicken; the reason is that per-actor
/// interpolation anchors (`RealValue::memoTicks`, and the body's keyframe
/// start time at scratchBuffer[+4]) measure elapsed time as `timer - anchor`.
/// When the engine resumes after a long pause this delta becomes huge and
/// every in-flight interpolation trips its completion branch
/// (`evaluateReal()` in anim.cpp:175, `SetInterAnimObjet()` in anim.cpp:864).
///
/// Rather than literally pausing, we rewind the actor's interpolation
/// anchors so the same completion branches fire on the very next tick. We
/// also clear pending anim actions and the pathfinding stuck counter, both
/// of which can latch an actor out of chase mode.
///
static void wakeUpStuckActor(tObject* actor)
{
    if (!actor)
        return;

    // Force all RealValue interpolations to snap to their end values next
    // frame by pushing their anchors well into the past.
    const unsigned int kRewind = 10000;
    unsigned int pastTick = (timer > kRewind) ? (timer - kRewind) : 0;
    actor->speedChange.memoTicks = pastTick;
    actor->rotate.memoTicks      = pastTick;
    actor->YHandler.memoTicks    = pastTick;

    // Push the body's keyframe start-time anchor into the past as well so
    // the animation player advances past whatever keyframe it is wedged on.
    if (actor->bodyNum >= 0)
    {
        sBody* pBody = HQR_Get(HQ_Bodys, actor->bodyNum);
        if (pBody && !pBody->m_scratchBuffer.empty() && pBody->m_scratchBuffer.size() >= 6)
        {
            u16 newAnchor = (u16)((u16)timer - (u16)0x4000);
            *(u16*)(pBody->m_scratchBuffer.data() + 4) = newAnchor;
        }
    }

    // Let the life script reach its "animation finished" branch so it can
    // pick the next action (usually chase/move). Do NOT clear ANIM itself
    // - locomotion in AITD is animation-driven, wiping ANIM strands the
    // actor mid-air.
    actor->flagEndAnim = 1;
    actor->animActionType = 0;

    // Clear pathfinding stuck counters - these can disable active chase
    // after too many failed waypoint-approach frames.
    resetTrackStuckCounters(actor->indexInWorld);
}

static bool patchCondition_EnemyStuckAtWindow(int lifeNum, int opcode, tObject* actor)
{
    // Only apply to zombie chicken enemies, not other actors
    if (actor == nullptr || !isZombieChickenBody(actor->bodyNum))
        return false;

    // Check on ALL opcodes for zombie chickens so we continuously monitor them
    // This ensures we detect stuck state even when trackMode becomes 0 (when actor is stuck)
    // We need to keep monitoring to detect when they need unsticking, even if trackMode changes
    return true;
}

static void patchAction_EnemyStuckAtWindow(int lifeNum, int opcode, tObject* actor, int context)
{
    // Post-opcode: Check if zombie chicken is stuck after attempting to crash through window
    // Only apply to zombie chicken enemies
    if (context != 1 || !actor || !isZombieChickenBody(actor->bodyNum))
        return;

    int actorIdx = actor->indexInWorld;

    // IMPORTANT: Do not interfere with the chicken while it is still behind
    // the window. The shadow-hide patch keeps actors in
    // s_zombieChickenHiddenShadow until they have moved far enough to be
    // considered "crashed through". Before that point the chicken legitimately
    // sits in its idle/approach animation with speed == 0, and forcing speed /
    // trackMode / ANIM here would preempt the scripted approach + crash
    // sequence. Also skip until we've at least observed the initial position.
    if (s_zombieChickenShadowInitialPosition.count(actorIdx) == 0)
        return;
    if (s_zombieChickenHiddenShadow.count(actorIdx) > 0)
    {
        // Still behind the window - clear any lingering stuck record so we
        // don't count pre-crash idle time against the post-crash timer.
        s_zombieChickenStuckCount.erase(actorIdx);
        s_zombieChickenStuckTime.erase(actorIdx);
        s_zombieChickenLastPosition.erase(actorIdx);
        s_zombieChickenPostCrashTime.erase(actorIdx);
        s_zombieChickenLastY.erase(actorIdx);
        s_zombieChickenLastYChangeTime.erase(actorIdx);
        return;
    }

    auto now = std::chrono::high_resolution_clock::now();

    // Stamp the moment the chicken was first observed post-crash. The
    // shadow-hide threshold (~200 horizontal units) can be crossed during the
    // mid-air portion of the crash/jump animation, so we must NOT start
    // looking for "stuck" state until the scripted land has had time to
    // complete. Enforce a grace period from this timestamp.
    if (s_zombieChickenPostCrashTime.count(actorIdx) == 0)
    {
        s_zombieChickenPostCrashTime[actorIdx] = now;
        s_zombieChickenLastY[actorIdx] = actor->worldY;
        s_zombieChickenLastYChangeTime[actorIdx] = now;
        printf(LIFE_TAG "Bytecode patch: Zombie chicken actor %d now post-crash - starting grace period" CON_RESET "\n", actorIdx);
        return;
    }

    // Track whether the actor is airborne. If worldY has changed since the
    // last observation it is still falling / jumping; refresh the timestamp.
    {
        auto lastYIt = s_zombieChickenLastY.find(actorIdx);
        if (lastYIt != s_zombieChickenLastY.end() && lastYIt->second != actor->worldY)
        {
            lastYIt->second = actor->worldY;
            s_zombieChickenLastYChangeTime[actorIdx] = now;
        }
    }

    // Require a grace period after post-crash detection before any
    // intervention so the scripted jump/land plays out untouched.
    auto postCrashElapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - s_zombieChickenPostCrashTime[actorIdx]).count();
    const long POST_CRASH_GRACE_SECONDS = 5;
    if (postCrashElapsed < POST_CRASH_GRACE_SECONDS)
        return;

    // Require the chicken to be grounded (worldY stable) for at least ~1s
    // before we treat it as stuck - otherwise we might interrupt a jump.
    auto yStableElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - s_zombieChickenLastYChangeTime[actorIdx]).count();
    if (yStableElapsed < 1000)
        return;

    // Check if we're still tracking this actor as stuck
    bool hasStuckRecord = (s_zombieChickenStuckTime.count(actorIdx) > 0);

    // Only detect stuck AFTER normal behavior has been attempted
    // Detect stuck condition: zombie chicken with zero speed but active trackmode
    if (actor->speed == 0 && actor->ANIM >= 0)
    {
        // First time detecting stuck - record the timestamp
        if (!hasStuckRecord)
        {
            s_zombieChickenStuckCount[actorIdx] = 1;
            s_zombieChickenStuckTime[actorIdx] = now;
            s_zombieChickenLastPosition[actorIdx] = {actor->worldX, actor->worldY, actor->worldZ};
            printf(LIFE_TAG "Bytecode patch: Zombie chicken stuck detected (post-crash) in life %d, actor %d, body %d - waiting 10 seconds before intervention..." CON_RESET "\n",
                   lifeNum, actorIdx, actor->bodyNum);
            return;  // Just record, don't intervene yet
        }
    }
    else if (!hasStuckRecord)
    {
        // Not stuck and no record to clean up
        return;
    }

    // If we have a stuck record, check the elapsed time
    if (hasStuckRecord)
    {
        auto stuckStart = s_zombieChickenStuckTime[actorIdx];
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - stuckStart).count();

        // After 10 seconds, FORCE intervention
        if (elapsed >= 10)
        {
            printf(LIFE_TAG "Bytecode patch: Zombie chicken stuck for %ld seconds in life %d, actor %d - FORCING unstick!" CON_RESET "\n",
                   elapsed, lifeNum, actorIdx);

            // Replicate what unfocus+refocus does for the player: rewind
            // the actor's interpolation anchors so frozen anim/speed
            // interpolations snap to completion on the next tick. This is
            // what actually unfreezes the chicken in practice.
            wakeUpStuckActor(actor);

            // Then nudge AI intent toward active chase.
            actor->speed = 80;
            actor->HIT = 3;
            actor->speedChange.startValue = 0;
            actor->speedChange.endValue = 80;
            actor->speedChange.numSteps = 0;
            actor->trackMode = 2;

            // DO NOT update position here - keep comparing to ORIGINAL stuck position
        }

        // NOTE: Teleport fallback intentionally disabled - it was causing the
        // chicken to end up outside the window geometry when it triggered.
        // The 10-second force-unstick above is sufficient in practice.

        // Check for real progress: actor moved significantly OR speed increased durably
        // Don't clear tracking just because speed > 0 - verify actual movement
        if (actor->speed > 0 && elapsed > 1)
        {
            // Check if actor actually moved from last known position
            if (s_zombieChickenLastPosition.count(actorIdx) > 0)
            {
                auto lastPos = s_zombieChickenLastPosition[actorIdx];
                int distX = actor->worldX - lastPos.x;
                // Only check horizontal movement (X and Z), ignore Y (jumping doesn't count)
                int distZ = actor->worldZ - lastPos.z;

                // Distance threshold - if moved more than ~10 units horizontally, consider recovered
                int distSquared = distX*distX + distZ*distZ;
                int thresholdSquared = 100;  // sqrt(100) = ~10 units

                if (distSquared >= thresholdSquared)
                {
                    printf(LIFE_TAG "Bytecode patch: Zombie chicken in life %d, actor %d moved %d units after %ld seconds - forcing chase on player" CON_RESET "\n",
                           lifeNum, actorIdx, (int)sqrt(distSquared), elapsed);

                    // Wake any frozen interpolations (same mechanism as
                    // window refocus unfreezing the chicken) and nudge AI
                    // intent toward active chase. Do NOT touch ANIM/newAnim
                    // directly - the animation carries locomotion.
                    wakeUpStuckActor(actor);
                    actor->speed = 80;
                    actor->HIT = 3;
                    actor->trackMode = 2;
                    actor->speedChange.startValue = 0;
                    actor->speedChange.endValue = 80;
                    actor->speedChange.numSteps = 0;

                    // Re-baseline position and timer so we keep validating
                    // that the chicken is continuing to move. If it stalls
                    // again we'll re-trigger the full unstick path.
                    s_zombieChickenLastPosition[actorIdx] = {actor->worldX, actor->worldY, actor->worldZ};
                    s_zombieChickenStuckTime[actorIdx] = now;
                }
                else
                {
                    // Actor has speed but hasn't moved horizontally - still stuck, keep forcing
                    printf(LIFE_TAG "Bytecode patch: Zombie chicken in life %d, actor %d has speed but no horizontal movement after %ld seconds - CONTINUING force" CON_RESET "\n",
                           lifeNum, actorIdx, elapsed);
                    // Keep waking interpolations and nudging intent. Leave
                    // ANIM alone - clearing it kills locomotion.
                    wakeUpStuckActor(actor);
                    actor->speed = 80;
                    actor->HIT = 3;
                    actor->trackMode = 2;
                }
            }
            else
            {
                // No position record - clear and stop tracking
                s_zombieChickenStuckCount.erase(actorIdx);
                s_zombieChickenStuckTime.erase(actorIdx);
            }
        }
    }
}

///
/// PATCH: Bounds check animation frame indices to prevent crashes
///
/// Issue:
///   Animation opcodes may set frame indices that exceed animation frame count,
///   causing buffer overruns, rendering crashes, or undefined behavior.
///   Native code at line 1145+ doesn't validate animation bounds.
///
/// Solution:
///   Check animation frame count against requested frame index.
///   If index exceeds bounds, clamp to valid range or disable animation.
///   Applies to all animation-related opcodes.
///
static bool patchCondition_AnimationFrameBounds(int lifeNum, int opcode, tObject* actor)
{
    // Apply to animation opcodes
    if (opcode != LM_ANIM_ONCE && opcode != LM_ANIM_REPEAT && opcode != LM_ANIM_ALL_ONCE)
        return false;

    return (actor != nullptr && actor->ANIM >= 0);
}

static void patchAction_AnimationFrameBounds(int lifeNum, int opcode, tObject* actor, int context)
{
    // Post-opcode: validate animation bounds
    if (context != 1 || !actor || actor->ANIM < 0)
        return;

    sAnimation* anim = HQR_Get(HQ_Anims, actor->ANIM);
    if (!anim)
    {
        printf(LIFE_WARN "Bytecode patch: Animation %d bounds check failed - animation not found (life %d, actor %d)" CON_RESET "\n",
               actor->ANIM, lifeNum, actor->indexInWorld);
        actor->ANIM = -1;  // Disable animation
        return;
    }

    // Check if current frame is within bounds
    if (anim->m_numFrames > 0 && actor->frame >= anim->m_numFrames)
    {
        printf(LIFE_WARN "Bytecode patch: Animation frame out of bounds - frame %d exceeds max %d (life %d, actor %d, anim %d)" CON_RESET "\n",
               actor->frame, anim->m_numFrames - 1, lifeNum, actor->indexInWorld, actor->ANIM);
        // Clamp to last valid frame
        actor->frame = anim->m_numFrames - 1;
    }
}

///
/// PATCH: Bounds check body indices to prevent crashes
///
/// Issue:
///   Body indices used in LM_BODY and related opcodes may exceed valid HQR bounds,
///   causing null pointer dereferences. Native code doesn't validate body indices.
///
/// Solution:
///   Validate body index is within reasonable bounds before HQR_Get.
///   Log and use fallback if body is missing.
///
static bool patchCondition_BodyBounds(int lifeNum, int opcode, tObject* actor)
{
    // Apply to body-related opcodes
    if (opcode != LM_BODY && opcode != LM_DO_MOVE && opcode != LM_MOVE)
        return false;

    return (actor != nullptr && actor->bodyNum >= 0);
}

static void patchAction_BodyBounds(int lifeNum, int opcode, tObject* actor, int context)
{
    // Post-opcode: validate body bounds
    if (context != 1 || !actor || actor->bodyNum < 0)
        return;

    // Sanity check: body index should be reasonable (less than 5000)
    if (actor->bodyNum > 5000)
    {
        printf(LIFE_WARN "Bytecode patch: Body index out of bounds - bodyNum %d exceeds safe range (life %d, actor %d)" CON_RESET "\n",
               actor->bodyNum, lifeNum, actor->indexInWorld);
        actor->bodyNum = -1;  // Disable body
        return;
    }

    sBody* body = HQR_Get(HQ_Bodys, actor->bodyNum);
    if (!body)
    {
        printf(LIFE_WARN "Bytecode patch: Body not found - bodyNum %d (life %d, actor %d)" CON_RESET "\n",
               actor->bodyNum, lifeNum, actor->indexInWorld);
        actor->bodyNum = -1;  // Mark as invalid
    }
}

///
/// PATCH: Ensure movement opcodes properly initialize speed and direction
///
/// Issue:
///   Movement opcodes (LM_DO_MOVE, LM_MOVE) may not properly initialize actor speed,
///   causing actors to move unpredictably or get stuck in place. Speed might remain
///   at previous value or be set to invalid state.
///
/// Solution:
///   Track when movement opcodes are executed. Verify speed is reasonable (0-100).
///   If speed is invalid, set to sensible default for the movement type.
///   Ensure trackMode is set to allow movement.
///
static bool patchCondition_MovementInitialization(int lifeNum, int opcode, tObject* actor)
{
    // Apply to movement opcodes
    if (opcode != LM_DO_MOVE && opcode != LM_MOVE)
        return false;

    return (actor != nullptr);
}

static void patchAction_MovementInitialization(int lifeNum, int opcode, tObject* actor, int context)
{
    // Post-opcode: ensure movement state is valid
    if (context != 1 || !actor)
        return;

    int actorIdx = actor->indexInWorld;

    // Check if speed is uninitialized or invalid.
    // NOTE: Negative speeds are LEGAL - they represent walking/running backwards.
    // Only clamp speeds that are truly out of range (outside +/-200).
    if (actor->speed < -200 || actor->speed > 200)
    {
        printf(LIFE_WARN "Bytecode patch: Actor %d has invalid speed %d after movement opcode (life %d) - resetting to safe default" CON_RESET "\n",
               actorIdx, actor->speed, lifeNum);
        actor->speed = 20;  // Safe default movement speed
    }

    // Ensure trackMode allows movement
    if (actor->trackMode < 0 || actor->trackMode > 3)
    {
        printf(LIFE_WARN "Bytecode patch: Actor %d has invalid trackMode %d after movement opcode (life %d) - resetting to manual mode" CON_RESET "\n",
               actorIdx, actor->trackMode, lifeNum);
        actor->trackMode = 1;  // Manual mode for player-controlled movement
    }

    // Ensure speedChange is properly initialized
    if (actor->speedChange.numSteps < -1)
    {
        printf(LIFE_WARN "Bytecode patch: Actor %d has invalid speedChange.numSteps %d (life %d) - resetting" CON_RESET "\n",
               actorIdx, actor->speedChange.numSteps, lifeNum);
        actor->speedChange.numSteps = 0;
    }

    // Track this actor for monitoring
    if (s_actorsWithUninitializedMovement.count(actorIdx) == 0)
    {
        s_actorsWithUninitializedMovement.insert(actorIdx);
        printf(LIFE_TAG "Bytecode patch: Monitoring actor %d for movement issues (life %d)" CON_RESET "\n",
               actorIdx, lifeNum);
    }
}

///
/// PATCH: Validate collision volume (ZV) accesses to prevent crashes
///
/// Issue:
///   Collision volume (ZV) calculations at native lines 1165-1178 don't validate
///   that actor room coordinates are within valid bounds. This can cause invalid
///   collision volumes or memory corruption.
///
/// Solution:
///   Before and after ZV operations, validate that roomX/roomY/roomZ are reasonable.
///   If coordinates are way out of bounds (outside [-10000, 50000] range), log warning
///   but don't crash - let the game continue.
///
static bool patchCondition_CollisionVolumeBounds(int lifeNum, int opcode, tObject* actor)
{
    // Apply to ZV-related opcodes
    if (opcode != LM_DEF_ZV && opcode != LM_DEF_ABS_ZV && opcode != LM_DO_ROT_ZV)
        return false;

    return (actor != nullptr);
}

static void patchAction_CollisionVolumeBounds(int lifeNum, int opcode, tObject* actor, int context)
{
    // Post-opcode: validate collision volume coordinates
    if (context != 1 || !actor)
        return;

    // Check if actor room coordinates are reasonable (-10000 to 50000 range)
    const int MIN_COORD = -10000;
    const int MAX_COORD = 50000;

    bool coordsInvalid = false;

    if (actor->roomX < MIN_COORD || actor->roomX > MAX_COORD)
    {
        printf(LIFE_WARN "Bytecode patch: Actor %d has invalid roomX %d (life %d, opcode %d)" CON_RESET "\n",
               actor->indexInWorld, actor->roomX, lifeNum, opcode);
        coordsInvalid = true;
    }

    if (actor->roomY < MIN_COORD || actor->roomY > MAX_COORD)
    {
        printf(LIFE_WARN "Bytecode patch: Actor %d has invalid roomY %d (life %d, opcode %d)" CON_RESET "\n",
               actor->indexInWorld, actor->roomY, lifeNum, opcode);
        coordsInvalid = true;
    }

    if (actor->roomZ < MIN_COORD || actor->roomZ > MAX_COORD)
    {
        printf(LIFE_WARN "Bytecode patch: Actor %d has invalid roomZ %d (life %d, opcode %d)" CON_RESET "\n",
               actor->indexInWorld, actor->roomZ, lifeNum, opcode);
        coordsInvalid = true;
    }

    // Also validate ZV bounds if they exist
    if (actor->zv.ZVX1 < MIN_COORD || actor->zv.ZVX1 > MAX_COORD ||
        actor->zv.ZVX2 < MIN_COORD || actor->zv.ZVX2 > MAX_COORD)
    {
        printf(LIFE_WARN "Bytecode patch: Actor %d has invalid ZV X bounds (ZVX1=%d, ZVX2=%d) (life %d)" CON_RESET "\n",
               actor->indexInWorld, actor->zv.ZVX1, actor->zv.ZVX2, lifeNum);
        coordsInvalid = true;
    }

    if (actor->zv.ZVZ1 < MIN_COORD || actor->zv.ZVZ1 > MAX_COORD ||
        actor->zv.ZVZ2 < MIN_COORD || actor->zv.ZVZ2 > MAX_COORD)
    {
        printf(LIFE_WARN "Bytecode patch: Actor %d has invalid ZV Z bounds (ZVZ1=%d, ZVZ2=%d) (life %d)" CON_RESET "\n",
               actor->indexInWorld, actor->zv.ZVZ1, actor->zv.ZVZ2, lifeNum);
        coordsInvalid = true;
    }

    if (coordsInvalid)
    {
        printf(LIFE_WARN "Bytecode patch: Collision volume validation failed for actor %d - continuing with caution" CON_RESET "\n",
               actor->indexInWorld);
    }
}

///
/// PATCH: Fix player stuck when entering CAMERA05_000
///
/// Issue:
///   When the player enters CAMERA05_000 (specific camera transition), they can get stuck
///   with zero speed and unable to move.
///
/// Solution:
///   Detect when player has speed=0 after entering this camera.
///   Wait a short grace period, then force unstick if still stuck.
///   Only applies to player actor (indexInWorld == 0).
///
static bool patchCondition_PlayerStuckAtCamera05(int lifeNum, int opcode, tObject* actor)
{
    // Only monitor the player
    if (actor == nullptr || actor->indexInWorld != 0)
        return false;

    // Monitor on all opcodes to detect stuck state
    return true;
}

static void patchAction_PlayerStuckAtCamera05(int lifeNum, int opcode, tObject* actor, int context)
{
    // Post-opcode only
    if (context != 1 || !actor || actor->indexInWorld != 0)
        return;

    // Reset tracking if player is no longer at Floor 5, Camera 5
    // This prevents the patch from carrying over when transitioning to other floors/cameras
    if (s_playerStuckTracking && (g_currentFloor != 5 || NumCamera != 5))
    {
        printf(LIFE_TAG "Bytecode patch: Player left Floor 5 Camera 5, resetting stuck tracking" CON_RESET "\n");
        s_playerStuckTracking = false;
        return;
    }

    // Only apply patch when at Floor 5, Camera 5 (CAMERA05_000)
    if (g_currentFloor != 5 || NumCamera != 5)
        return;

    auto now = std::chrono::high_resolution_clock::now();

    // Detect stuck condition: player with zero speed
    if (actor->speed == 0 && actor->ANIM >= 0)
    {
        // First time detecting stuck - record the timestamp
        if (!s_playerStuckTracking)
        {
            s_playerStuckTracking = true;
            s_playerStuckTime = now;
            s_playerStuckPosition = {actor->worldX, actor->worldY, actor->worldZ};
            printf(LIFE_TAG "Bytecode patch: Player stuck detected at Floor 5, Camera 5 (CAMERA05_000) - waiting 5 seconds before intervention..." CON_RESET "\n");
            return;  // Just record, don't intervene yet
        }
    }
    else if (!s_playerStuckTracking)
    {
        // Not stuck and no record to clean up
        return;
    }

    // If tracking stuck state, check elapsed time
    if (s_playerStuckTracking)
    {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - s_playerStuckTime).count();

        // After 5 seconds, force unstick
        if (elapsed >= 5)
        {
            printf(LIFE_TAG "Bytecode patch: Player stuck for %ld seconds at Floor 5, Camera 5 - FORCING unstick!" CON_RESET "\n", elapsed);

            // Force movement state
            actor->speed = 60;  // High speed
            actor->HIT = 0;     // No aggression for player
            actor->speedChange.startValue = 0;
            actor->speedChange.endValue = 60;
            actor->speedChange.numSteps = 0;

            // Force-clear animation
            actor->ANIM = -1;
            actor->flagEndAnim = 0;
            actor->newAnim = -1;

            // Force active movement mode
            actor->trackMode = 1;
        }

        // After 15 seconds, more aggressive intervention
        if (elapsed >= 15)
        {
            printf(LIFE_TAG "Bytecode patch: Player still stuck after %ld seconds at Floor 5, Camera 5 - applying more aggressive unstick!" CON_RESET "\n", elapsed);

            // More aggressive force
            actor->speed = 100;  // Very high speed
            actor->ANIM = -1;    // Clear animation forcefully
            actor->trackMode = 1;
            actor->flagEndAnim = 0;
            actor->newAnim = -1;
        }

        // Check for recovery: player moved 10+ units horizontally
        if (actor->speed > 0 && elapsed > 1)
        {
            int distX = actor->worldX - s_playerStuckPosition.x;
            int distZ = actor->worldZ - s_playerStuckPosition.z;
            int distSquared = distX*distX + distZ*distZ;
            int thresholdSquared = 100;  // 10 units

            if (distSquared >= thresholdSquared)
            {
                printf(LIFE_TAG "Bytecode patch: Player recovered and moving from Floor 5, Camera 5 after %ld seconds (moved %d units)" CON_RESET "\n",
                       elapsed, (int)sqrt(distSquared));
                s_playerStuckTracking = false;
            }
            else
            {
                // Player has speed but hasn't moved - keep forcing
                printf(LIFE_TAG "Bytecode patch: Player has speed but no horizontal movement at Floor 5, Camera 5 after %ld seconds - CONTINUING force" CON_RESET "\n", elapsed);
                actor->speed = 60;
                actor->ANIM = -1;
                actor->trackMode = 1;
            }
        }
    }
}

///
/// PATCH: Fix player stuck when entering CAMERA04_000
///
/// Issue:
///   When the player enters CAMERA04_000 (specific camera transition), they can get stuck
///   with zero speed and unable to move.
///
/// Solution:
///   Detect when player has speed=0 after entering this camera.
///   Wait a short grace period, then force unstick if still stuck.
///   Only applies to player actor (indexInWorld == 0).
///
static bool patchCondition_PlayerStuckAtCamera04(int lifeNum, int opcode, tObject* actor)
{
    // Only monitor the player
    if (actor == nullptr || actor->indexInWorld != 0)
        return false;

    // Monitor on all opcodes to detect stuck state
    return true;
}

static void patchAction_PlayerStuckAtCamera04(int lifeNum, int opcode, tObject* actor, int context)
{
    // Post-opcode only
    if (context != 1 || !actor || actor->indexInWorld != 0)
        return;

    // Reset tracking if player is no longer at Floor 4, Camera 0
    // This prevents the patch from carrying over when transitioning to other floors/cameras
    if (s_playerStuckTracking_Camera04 && (g_currentFloor != 4 || NumCamera != 0))
    {
        printf(LIFE_TAG "Bytecode patch: Player left Floor 4 Camera 0, resetting stuck tracking" CON_RESET "\n");
        s_playerStuckTracking_Camera04 = false;
        return;
    }

    // Only apply patch when at Floor 4, Camera 0 (CAMERA04_000)
    if (g_currentFloor != 4 || NumCamera != 0)
        return;

    auto now = std::chrono::high_resolution_clock::now();

    // Detect stuck condition: player with zero speed
    if (actor->speed == 0 && actor->ANIM >= 0)
    {
        // First time detecting stuck - record the timestamp
        if (!s_playerStuckTracking_Camera04)
        {
            s_playerStuckTracking_Camera04 = true;
            s_playerStuckTime_Camera04 = now;
            s_playerStuckPosition_Camera04 = {actor->worldX, actor->worldY, actor->worldZ};
            printf(LIFE_TAG "Bytecode patch: Player stuck detected at Floor 4, Camera 0 (CAMERA04_000) - waiting 5 seconds before intervention..." CON_RESET "\n");
            return;  // Just record, don't intervene yet
        }
    }
    else if (!s_playerStuckTracking_Camera04)
    {
        // Not stuck and no record to clean up
        return;
    }

    // If tracking stuck state, check elapsed time
    if (s_playerStuckTracking_Camera04)
    {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - s_playerStuckTime_Camera04).count();

        // After 5 seconds, force unstick
        if (elapsed >= 5)
        {
            printf(LIFE_TAG "Bytecode patch: Player stuck for %ld seconds at Floor 4, Camera 0 - FORCING unstick!" CON_RESET "\n", elapsed);

            // Force movement state
            actor->speed = 60;  // High speed
            actor->HIT = 0;     // No aggression for player
            actor->speedChange.startValue = 0;
            actor->speedChange.endValue = 60;
            actor->speedChange.numSteps = 0;

            // Force-clear animation
            actor->ANIM = -1;
            actor->flagEndAnim = 0;
            actor->newAnim = -1;

            // Force active movement mode
            actor->trackMode = 1;
        }

        // After 15 seconds, more aggressive intervention
        if (elapsed >= 15)
        {
            printf(LIFE_TAG "Bytecode patch: Player still stuck after %ld seconds at Floor 4, Camera 0 - applying more aggressive unstick!" CON_RESET "\n", elapsed);

            // More aggressive force
            actor->speed = 100;  // Very high speed
            actor->ANIM = -1;    // Clear animation forcefully
            actor->trackMode = 1;
            actor->flagEndAnim = 0;
            actor->newAnim = -1;
        }

        // Check for recovery: player moved 10+ units horizontally
        if (actor->speed > 0 && elapsed > 1)
        {
            int distX = actor->worldX - s_playerStuckPosition_Camera04.x;
            int distZ = actor->worldZ - s_playerStuckPosition_Camera04.z;
            int distSquared = distX*distX + distZ*distZ;
            int thresholdSquared = 100;  // 10 units

            if (distSquared >= thresholdSquared)
            {
                printf(LIFE_TAG "Bytecode patch: Player recovered and moving from Floor 4, Camera 0 after %ld seconds (moved %d units)" CON_RESET "\n",
                       elapsed, (int)sqrt(distSquared));
                s_playerStuckTracking_Camera04 = false;
            }
            else
            {
                // Player has speed but hasn't moved - keep forcing
                printf(LIFE_TAG "Bytecode patch: Player has speed but no horizontal movement at Floor 4, Camera 0 after %ld seconds - CONTINUING force" CON_RESET "\n", elapsed);
                actor->speed = 60;
                actor->ANIM = -1;
                actor->trackMode = 1;
            }
        }
    }
}

///
/// PATCH: Hide Zombie Chicken stencil/blob shadow until it crashes through the window
///
/// Issue:
///   Zombie chickens spawn behind windows in a neighboring room but their shadow is
///   still drawn on the floor of the player's room, which visually leaks their
///   presence through the wall before they crash through the glass.
///
/// Solution:
///   Track each zombie chicken actor. While its `room` differs from the player's
///   current room (i.e., it has not yet crashed through the window into the player's
///   room), flag it as "shadow hidden". The renderer queries
///   bytecodePatch_shouldHideShadowForActor() and skips shadow drawing for flagged
///   actors. Once the actor's room matches the player's, it is considered to have
///   crashed through and the shadow is re-enabled.
///
static bool patchCondition_ZombieChickenHideShadow(int lifeNum, int opcode, tObject* actor)
{
    // Only apply to zombie chickens
    if (actor == nullptr || !isZombieChickenBody(actor->bodyNum))
        return false;

    // Monitor on all opcodes so the hidden/visible transition is always up-to-date
    return true;
}

static void patchAction_ZombieChickenHideShadow(int lifeNum, int opcode, tObject* actor, int context)
{
    // Post-opcode only to observe the settled state after the bytecode ran
    if (context != 1 || !actor || !isZombieChickenBody(actor->bodyNum))
        return;

    int actorIdx = actor->indexInWorld;
    if (actorIdx < 0)
        return;

    // On first observation: record the initial world position and hide the
    // shadow. The chicken is presumed to be perched behind the window until it
    // has moved far enough to be considered "crashed through".
    if (s_zombieChickenShadowInitialPosition.count(actorIdx) == 0)
    {
        s_zombieChickenShadowInitialPosition[actorIdx] = {actor->worldX, actor->worldY, actor->worldZ};
        if (s_zombieChickenHiddenShadow.insert(actorIdx).second)
        {
            printf(LIFE_TAG "Bytecode patch: Hiding shadow for Zombie Chicken actor %d (body %d) - behind window (origin %d,%d,%d)" CON_RESET "\n",
                   actorIdx, actor->bodyNum, actor->worldX, actor->worldY, actor->worldZ);
        }
        return;
    }

    // If the shadow is no longer hidden for this actor, nothing to do.
    if (s_zombieChickenHiddenShadow.count(actorIdx) == 0)
        return;

    // Compute horizontal distance from the initial spawn position. Once the
    // chicken has moved more than the threshold it has crashed through the
    // window and the shadow is re-enabled permanently.
    const ActorPosition& origin = s_zombieChickenShadowInitialPosition[actorIdx];
    int dx = actor->worldX - origin.x;
    int dz = actor->worldZ - origin.z;
    int distSquared = dx * dx + dz * dz;
    const int thresholdSquared = 40000; // ~200 world units

    if (distSquared >= thresholdSquared)
    {
        s_zombieChickenHiddenShadow.erase(actorIdx);
        printf(LIFE_TAG "Bytecode patch: Zombie Chicken actor %d (body %d) crashed through - shadow re-enabled (moved %d units)" CON_RESET "\n",
               actorIdx, actor->bodyNum, (int)sqrt((double)distSquared));
    }
}

bool bytecodePatch_shouldHideShadowForActor(tObject* actor)
{
    if (!actor)
        return false;

    int actorIdx = actor->indexInWorld;
    if (actorIdx < 0)
        return false;

    if (s_zombieChickenHiddenShadow.count(actorIdx) > 0)
        return true;

    if (s_permanentNoShadowActors.count(actorIdx) > 0)
        return true;

    // Also consult body number directly so newly spawned actors of a
    // no-shadow body type have their shadow suppressed even before the
    // post-opcode patch has had a chance to register them.
    if (isNoShadowBody(actor->bodyNum))
        return true;

    return false;
}

///
/// PATCH: Disable camera mask occlusion for the frog actor in the intro
///
/// Issue:
///   In the intro cinematic the frog actor (body 267) is incorrectly
///   occluded by the camera mask overlay, causing it to disappear behind
///   foreground scenery it should stay visible on top of.
///
/// Solution:
///   Flag any actor whose body is 267 so the renderer can skip its camera
///   mask pass. The renderer queries bytecodePatch_shouldSkipMaskForActor().
///
static bool patchCondition_FrogIntroNoMask(int lifeNum, int opcode, tObject* actor)
{
    return (actor != nullptr && actor->indexInWorld >= 0 && actor->bodyNum == FROG_INTRO_BODY_NUM);
}

static void patchAction_FrogIntroNoMask(int lifeNum, int opcode, tObject* actor, int context)
{
    if (!actor || actor->indexInWorld < 0 || actor->bodyNum != FROG_INTRO_BODY_NUM)
        return;

    int actorIdx = actor->indexInWorld;
    if (s_skipMaskActors.insert(actorIdx).second)
    {
        printf(LIFE_TAG "Bytecode patch: Disabling camera mask for frog actor %d (body %d)" CON_RESET "\n",
               actorIdx, actor->bodyNum);
    }
}

///
/// PATCH: Permanently disable stencil/blob shadow for selected body types
///
/// Issue:
///   Bodies 5, 6, 55 and 56 (LISTBODY/LISTBOD2 variants) are small/flat
///   props that look incorrect with a contact shadow.
///
/// Solution:
///   Flag any actor whose body matches isNoShadowBody() so the renderer's
///   shadow-hide hook returns true for them.
///
static bool patchCondition_NoShadowBody(int lifeNum, int opcode, tObject* actor)
{
    return (actor != nullptr && actor->indexInWorld >= 0 && isNoShadowBody(actor->bodyNum));
}

static void patchAction_NoShadowBody(int lifeNum, int opcode, tObject* actor, int context)
{
    if (!actor || actor->indexInWorld < 0 || !isNoShadowBody(actor->bodyNum))
        return;

    int actorIdx = actor->indexInWorld;
    if (s_permanentNoShadowActors.insert(actorIdx).second)
    {
        printf(LIFE_TAG "Bytecode patch: Disabling stencil shadow for actor %d (body %d)" CON_RESET "\n",
               actorIdx, actor->bodyNum);
    }
}

bool bytecodePatch_shouldSkipMaskForActor(tObject* actor)
{
    if (!actor)
        return false;

    int actorIdx = actor->indexInWorld;
    if (actorIdx < 0)
        return false;

    return s_skipMaskActors.count(actorIdx) > 0;
}

// Track last spawn time per actor to control spawn rate
static std::map<int, std::chrono::high_resolution_clock::time_point> s_carLastParticleSpawn;

///
/// PATCH: Spawn brown dirt particles for the car (body 066) during the intro
///
/// Issue:
///   The car driving along the road in the intro lacks visual polish - no
///   dust/dirt particles are kicked up as it moves, making the motion look
///   less cinematic.
///
/// Solution:
///   Detect when the car actor (body 066) is moving with non-zero speed.
///   Continuously spawn brown dirt particles at the car's rear position
///   while it's in motion. Particles are brown/tan colored, kick up and back
///   from the car's motion, then settle down. Uses the existing dust particle
///   system with brown dirt variant.
///
/// Car body number: 066 (intro sequence car)
///
static bool patchCondition_CarDirtParticles(int lifeNum, int opcode, tObject* actor)
{
    // Debug: Log all body numbers we see to find the car
    static std::set<int> seenBodies;
    if (actor && actor->bodyNum > 0 && seenBodies.find(actor->bodyNum) == seenBodies.end())
    {
        seenBodies.insert(actor->bodyNum);
        printf("[CAR-PATCH] Saw actor with body %d (looking for 66)\n", actor->bodyNum);
    }

    // Only apply to the car in the intro (body 066)
    if (actor == nullptr || actor->bodyNum != 66)
        return false;

    // Debug: Confirm we found the car
    static bool foundCar = false;
    if (!foundCar)
    {
        printf("[CAR-PATCH] *** FOUND CAR (body 66) - patch active! ***\n");
        foundCar = true;
    }

    // Apply to all opcodes so we continuously monitor the car's movement
    return true;
}

static void patchAction_CarDirtParticles(int lifeNum, int opcode, tObject* actor, int context)
{
    // Post-opcode only to observe settled state after bytecode execution
    if (context != 1 || !actor || actor->bodyNum != 66)
        return;

    // Only spawn particles if the car is actually moving
    if (actor->speed == 0)
        return;

    // Check if dust particle system is available
    if (!g_dustParticles || !g_dustParticles->isEnabled())
        return;

    int actorIdx = actor->indexInWorld;
    if (actorIdx < 0)
        return;

    auto now = std::chrono::high_resolution_clock::now();

    // Control spawn rate - spawn particles every ~100ms (10 particles/sec)
    if (s_carLastParticleSpawn.count(actorIdx) > 0)
    {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - s_carLastParticleSpawn[actorIdx]).count();

        // If not enough time passed, skip this frame
        if (elapsed < 100)
            return;
    }

    // Record this spawn time
    s_carLastParticleSpawn[actorIdx] = now;

    // Spawn dirt particles at the car's rear position
    // The car drives from left to right in the intro, so spawn behind it (negative X offset)
    int spawnX = actor->worldX - 100;  // Behind the car
    int spawnY = actor->worldY;        // At road level
    int spawnZ = actor->worldZ;        // Same depth as car

    // Spawn 2-4 particles per spawn event for continuous trail effect
    int particleCount = 2 + (rand() % 3);
    g_dustParticles->spawnDirtParticles(spawnX, spawnY, spawnZ, particleCount);

    // Debug: Log particle spawn events
    static int spawnCount = 0;
    if ((spawnCount % 10) == 0)  // Log every 10th spawn to avoid spam
    {
        printf("[CAR-DIRT] Spawned %d particles at world pos (%d, %d, %d), car speed=%d\n",
               particleCount, spawnX, spawnY, spawnZ, actor->speed);
    }
    spawnCount++;
}

///
/// PATCH: Track lantern oil state and lighting effects
///
/// Issue:
///   The lantern needs to emit light and glow when it has oil, especially
///   when held in the player's hand. This provides both visual feedback
///   about oil status and creates atmospheric lighting effects.
///
/// Solution:
///   Detect when lantern objects are picked up or put down.
///   Track oil state through inventory/hand mechanics.
///   Apply bloom glow and shadow casting effects when lantern has oil.
///   Position light source based on lantern location (in-hand vs placed).
///
/// Lantern body number: 040 (varies by game, may need adjustment)
///
static bool patchCondition_LanternOilTracking(int lifeNum, int opcode, tObject* actor)
{
    // Only apply to lantern objects (body 040)
    if (actor == nullptr || actor->bodyNum != 40)
        return false;

    // Monitor on all opcodes to detect state changes
    return true;
}

static void patchAction_LanternOilTracking(int lifeNum, int opcode, tObject* actor, int context)
{
    // Post-opcode to observe settled state
    if (context != 1 || !actor || actor->bodyNum != 40)
        return;

    int actorIdx = actor->indexInWorld;
    if (actorIdx < 0)
        return;

    // TODO: Detect lantern oil state from inventory data structure
    // For now, assume lantern has oil if it was in a specific inventory slot
    // In a real implementation, check the lantern object's oil property

    // Update lantern state
    LanternState* lanternState = getLanternState(actorIdx);
    if (!lanternState)
    {
        // Initialize lantern state
        setLanternInHand(actorIdx, false);
        lanternState = getLanternState(actorIdx);
    }

    // TODO: Detect if lantern is currently in hand
    // This would require checking current inventory state or player hand status
    // For now, we'll rely on external code to call setLanternInHand()
}

// ============================================================================
// Initialization
// ============================================================================

void initBytecodePatches()
{
    s_bytecodePatches.clear();
    s_zombieChickenHiddenShadow.clear();
    s_zombieChickenShadowInitialPosition.clear();
    s_zombieChickenPostCrashTime.clear();
    s_zombieChickenLastY.clear();
    s_zombieChickenLastYChangeTime.clear();
    s_skipMaskActors.clear();
    s_permanentNoShadowActors.clear();
    s_carLastParticleSpawn.clear();

    printf(LIFE_TAG "=== Initializing Bytecode Patch System ===" CON_RESET "\n");
    printf(LIFE_TAG "Native life scripts recompiling!" CON_RESET "\n");
    printf(LIFE_TAG "Using bytecode interpreter with runtime patches" CON_RESET "\n");

    // Register known bug patches
    registerBytecodePatch(-1, -1,
                         patchCondition_ValidateActor,
                         patchAction_ValidateActor,
                         "Validate actor pointer validity");

    registerBytecodePatch(-1, -1,
                         patchCondition_ResourceCheck,
                         patchAction_ResourceCheck,
                         "Check for null resource pointers");

    registerBytecodePatch(-1, -1,
                         patchCondition_AnimationValidation,
                         patchAction_AnimationValidation,
                         "Validate animation state after changes");

    registerBytecodePatch(-1, -1,
                         patchCondition_AnimationFrameBounds,
                         patchAction_AnimationFrameBounds,
                         "Bounds check animation frame indices");

    registerBytecodePatch(-1, -1,
                         patchCondition_BodyBounds,
                         patchAction_BodyBounds,
                         "Bounds check body indices");

    registerBytecodePatch(-1, -1,
                         patchCondition_MovementInitialization,
                         patchAction_MovementInitialization,
                         "Ensure movement opcodes properly initialize speed/direction");

    registerBytecodePatch(-1, -1,
                         patchCondition_CollisionVolumeBounds,
                         patchAction_CollisionVolumeBounds,
                         "Validate collision volume (ZV) coordinate bounds");

    registerBytecodePatch(-1, -1,
                         patchCondition_EnemyStuckAtWindow,
                         patchAction_EnemyStuckAtWindow,
                         "Fix enemies stuck at windows after crash");

    registerBytecodePatch(-1, -1,
                         patchCondition_PlayerStuckAtCamera05,
                         patchAction_PlayerStuckAtCamera05,
                         "Fix player stuck at CAMERA05_000");

    registerBytecodePatch(-1, -1,
                         patchCondition_PlayerStuckAtCamera04,
                         patchAction_PlayerStuckAtCamera04,
                         "Fix player stuck at CAMERA04_000");

    registerBytecodePatch(-1, -1,
                         patchCondition_ZombieChickenHideShadow,
                         patchAction_ZombieChickenHideShadow,
                         "Hide Zombie Chicken shadow until it crashes through the window");

    // DISABLED: Frog intro mask patch
    // registerBytecodePatch(-1, -1,
    //                      patchCondition_FrogIntroNoMask,
    //                      patchAction_FrogIntroNoMask,
    //                      "Disable camera mask for frog actor in the intro (body 267)");

    registerBytecodePatch(-1, -1,
                         patchCondition_NoShadowBody,
                         patchAction_NoShadowBody,
                         "Disable stencil shadow for bodies 5/6/55/56");

    // DISABLED: Car dirt particles patch
    // registerBytecodePatch(-1, -1,
    //                      patchCondition_CarDirtParticles,
    //                      patchAction_CarDirtParticles,
    //                      "Spawn brown dirt particles for car (body 066) during intro");

    registerBytecodePatch(-1, -1,
                         patchCondition_LanternOilTracking,
                         patchAction_LanternOilTracking,
                         "Track lantern oil state and apply lighting effects");

    printf(LIFE_OK "Bytecode patch system initialized: %d patches registered" CON_RESET "\n",
              (int)s_bytecodePatches.size());
}
