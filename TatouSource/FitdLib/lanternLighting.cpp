///////////////////////////////////////////////////////////////////////////////
// Lantern Oil Lighting System - Implementation
//
// Handles dynamic bloom and shadow casting for the lantern when it has oil
// and is being held in the player's hand.
//
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "lanternLighting.h"
#include "consoleLog.h"
#include "bgfxGlue.h"
#include "hdBackgroundRenderer.h"
#include "shaders/embeddedShaders.h"
#include "vars.h"
#include "renderer.h"
#include "imguiBGFX.h"
#include <map>
#include <cmath>
#include <chrono>

// Global lantern state tracking
std::map<int, LanternState> g_lanternStates;

// Shader handles for lantern effects
static bgfx::ProgramHandle s_lanternBloomShader = BGFX_INVALID_HANDLE;
static bgfx::ProgramHandle s_lanternShadowShader = BGFX_INVALID_HANDLE;

// Uniforms for lantern shaders
static bgfx::UniformHandle u_lanternPos = BGFX_INVALID_HANDLE;
static bgfx::UniformHandle u_lanternColor = BGFX_INVALID_HANDLE;
static bgfx::UniformHandle u_lanternIntensity = BGFX_INVALID_HANDLE;
static bgfx::UniformHandle u_lanternRadius = BGFX_INVALID_HANDLE;

// Warm orange/yellow color for oil lantern glow
static constexpr float LANTERN_GLOW_R = 1.0f;
static constexpr float LANTERN_GLOW_G = 0.8f;
static constexpr float LANTERN_GLOW_B = 0.3f;
static constexpr float LANTERN_BASE_RADIUS = 200.0f;  // World units

// Body numbers
static constexpr int LAMP_BODY_NUM = 11;  // Held lit lamp (both LISTBODY/Carny and LISTBOD2/Emily)

// Sticky primitive tracking - locks onto the lamp primitive once found
struct LampPrimitiveTracker
{
    int cachedOriginalPrimIndex = -1;  // The originalPrimIndex of the locked lamp primitive
    float lastKnownX = 0.0f;           // Last known screen position X
    float lastKnownY = 0.0f;           // Last known screen position Y
    bool hasValidCache = false;        // Whether we have a locked primitive
    int missFrameCount = 0;            // Frames since we lost the primitive

    void reset()
    {
        cachedOriginalPrimIndex = -1;
        hasValidCache = false;
        missFrameCount = 0;
    }
};

static LampPrimitiveTracker s_lampTracker;

// Fade state for smooth glow transitions
struct LanternGlowFade
{
    float currentFade = 0.0f;    // Current fade level (0.0 = invisible, 1.0 = full brightness)
    float targetFade = 0.0f;     // Target fade level
    float fadeSpeed = 3.5f;      // Fade speed (units per second)
    bool hasEverBeenVisible = false; // Track if lamp has been visible at least once

    void update(float deltaTime)
    {
        // Smoothly interpolate toward target
        if (currentFade < targetFade)
        {
            currentFade += fadeSpeed * deltaTime;
            if (currentFade > targetFade)
                currentFade = targetFade;
        }
        else if (currentFade > targetFade)
        {
            currentFade -= fadeSpeed * deltaTime;
            if (currentFade < targetFade)
                currentFade = targetFade;
        }
    }

    void setTarget(float target)
    {
        targetFade = target;
        if (target > 0.0f)
            hasEverBeenVisible = true;
    }

    void reset()
    {
        currentFade = 0.0f;
        targetFade = 0.0f;
        hasEverBeenVisible = false;
    }
};

static LanternGlowFade s_glowFade;

// Set to true while any menu (inventory, system menu, etc.) is open
// to suppress the glow overlay so it doesn't bleed through the UI.
static bool s_isMenuActive = false;

// Flag to allow glow to fade back in immediately after menu closes
// Without this, occlusion detection would immediately suppress the glow again
static bool s_allowGlowFadeIn = true;
static float s_fadeInTimeRemaining = 0.0f;  // Time to allow fade-in before occlusion takes over
static constexpr float FADE_IN_GRACE_PERIOD = 0.1f;  // 100ms grace period after menu close

void setLanternMenuActive(bool active)
{
    s_isMenuActive = active;
    printf("[LANTERN] setLanternMenuActive(%s) called - s_isMenuActive=%d\n", active ? "true" : "false", s_isMenuActive);

    if (active)
    {
        // Immediately kill the fade so the glow is gone before any
        // scene snapshot / freeze is captured by the menu system.
        s_glowFade.currentFade = 0.0f;
        s_glowFade.targetFade  = 0.0f;
        s_allowGlowFadeIn = false;  // Suppress fade-in logic while menu is open
        printf("[LANTERN] Menu opened: glow suppressed immediately\n");
    }
    else
    {
        // Menu is closing - allow glow to fade back in if lantern is still lit
        // Use setTarget to allow smooth interpolation back to full brightness
        s_glowFade.setTarget(1.0f);
        s_allowGlowFadeIn = true;                      // Re-enable fade-in
        s_fadeInTimeRemaining = FADE_IN_GRACE_PERIOD;  // Set grace period so occlusion doesn't immediately suppress it
        printf("[LANTERN] Menu closed: grace period ENABLED for %.1fms, targetFade set to 1.0\n", FADE_IN_GRACE_PERIOD * 1000.0f);
    }
}

// Renderer primitive structures
// Duplicated here since they're not in a public header
#ifndef NUM_MAX_VERTEX_IN_PRIM
#define NUM_MAX_VERTEX_IN_PRIM 64
#endif

struct rendererPointStruct
{
    float X;
    float Y;
    float Z;
};

struct primEntryStruct
{
    u8 material;
    u8 color;
    u16 size;
    u16 numOfVertices;
    int type;  // primTypeEnum value  
    int originalPrimIndex;
    bool isRampPrim;  // true for ramp-shaded primitives (material 3-6)
    bool nearClipped;
    rendererPointStruct vertices[NUM_MAX_VERTEX_IN_PRIM];
};

extern primEntryStruct primTable[];
extern u32 positionInPrimEntry;

// Dedicated cache for lamp body (body 11) primitives
// Populated during body 11 rendering, persists after other bodies overwrite primTable[]
struct LampPrimitiveCache
{
    static const int MAX_LAMP_PRIMS = 100;
    struct CachedPrim
    {
        int originalPrimIndex;
        int type;
        int material;
        u8 color;  // Color index from primitive (for rendering mask with actual colors)
        bool isRampPrim;
        bool nearClipped;
        int numOfVertices;
        struct { float X, Y, Z; } vertices[8];  // Simplified point structure for cache
        float centerX, centerY;  // Pre-calculated centroid
    };

    CachedPrim prims[MAX_LAMP_PRIMS];
    int count = 0;
    bool isValid = false;  // Set to true when body 11 renders, false when player drops lamp

    void clear()
    {
        count = 0;
        isValid = false;
    }
};

static LampPrimitiveCache s_lampPrimCache;

///////////////////////////////////////////////////////////////////////////////
// Cache Population - called immediately after body 11 is rendered
///////////////////////////////////////////////////////////////////////////////

// Populate cache from primTable[] while it still contains body 11's primitives
void populateLampPrimitiveCache()
{
    extern u32 positionInPrimEntry;
    extern primEntryStruct primTable[];

    s_lampPrimCache.clear();

    for (u32 i = 0; i < positionInPrimEntry && s_lampPrimCache.count < LampPrimitiveCache::MAX_LAMP_PRIMS; i++)
    {
        const primEntryStruct& prim = primTable[i];

        // Cache ramp polygons, textured polygons, and spheres (lamp components)
        bool isRampPoly = (prim.type == 1 && prim.isRampPrim);
        bool isTexturedPoly = (prim.type == 9 || prim.type == 10);
        bool isSphere = (prim.type == 3);

        if ((isRampPoly || isTexturedPoly || isSphere) && !prim.nearClipped && prim.numOfVertices >= 1 && prim.numOfVertices <= 8)
        {
            LampPrimitiveCache::CachedPrim& cached = s_lampPrimCache.prims[s_lampPrimCache.count];

            cached.originalPrimIndex = prim.originalPrimIndex;
            cached.type = prim.type;
            cached.material = prim.material;
            cached.color = prim.color;  // Cache color for rendering mask
            cached.isRampPrim = prim.isRampPrim;
            cached.nearClipped = prim.nearClipped;
            cached.numOfVertices = prim.numOfVertices;

            // Copy vertices and calculate centroid
            float sumX = 0.0f, sumY = 0.0f;
            for (int v = 0; v < prim.numOfVertices; v++)
            {
                cached.vertices[v].X = prim.vertices[v].X;
                cached.vertices[v].Y = prim.vertices[v].Y;
                cached.vertices[v].Z = prim.vertices[v].Z;
                sumX += prim.vertices[v].X;
                sumY += prim.vertices[v].Y;
            }
            cached.centerX = sumX / prim.numOfVertices;
            cached.centerY = sumY / prim.numOfVertices;

            s_lampPrimCache.count++;
        }
    }

    s_lampPrimCache.isValid = (s_lampPrimCache.count > 0);

    if (s_lampPrimCache.isValid)
    {
        static int debugCacheCount = 0;
        if (debugCacheCount++ % 120 == 0)
        {
            printf("[LANTERN-DEBUG] Cached %d lamp primitives from body 11\n", s_lampPrimCache.count);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
// Initialization
///////////////////////////////////////////////////////////////////////////////

void initLanternLighting()
{
    printf("[LANTERN] Initializing lantern lighting system\n");

    // Load bloom shader for lantern glow
    s_lanternBloomShader = loadBgfxProgram("lantern_bloom_vs", "lantern_bloom_ps");
    if (!bgfx::isValid(s_lanternBloomShader))
    {
        printf("[LANTERN-WARN] Failed to load lantern bloom shader\n");
    }

    // Load shadow shader for lantern light casting
    s_lanternShadowShader = loadBgfxProgram("lantern_shadow_vs", "lantern_shadow_ps");
    if (!bgfx::isValid(s_lanternShadowShader))
    {
        printf("[LANTERN-WARN] Failed to load lantern shadow shader\n");
    }

    // Create uniforms
    u_lanternPos = bgfx::createUniform("u_lanternPos", bgfx::UniformType::Vec4);
    u_lanternColor = bgfx::createUniform("u_lanternColor", bgfx::UniformType::Vec4);
    u_lanternIntensity = bgfx::createUniform("u_lanternIntensity", bgfx::UniformType::Vec4);
    u_lanternRadius = bgfx::createUniform("u_lanternRadius", bgfx::UniformType::Vec4);

    printf("[LANTERN] Lantern lighting system initialized\n");
}

void shutdownLanternLighting()
{
    if (bgfx::isValid(s_lanternBloomShader))
    {
        bgfx::destroy(s_lanternBloomShader);
        s_lanternBloomShader = BGFX_INVALID_HANDLE;
    }

    if (bgfx::isValid(s_lanternShadowShader))
    {
        bgfx::destroy(s_lanternShadowShader);
        s_lanternShadowShader = BGFX_INVALID_HANDLE;
    }

    if (bgfx::isValid(u_lanternPos))
    {
        bgfx::destroy(u_lanternPos);
        u_lanternPos = BGFX_INVALID_HANDLE;
    }

    if (bgfx::isValid(u_lanternColor))
    {
        bgfx::destroy(u_lanternColor);
        u_lanternColor = BGFX_INVALID_HANDLE;
    }

    if (bgfx::isValid(u_lanternIntensity))
    {
        bgfx::destroy(u_lanternIntensity);
        u_lanternIntensity = BGFX_INVALID_HANDLE;
    }

    if (bgfx::isValid(u_lanternRadius))
    {
        bgfx::destroy(u_lanternRadius);
        u_lanternRadius = BGFX_INVALID_HANDLE;
    }

    g_lanternStates.clear();
}

///////////////////////////////////////////////////////////////////////////////
// Lantern State Management
///////////////////////////////////////////////////////////////////////////////

bool isLanternWithOil(int objectIdx)
{
    if (g_lanternStates.find(objectIdx) == g_lanternStates.end())
        return false;

    return g_lanternStates[objectIdx].hasOil && 
           g_lanternStates[objectIdx].oilLevel > 0.0f;
}

LanternState* getLanternState(int objectIdx)
{
    if (g_lanternStates.find(objectIdx) == g_lanternStates.end())
        return nullptr;

    return &g_lanternStates[objectIdx];
}

void setLanternOil(int objectIdx, bool hasOil, float oilLevel)
{
    LanternState& state = g_lanternStates[objectIdx];

    float clampedLevel = (oilLevel < 0.0f) ? 0.0f : (oilLevel > 1.0f) ? 1.0f : oilLevel;

    // Track if this is a state change (on/off) vs oil level change
    bool stateChanged = (state.hasOil != hasOil);
    bool oilLevelChanged = (state.oilLevel != clampedLevel);

    if (stateChanged)
    {
        if (hasOil)
            printf("[LANTERN] Lantern #%d lit\n", objectIdx);
        else
            printf("[LANTERN] Lantern #%d extinguished\n", objectIdx);

        state.hasOil = hasOil;
    }

    // Always update oil level and glow intensity when oil level changes
    if (stateChanged || oilLevelChanged)
    {
        state.oilLevel = clampedLevel;

        // Scale glow intensity with oil level (0.0 = no glow, 1.0 = full glow)
        // When oil is present: brightness scales linearly with oil level
        if (hasOil && clampedLevel > 0.0f)
        {
            state.glowIntensity = clampedLevel;  // Direct scaling: 0% oil = 0% brightness, 100% oil = 100% brightness
        }
        else
        {
            state.glowIntensity = 0.0f;
        }

        // Scale light radius with oil level for more dramatic effect
        // Full oil = LANTERN_BASE_RADIUS, empty = 50% of base radius
        if (hasOil && clampedLevel > 0.0f)
        {
            state.lightRadius = LANTERN_BASE_RADIUS * (0.5f + clampedLevel * 0.5f);
        }
        else
        {
            state.lightRadius = LANTERN_BASE_RADIUS * 0.5f;
        }

        state.glowColorR = LANTERN_GLOW_R;
        state.glowColorG = LANTERN_GLOW_G;
        state.glowColorB = LANTERN_GLOW_B;

        if (oilLevelChanged && state.hasOil)
        {
            printf("[LANTERN] Lantern #%d oil level: %.1f%%\n", objectIdx, clampedLevel * 100.0f);
        }
    }
}

void setLanternInHand(int objectIdx, bool inHand)
{
    auto it = g_lanternStates.find(objectIdx);
    if (it == g_lanternStates.end())
    {
        // Initialize new lantern state
        LanternState newState = {};
        newState.hasOil = false;
        newState.oilLevel = 0.0f;
        newState.glowIntensity = 0.0f;
        newState.glowColorR = LANTERN_GLOW_R;
        newState.glowColorG = LANTERN_GLOW_G;
        newState.glowColorB = LANTERN_GLOW_B;
        newState.lightPosX = 0.0f;
        newState.lightPosY = 0.0f;
        newState.lightPosZ = 0.0f;
        newState.lightRadius = LANTERN_BASE_RADIUS;
        newState.isInHand = inHand;
        g_lanternStates[objectIdx] = newState;

        if (inHand)
        {
            printf("[LANTERN] Lantern #%d picked up\n", objectIdx);
            // Reset primitive tracker when picking up
            s_lampTracker.reset();
            // Enable glow to fade in when lantern is picked up (if it has oil)
            // The fade will interpolate from current value to 1.0 over time
            s_glowFade.setTarget(1.0f);
            s_allowGlowFadeIn = true;
            s_fadeInTimeRemaining = FADE_IN_GRACE_PERIOD;
        }
    }
    else if (it->second.isInHand != inHand)
    {
        // Only update and log when state actually changes
        it->second.isInHand = inHand;
        printf("[LANTERN] Lantern #%d %s\n", objectIdx, inHand ? "picked up" : "put down");

        // Reset primitive tracker and cache on state change
        s_lampTracker.reset();
        if (inHand)
        {
            // Enable glow to fade in when lantern is picked up
            s_glowFade.setTarget(1.0f);
            s_allowGlowFadeIn = true;
            s_fadeInTimeRemaining = FADE_IN_GRACE_PERIOD;
        }
        else
        {
            // Clear cache when lamp is dropped
            s_lampPrimCache.clear();
            // Reset fade state when dropping lamp
            s_glowFade.reset();
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
// Flicker Effect Calculation
///////////////////////////////////////////////////////////////////////////////

// Simplified noise function using sine waves for realistic flame flicker
// Creates natural-looking flame behavior with multiple frequency components
static float calculateFlameFlicker(float time)
{
    // Use multiple sine waves at different frequencies to create organic-looking flicker
    // This simulates the natural variation in flame intensity

    // Primary flicker: slow wave (primary flame breathing)
    float slow = sinf(time * 2.5f);

    // Secondary flicker: faster wave (flame licking)
    float fast = sinf(time * 7.3f);

    // Tertiary flicker: random-like movement (turbulent flame)
    float veryFast = sinf(time * 13.7f + cosf(time * 3.2f));

    // Combine the waves with different weights
    // Slow component gives the breathing effect, fast adds flicker detail
    float combined = (slow * 0.5f) + (fast * 0.3f) + (veryFast * 0.2f);

    // Normalize to 0.7-1.0 range for realistic flicker
    // Flame stays lit but varies in intensity
    float flicker = 0.85f + combined * 0.15f;

    // Clamp to valid range
    return (flicker < 0.0f) ? 0.0f : (flicker > 1.0f) ? 1.0f : flicker;
}

///////////////////////////////////////////////////////////////////////////////
// Light Flare Effect Calculation
///////////////////////////////////////////////////////////////////////////////

// Calculate flare visibility based on camera view direction and lantern position
// Returns visibility factor (0.0 = not in view, 1.0 = directly looking at lantern)
static float calculateFlareVisibility(float screenX, float screenY)
{
    // Flare is most visible when lantern is near screen center
    // Calculate distance from center of screen (0.5, 0.5)
    float centerX = 0.5f;
    float centerY = 0.5f;

    float dx = screenX - centerX;
    float dy = screenY - centerY;
    float distFromCenter = std::sqrt(dx * dx + dy * dy);

    // Visibility falls off from center with smooth falloff
    // Max visibility at center, zero visibility at screen edges
    float visibility = 1.0f - (distFromCenter * 1.5f);  // 1.5 provides falloff beyond center
    return (visibility < 0.0f) ? 0.0f : (visibility > 1.0f) ? 1.0f : visibility;
}

// Animate lens flare properties (pulsing glow, etc.)
static void updateFlareAnimation(float& flarePulse, float deltaTime)
{
    static float pulseAccum = 0.0f;
    pulseAccum += deltaTime;

    // Create a pulsing effect at 2 Hz
    flarePulse = 0.5f + 0.5f * sinf(pulseAccum * 2.0f * 3.14159f);
}

// Forward declarations for shadow occlusion functions
static float calculateShadowOcclusion(const LanternState& lantern, float targetX, float targetY, float targetZ);
static float calculateDepthShadowFilter(float objectY, float lanternY, float playerY);
static void updateShadowOcclusion(LanternState& lantern);
static void updateBackgroundLighting(LanternState& lantern, float playerX, float playerY, float playerZ);

// Convert world position to screen space for flare visibility detection
// Requires camera/view matrix calculations - uses existing rendering context
static bool worldToScreenSpace(float worldX, float worldY, float worldZ,
                               float& outScreenX, float& outScreenY)
{
    // This would require access to the camera matrix from the rendering system
    // For now, return false to indicate not on-screen
    // In practice, this would be filled in with actual camera math
    // or obtained from the rendering system's context
    return false;
}

///////////////////////////////////////////////////////////////////////////////
// Lighting Updates
///////////////////////////////////////////////////////////////////////////////

void updateLanternLighting()
{
    extern std::array<tObject, NUM_MAX_OBJECT> ListObjets;
    extern std::vector<tWorldObject> ListWorldObjets;

    for (auto& pair : g_lanternStates)
    {
        int worldObjIdx = pair.first;  // This is a world object index (into ListWorldObjets)
        LanternState& state = pair.second;

        // Update flicker effect
        if (state.hasOil && state.glowIntensity > 0.0f)
        {
            state.flickerTime += 0.016f;  // Assume ~60 FPS, ~16.67ms per frame
            state.flickerIntensity = calculateFlameFlicker(state.flickerTime);
            // Apply flicker to glow intensity (dampens/brightens the base glow)
            state.effectiveGlowIntensity = state.glowIntensity * state.flickerIntensity;
        }
        else
        {
            state.flickerIntensity = 0.0f;
            state.effectiveGlowIntensity = 0.0f;
            state.flickerTime = 0.0f;  // Reset flicker time when lantern is off
        }

        // Update lens flare effect (for in-hand lanterns only)
        if (state.isInHand && state.hasOil && state.effectiveGlowIntensity > 0.0f)
        {
            // Lens flare is most visible when looking directly at lantern
            // For in-hand lanterns, approximate visibility based on screen center
            // (Full implementation would use actual camera direction)
            static float flareScreenX = 0.5f;  // Center screen
            static float flareScreenY = 0.5f;

            state.flareScreenX = flareScreenX;
            state.flareScreenY = flareScreenY;

            // Calculate flare visibility based on position
            state.flareIntensity = calculateFlareVisibility(state.flareScreenX, state.flareScreenY);

            // Scale flare with oil level and effective glow
            state.flareIntensity *= state.effectiveGlowIntensity * state.oilLevel;

            // Fade in/out smoothly
            float targetAlpha = state.flareIntensity;
            state.flareAlpha = state.flareAlpha * 0.9f + targetAlpha * 0.1f;  // Smooth lerp

            // Update flare pulse animation
            updateFlareAnimation(state.flarePulse, 0.016f);

            state.isFlareVisible = (state.flareAlpha > 0.01f);
        }
        else
        {
            state.isFlareVisible = false;
            state.flareAlpha = 0.0f;
            state.flareIntensity = 0.0f;
        }

        // Update shadow occlusion for dynamic object shadows
        updateShadowOcclusion(state);

        // Update background lighting when in-hand
        if (state.isInHand)
        {
            s16 heroSlot = currentCameraTargetActor;
            if (heroSlot < 0 || heroSlot >= NUM_MAX_OBJECT)
                heroSlot = 0;
            tObject& player = ListObjets[heroSlot];

            updateBackgroundLighting(state, 
                                   static_cast<float>(player.worldX),
                                   static_cast<float>(player.worldY),
                                   static_cast<float>(player.worldZ));
        }

        if (state.isInHand)
        {
            // When held, the lantern moves with the player. The hero actor
            // slot is NOT always 0 in FITD — it's tracked globally as
            // currentCameraTargetActor (slot 0 in some rooms is e.g. a door).
            s16 heroSlot = currentCameraTargetActor;
            if (heroSlot < 0 || heroSlot >= NUM_MAX_OBJECT)
                heroSlot = 0;  // safety fallback only
            tObject& player = ListObjets[heroSlot];
            state.lightPosX = static_cast<float>(player.worldX);
            state.lightPosY = static_cast<float>(player.worldY + 50);  // Slight upward offset
            state.lightPosZ = static_cast<float>(player.worldZ);
            state.lightRadius = LANTERN_BASE_RADIUS * (0.8f + state.oilLevel * 0.4f);
        }
        else if (worldObjIdx >= 0 && worldObjIdx < (int)ListWorldObjets.size())
        {
            // When placed, look up the actor slot via tWorldObject.objIndex
            s16 actorSlot = ListWorldObjets[worldObjIdx].objIndex;
            if (actorSlot >= 0 && actorSlot < NUM_MAX_OBJECT)
            {
                tObject& obj = ListObjets[actorSlot];
                state.lightPosX = static_cast<float>(obj.worldX);
                state.lightPosY = static_cast<float>(obj.worldY);
                state.lightPosZ = static_cast<float>(obj.worldZ);
            }
            state.lightRadius = LANTERN_BASE_RADIUS * 0.6f;
        }
    }

    // Push the active lantern lighting state to the HD background shader.
    //
    // FIX 1 - FLICKER: flareScreenX/Y is an unstable animation value that jumps
    // every frame as the lamp primitive is found/lost during movement. We maintain
    // a smoothed screen position that lerps slowly toward the flare when it has
    // high confidence (flareAlpha), and stays at the in-hand default (0.5, 0.65)
    // otherwise. This produces stable, non-flickering background lighting.
    //
    // FIX 3 - LANTERN STAYS ON: gate the entire effect (including influence) on
    // s_glowFade.currentFade, which is reliably zeroed by s_glowFade.reset() in
    // setLanternInHand(false). isInHand alone is not reliable across all drop paths.
    {
        static float s_bgLightSmoothX         = 0.5f;
        static float s_bgLightSmoothY         = 0.65f;
        static float s_bgLightSmoothIntensity = 0.0f;
        static float s_bgLightSmoothInfluence = 0.0f;

        const LanternState* activeLantern = nullptr;
        for (auto& pair : g_lanternStates)
        {
            const LanternState& state = pair.second;
            if (state.isInHand && state.hasOil && state.effectiveGlowIntensity > 0.001f)
            {
                if (!activeLantern || state.effectiveGlowIntensity > activeLantern->effectiveGlowIntensity)
                    activeLantern = &state;
            }
        }

        // Gate background lighting on lantern presence only — NOT on the glow fade.
        // s_glowFade goes to 0 when facing away from camera (to hide the glow overlay),
        // but the background lighting should remain stable regardless of facing direction.
        // The glow overlay (rendered via ImGui) uses s_glowFade independently.
        float fade = s_glowFade.currentFade;

        if (activeLantern)
        {
            // Smooth the screen position: only pull toward flare pos when the flare
            // has enough opacity to trust it (flareAlpha > 0.25). Otherwise hold
            // the smoothed position steady to avoid jitter during movement.
            const float LERP_FAST = 0.06f;   // pull rate when flare is visible
            const float LERP_SLOW = 0.015f;  // drift rate back to default when not
            float defaultX = 0.5f;
            float defaultY = 0.65f;

            if (activeLantern->isFlareVisible && activeLantern->flareAlpha > 0.25f)
            {
                s_bgLightSmoothX += (activeLantern->flareScreenX - s_bgLightSmoothX) * LERP_FAST;
                s_bgLightSmoothY += (activeLantern->flareScreenY - s_bgLightSmoothY) * LERP_FAST;
            }
            else
            {
                s_bgLightSmoothX += (defaultX - s_bgLightSmoothX) * LERP_SLOW;
                s_bgLightSmoothY += (defaultY - s_bgLightSmoothY) * LERP_SLOW;
            }

            // Background lighting is NOT gated by glow fade: when the character faces
            // away from the camera, s_glowFade drops to 0 to hide the glow overlay,
            // but the lantern still illuminates the background regardless of facing.
            const float LERP_VALUE = 0.08f;
            float targetIntensity = activeLantern->effectiveGlowIntensity;
            float targetInfluence = activeLantern->backgroundLightInfluence;
            s_bgLightSmoothIntensity += (targetIntensity - s_bgLightSmoothIntensity) * LERP_VALUE;
            s_bgLightSmoothInfluence += (targetInfluence - s_bgLightSmoothInfluence) * LERP_VALUE;

            float lightRadius = 0.38f * (0.8f + activeLantern->oilLevel * 0.4f);

            // In SD mode: pass influence=0 and ambientDarken=1.0 so the shader
            // never darkens or tints non-HD backgrounds/overlays.
            const float ambientDarken = g_currentBackgroundIsHD ? 0.55f : 1.0f;
            setHDBackgroundLanternUniforms(
                s_bgLightSmoothX, s_bgLightSmoothY, s_bgLightSmoothIntensity,
                LANTERN_GLOW_R, LANTERN_GLOW_G, LANTERN_GLOW_B,
                g_currentBackgroundIsHD ? s_bgLightSmoothInfluence : 0.0f,
                lightRadius,
                ambientDarken
            );
        }
        else
        {
            // No active lantern — zero everything immediately.
            s_bgLightSmoothX         = 0.5f;
            s_bgLightSmoothY         = 0.65f;
            s_bgLightSmoothIntensity = 0.0f;
            s_bgLightSmoothInfluence = 0.0f;

            setHDBackgroundLanternUniforms(0.5f, 0.65f, 0.0f, 1.0f, 0.75f, 0.25f, 0.0f, 0.38f, 1.0f);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
// Shader Ac
///////////////////////////////////////////////////////////////////////////////

bgfx::ProgramHandle getLanternBloomShader()
{
    return s_lanternBloomShader;
}

///////////////////////////////////////////////////////////////////////////////
// Shadow Occlusion and Background Lighting
///////////////////////////////////////////////////////////////////////////////

// Calculate shadow occlusion based on object positions between lantern and background
// Returns occlusion factor (0.0 = no occlusion, 1.0 = fully occluded)
static float calculateShadowOcclusion(const LanternState& lantern, float targetX, float targetY, float targetZ)
{
    // Simple distance-based occlusion: objects closer to lantern cast stronger shadows
    float dx = targetX - lantern.lightPosX;
    float dy = targetY - lantern.lightPosY;
    float dz = targetZ - lantern.lightPosZ;

    float distToTarget = std::sqrt(dx * dx + dy * dy + dz * dz);

    // Shadow strength decreases with distance from lantern
    // Objects very close to lantern cast full shadows
    // Objects far away cast minimal shadows
    float shadowFalloff = 1.0f - (distToTarget / lantern.lightRadius);

    if (shadowFalloff < 0.0f)
        shadowFalloff = 0.0f;

    return shadowFalloff;
}

// Calculate shadow filter based on depth (Y position)
// Objects higher up cast softer, less defined shadows
static float calculateDepthShadowFilter(float objectY, float lanternY, float playerY)
{
    // Calculate relative heights
    float objectHeightAboveGround = objectY - playerY;
    float lanternHeightAboveGround = lanternY - playerY;

    // Objects much higher than lantern cast very soft shadows
    float heightDifference = lanternHeightAboveGround - objectHeightAboveGround;

    // Shadow hardness based on height difference
    // Close heights = hard shadow, large difference = soft shadow
    float shadowHardness = 1.0f - (heightDifference / (lanternHeightAboveGround * 2.0f));

    if (shadowHardness < 0.1f)
        shadowHardness = 0.1f;
    if (shadowHardness > 1.0f)
        shadowHardness = 1.0f;

    return shadowHardness;
}

// Update shadow occlusion for all objects in the scene
static void updateShadowOcclusion(LanternState& lantern)
{
    extern std::array<tObject, NUM_MAX_OBJECT> ListObjets;

    lantern.occludingObjectCount = 0;
    lantern.shadowIntensity = 0.0f;

    if (!lantern.hasOil || lantern.effectiveGlowIntensity <= 0.0f)
        return;

    // Check all objects for occlusion
    float maxShadow = 0.0f;
    int shadowCount = 0;

    for (int i = 0; i < NUM_MAX_OBJECT; ++i)
    {
        const tObject& obj = ListObjets[i];

        // Skip lantern holder and inactive objects
        if (i == 0 || obj.worldX == 0 && obj.worldY == 0 && obj.worldZ == 0)
            continue;

        // Calculate shadow contribution from this object
        float occlusion = calculateShadowOcclusion(lantern, 
                                                   static_cast<float>(obj.worldX),
                                                   static_cast<float>(obj.worldY),
                                                   static_cast<float>(obj.worldZ));

        if (occlusion > 0.1f)
        {
            // Apply depth-based shadow filtering for more realistic shadows
            float depthFilter = calculateDepthShadowFilter(static_cast<float>(obj.worldY),
                                                          lantern.lightPosY,
                                                          lantern.lightPosY);

            occlusion *= depthFilter;

            shadowCount++;
            maxShadow = (occlusion > maxShadow) ? occlusion : maxShadow;
        }
    }

    lantern.occludingObjectCount = shadowCount;
    lantern.shadowIntensity = maxShadow * lantern.effectiveGlowIntensity;  // Scale with glow
}

// Calculate background lighting based on lantern position and intensity
static void updateBackgroundLighting(LanternState& lantern, float playerX, float playerY, float playerZ)
{
    if (!lantern.hasOil || lantern.effectiveGlowIntensity <= 0.0f)
    {
        lantern.backgroundLightInfluence = 0.0f;
        lantern.lightBrightness = 0.0f;
        lantern.lightDarkness = 0.0f;
        return;
    }

    // Calculate distance from lantern to player/background
    float dx = lantern.lightPosX - playerX;
    float dy = lantern.lightPosY - playerY;
    float dz = lantern.lightPosZ - playerZ;

    float distToBackground = std::sqrt(dx * dx + dy * dy + dz * dz);

    // Light influence falls off with distance
    float influence = 1.0f - (distToBackground / (lantern.lightRadius * 2.0f));
    lantern.backgroundLightInfluence = (influence < 0.0f) ? 0.0f : (influence > 1.0f) ? 1.0f : influence;

    // Calculate illumination and shadow effects
    lantern.lightBrightness = lantern.effectiveGlowIntensity * 0.8f;  // Brighten lit areas
    lantern.lightDarkness = lantern.effectiveGlowIntensity * 0.4f;    // Slightly darken unlit areas
}

///////////////////////////////////////////////////////////////////////////////
// Lighting Effects
///////////////////////////////////////////////////////////////////////////////

void applyLanternBloom(bgfx::TextureHandle lanternTexture, const LanternState& lantern)
{
    if (!bgfx::isValid(s_lanternBloomShader))
        return;

    if (lantern.effectiveGlowIntensity <= 0.0f)
        return;

    // Set bloom shader uniforms
    // Use effectiveGlowIntensity which includes flicker effect
    float posUniform[4] = { lantern.lightPosX, lantern.lightPosY, lantern.lightPosZ, 1.0f };
    float colorUniform[4] = { lantern.glowColorR, lantern.glowColorG, lantern.glowColorB, lantern.effectiveGlowIntensity };

    bgfx::setUniform(u_lanternPos, posUniform);
    bgfx::setUniform(u_lanternColor, colorUniform);
    bgfx::setUniform(u_lanternIntensity, colorUniform);

    bgfx::setTexture(0, bgfx::UniformHandle{0}, lanternTexture);
}

void applyLanternLightingToBackground(bgfx::TextureHandle backgroundTexture,
                                       const LanternState& lantern,
                                       float playerX, float playerY, float playerZ)
{
    if (!bgfx::isValid(s_lanternShadowShader))
        return;

    if (lantern.effectiveGlowIntensity <= 0.0f)
        return;

    // Calculate distance from lantern to background
    // The closer the light source, the stronger the effect
    float dx = lantern.lightPosX - playerX;
    float dy = lantern.lightPosY - playerY;
    float dz = lantern.lightPosZ - playerZ;
    float distToPlayer = std::sqrt(dx*dx + dy*dy + dz*dz);
    float distAttenuation = 1.0f - (distToPlayer / lantern.lightRadius);
    if (distAttenuation < 0.0f) distAttenuation = 0.0f;
    if (distAttenuation > 1.0f) distAttenuation = 1.0f;

    // Apply shadow occlusion to reduce background lighting
    // Occluded areas get darkened more by shadows
    float occlusionFactor = 1.0f - (lantern.shadowIntensity * 0.5f);
    float effectiveIntensity = lantern.effectiveGlowIntensity * occlusionFactor;

    // Apply background lighting influence
    // This affects how much the lantern light brightens the background
    float backgroundInfluence = lantern.backgroundLightInfluence * distAttenuation;

    // Enhanced color with lighting/shadow effects
    // Bright lantern illuminates warm areas, shadows darken cool areas
    float brightBoost = lantern.lightBrightness * backgroundInfluence;
    float shadowEffect = lantern.lightDarkness * (1.0f - backgroundInfluence);

    // Set shadow shader uniforms with enhanced lighting
    float posUniform[4] = { lantern.lightPosX, lantern.lightPosY, lantern.lightPosZ, distAttenuation };

    // Color with brightness boost in illuminated areas
    float colorUniform[4] = { 
        lantern.glowColorR * (1.0f + brightBoost), 
        lantern.glowColorG * (1.0f + brightBoost), 
        lantern.glowColorB * (1.0f + brightBoost), 
        effectiveIntensity 
    };

    // Radius with occlusion factor
    float radiusUniform[4] = { lantern.lightRadius * (1.0f - shadowEffect), 0.0f, 0.0f, shadowEffect };

    bgfx::setUniform(u_lanternPos, posUniform);
    bgfx::setUniform(u_lanternColor, colorUniform);
    bgfx::setUniform(u_lanternRadius, radiusUniform);

    bgfx::setTexture(0, bgfx::UniformHandle{0}, backgroundTexture);
}

///////////////////////////////////////////////////////////////////////////////
// Color-Based Lamp Detection
///////////////////////////////////////////////////////////////////////////////

// Dynamic lamp position detection system:
// Instead of using hardcoded offsets, this system finds the lamp texture's actual 
// rendered position by:
// 1. Searching the primitive table for textured/ramp polygons
// 2. Identifying the lamp texture polygons (from body 11 when held)
// 3. Calculating the centroid of the lamp's rendered polygons
//
// This automatically adapts to:
// - Player movement and rotation
// - Different camera angles
// - Animation frames
// - Screen resolution scaling
//
// The glow will always render exactly where the lamp texture appears on screen.

// External primitive table access (from renderer.cpp / vars.h)
// primTypeEnum: Line=0, Poly=1, Point=2, Sphere=3, Disk=4, Cylinder=5...
// Textured poly types: 9, 10

// Find lamp texture position by searching the primitive render table
// Returns true if lamp texture was found, with centroid in outX, outY (screen space)
static bool findLampColorPosition(float& outX, float& outY)
{
    // USE CACHE INSTEAD OF primTable[]!
    // The cache was populated during body 11 rendering and persists even after
    // other bodies (enemies, doors, etc.) overwrite primTable[] during depth-sorted rendering.
    // This solves the race condition where closer objects render later, making
    // g_currentPrimTableBodyNum != 11 when we need to find the lamp position.

    extern std::array<tObject, NUM_MAX_OBJECT> ListObjets;

    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    if (displaySize.x <= 0.0f || displaySize.y <= 0.0f)
        return false;

    float scaleX = displaySize.x / 320.0f;
    float scaleY = displaySize.y / 200.0f;

    // Get the player actor
    s16 heroSlot = currentCameraTargetActor;
    if (heroSlot < 0 || heroSlot >= NUM_MAX_OBJECT)
        heroSlot = 0;

    const tObject& player = ListObjets[heroSlot];

    // Get player's current body number
    // Body 11 is held lit lamp (for both LISTBODY/Carny and LISTBOD2/Emily)
    bool playerHasLamp = (player.bodyNum == LAMP_BODY_NUM);

    // If player doesn't have lamp body, don't search for lamp primitives
    if (!playerHasLamp)
    {
        // Reset tracker if player no longer has lamp body
        if (s_lampTracker.hasValidCache)
        {
            printf("[LANTERN-DEBUG] Player bodyNum changed from lamp (%d != %d), resetting tracker\n",
                   player.bodyNum, LAMP_BODY_NUM);
            s_lampTracker.reset();
        }
        return false;
    }

    // Calculate player center in screen space
    // Note: screenXMin/Max can be unreliable when player is partially off-screen or
    // during certain animations, so we calculate a robust center point.
    float playerCX = (player.screenXMin + player.screenXMax) * 0.5f;
    float playerCY = (player.screenYMin + player.screenYMax) * 0.5f;

    // Validate player screen bounds (sometimes they can be invalid: -1 or extreme values)
    bool hasValidScreenBounds = (player.screenXMin >= 0 && player.screenXMax >= 0 &&
                                  player.screenYMin >= 0 && player.screenYMax >= 0 &&
                                  player.screenXMin <= 320.0f && player.screenXMax <= 320.0f &&
                                  player.screenYMin <= 200.0f && player.screenYMax <= 200.0f);

    // If screen bounds are invalid, skip proximity filtering entirely and search all candidates
    bool useProximityFilter = hasValidScreenBounds;

    if (!hasValidScreenBounds)
    {
        static int debugInvalidBoundsCount = 0;
        if (debugInvalidBoundsCount++ % 60 == 0)
        {
            printf("[LANTERN-DEBUG] Player screen bounds invalid (%.1f,%.1f)-(%.1f,%.1f), skipping proximity filter\n",
                   player.screenXMin, player.screenYMin, player.screenXMax, player.screenYMax);
        }
    }

    const int LAMP_BODY_NUM = 11;

    // If cache isn't valid, we don't have lamp primitives to search
    if (!s_lampPrimCache.isValid)
    {
        static int debugNoCacheCount = 0;
        if (debugNoCacheCount++ % 60 == 0)
        {
            printf("[LANTERN-DEBUG] Lamp primitive cache not valid\n");
        }
        return false;
    }

    // STEP 1: If we have a sticky-tracked lamp primitive, try to find it first in the cache
    if (s_lampTracker.hasValidCache)
    {
        for (int i = 0; i < s_lampPrimCache.count; i++)
        {
            const LampPrimitiveCache::CachedPrim& prim = s_lampPrimCache.prims[i];

            // Look for our cached primitive by originalPrimIndex
            if (prim.originalPrimIndex == s_lampTracker.cachedOriginalPrimIndex)
            {
                // Skip if clipped
                if (prim.nearClipped)
                    continue;

                // Found our cached primitive - use it!
                outX = prim.centerX * scaleX;
                outY = prim.centerY * scaleY;

                // Update last known position
                s_lampTracker.lastKnownX = outX;
                s_lampTracker.lastKnownY = outY;
                s_lampTracker.missFrameCount = 0;

                static int debugTrackedCount = 0;
                if (debugTrackedCount++ % 120 == 0)
                {
                    printf("[LANTERN-DEBUG] Tracking cached prim #%d at screen=(%.1f,%.1f)\n",
                           s_lampTracker.cachedOriginalPrimIndex, outX, outY);
                }

                return true;
            }
        }

        // Cached primitive not found - immediately stop rendering and reset
        printf("[LANTERN-DEBUG] Cached prim #%d lost, resetting tracker\n",
               s_lampTracker.cachedOriginalPrimIndex);
        s_lampTracker.reset();
        // Fall through to search for new primitive
    }

    // STEP 2: No cached primitive (or cache expired) - search for a new one
    // When the lamp is held, it's part of the player's body rendering
    // Different character models (LISTBODY, LISTBOD2) may use different primitive types

    int bestPrimIndex = -1;
    int bestOriginalPrimIndex = -1;
    float bestX = 0.0f, bestY = 0.0f;
    float bestDist = 999999.0f;

    // Search cached lamp primitives instead of primTable[]
    // Cache persists even after other bodies overwrite primTable[] during depth-sorted rendering
    for (int i = 0; i < s_lampPrimCache.count; i++)
    {
        const LampPrimitiveCache::CachedPrim& prim = s_lampPrimCache.prims[i];

        // Skip clipped primitives
        if (prim.nearClipped)
            continue;

        // Use pre-calculated centroid from cache
        float polyX = prim.centerX;
        float polyY = prim.centerY;

            // Filter by proximity to player (only if we have valid screen bounds)
            float dx = polyX - playerCX;
            float dy = polyY - playerCY;
            float distFromPlayer = sqrt(dx*dx + dy*dy);

            // If player screen bounds are valid, use proximity filtering.
            // If invalid (e.g., player partially off-screen), accept all lamp-type primitives.
            bool passesProximityCheck = !useProximityFilter || 
                (distFromPlayer < 60.0f && dy > -20.0f && dy < 50.0f && abs(dx) < 45.0f);

            if (passesProximityCheck)
            {
                // Debug: log candidate primitives
                static int debugPolyCount = 0;
                if (debugPolyCount++ % 180 == 0)  // Log every 3 seconds
                {
                    printf("[LANTERN-DEBUG] Candidate: cacheIdx=%d, origIdx=%d, type=%d, ramp=%d, material=%d, pos=(%.1f,%.1f), dist=%.1f\n",
                           i, prim.originalPrimIndex, prim.type, prim.isRampPrim, prim.material, polyX, polyY, distFromPlayer);
                }

                        // Pick the primitive closest to player center as our target
                        if (distFromPlayer < bestDist)
                        {
                            bestDist = distFromPlayer;
                            bestPrimIndex = i;
                            bestOriginalPrimIndex = prim.originalPrimIndex;
                            bestX = polyX;
                            bestY = polyY;
                        }
                    }
                }

    if (bestPrimIndex >= 0)
    {
        // Found a lamp primitive - lock onto it!
        s_lampTracker.cachedOriginalPrimIndex = bestOriginalPrimIndex;
        s_lampTracker.hasValidCache = true;
        s_lampTracker.missFrameCount = 0;

        outX = bestX * scaleX;
        outY = bestY * scaleY;

        s_lampTracker.lastKnownX = outX;
        s_lampTracker.lastKnownY = outY;

        printf("[LANTERN-DEBUG] Locked onto prim #%d (origIdx=%d) at game=(%.1f,%.1f) screen=(%.1f,%.1f), dist=%.1f\n",
               bestPrimIndex, bestOriginalPrimIndex, bestX, bestY, outX, outY, bestDist);

        return true;
    }

    // No lamp primitives found at all
    static int debugMissCount = 0;
    if (debugMissCount++ % 60 == 0)
    {
        printf("[LANTERN-DEBUG] No lamp prims found (player at %.1f,%.1f, %d total prims)\n",
               playerCX, playerCY, positionInPrimEntry);
    }

    return false;
}

///////////////////////////////////////////////////////////////////////////////
// Fullscreen Glow Render with Depth Occlusion via Masks
///////////////////////////////////////////////////////////////////////////////

// Helper function to render a filled circle using ImGui
static void renderFilledCircle(float centerX, float centerY, float radius,
                               float r, float g, float b, float a,
                               int segments);

// Helper function to render player silhouette mask on top of glow (for occlusion)
static void renderPlayerMask();

// Update fade state based on occlusion and visibility
// Called each frame to smoothly transition glow intensity
// Render lens flare effect at specified screen position
static void renderLensFlareStar(float screenX, float screenY, float intensity, float alpha,
                               float r, float g, float b)
{
    // Lens flare star pattern: 4-pointed star with glow
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    ImVec2 displaySize = ImGui::GetIO().DisplaySize;

    if (!drawList || displaySize.x <= 0.0f || displaySize.y <= 0.0f)
        return;

    // Scale coordinates to display resolution
    float scaleX = displaySize.x / 320.0f;
    float scaleY = displaySize.y / 200.0f;

    float screenPosX = screenX * scaleX;
    float screenPosY = screenY * scaleY;

    // Flare size scales with intensity
    float starSize = intensity * 8.0f;

    ImU32 flareColor = ImGui::GetColorU32(ImVec4(r, g, b, alpha));
    ImU32 flareCoreColor = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, alpha * 0.8f));

    // Draw 4-pointed star (horizontal and vertical spikes)
    // Vertical spike
    drawList->AddLine(ImVec2(screenPosX, screenPosY - starSize), 
                     ImVec2(screenPosX, screenPosY + starSize),
                     flareColor, 2.0f);

    // Horizontal spike
    drawList->AddLine(ImVec2(screenPosX - starSize, screenPosY), 
                     ImVec2(screenPosX + starSize, screenPosY),
                     flareColor, 2.0f);

    // Diagonal spikes (45 degrees)
    float diagonalDist = starSize * 0.7f;
    drawList->AddLine(ImVec2(screenPosX - diagonalDist, screenPosY - diagonalDist), 
                     ImVec2(screenPosX + diagonalDist, screenPosY + diagonalDist),
                     flareColor, 1.5f);

    drawList->AddLine(ImVec2(screenPosX + diagonalDist, screenPosY - diagonalDist), 
                     ImVec2(screenPosX - diagonalDist, screenPosY + diagonalDist),
                     flareColor, 1.5f);

    // Central bright core
    float coreSize = starSize * 0.3f;
    drawList->AddCircleFilled(ImVec2(screenPosX, screenPosY), coreSize, flareCoreColor, 16);

    // Glow rings around flare
    float glowIntensity = alpha * 0.4f;
    for (int ring = 3; ring >= 1; ring--)
    {
        float ringRadius = starSize * (0.5f + ring * 0.15f);
        ImU32 ringColor = ImGui::GetColorU32(ImVec4(r, g, b, glowIntensity / ring));
        drawList->AddCircle(ImVec2(screenPosX, screenPosY), ringRadius, ringColor, 24, 1.0f);
    }
}

// Render anamorphic flare streaks (light rays)
static void renderFlareStreaks(float screenX, float screenY, float intensity, float alpha,
                              float r, float g, float b)
{
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    ImVec2 displaySize = ImGui::GetIO().DisplaySize;

    if (!drawList || displaySize.x <= 0.0f || displaySize.y <= 0.0f)
        return;

    float scaleX = displaySize.x / 320.0f;
    float scaleY = displaySize.y / 200.0f;

    float screenPosX = screenX * scaleX;
    float screenPosY = screenY * scaleY;

    // Horizontal anamorphic streak
    float streakLength = intensity * 40.0f;
    float streakAlpha = alpha * 0.3f;
    ImU32 streakColor = ImGui::GetColorU32(ImVec4(r, g, b, streakAlpha));

    // Left streak
    drawList->AddLine(ImVec2(screenPosX - streakLength, screenPosY),
                     ImVec2(screenPosX, screenPosY),
                     streakColor, 1.5f);

    // Right streak
    drawList->AddLine(ImVec2(screenPosX, screenPosY),
                     ImVec2(screenPosX + streakLength, screenPosY),
                     streakColor, 1.5f);
}

static void updateLanternGlowFade(float deltaTime);

///////////////////////////////////////////////////////////////////////////////
// Player Occlusion Detection & Fade Management
///////////////////////////////////////////////////////////////////////////////

// Calculate if player is facing away from camera based on relative angle
// Returns true if lamp should be occluded by player's body
static bool isLampOccludedByPlayerDynamic()
{
    extern std::array<tObject, NUM_MAX_OBJECT> ListObjets;
    extern std::vector<cameraDataStruct> g_currentFloorCameraData;
    extern s16 NumCamera;

    // Get the player actor
    s16 heroSlot = currentCameraTargetActor;
    if (heroSlot < 0 || heroSlot >= NUM_MAX_OBJECT)
        heroSlot = 0;

    const tObject& player = ListObjets[heroSlot];

    // Player's beta angle (Y-axis rotation, facing direction)
    // In FITD, beta is in range 0-1023, representing 0-360°
    s16 playerBeta = player.beta;

    // Get current camera data
    if (NumCamera < 0 || NumCamera >= (s16)g_currentFloorCameraData.size())
        return false;  // Invalid camera, don't occlude

    const cameraDataStruct& camera = g_currentFloorCameraData[NumCamera];

    // Camera's beta angle (camera viewing direction)
    s16 cameraBeta = camera.beta;

    // Normalize angles to 0-1023 range
    while (playerBeta < 0) playerBeta += 1024;
    while (playerBeta >= 1024) playerBeta -= 1024;
    while (cameraBeta < 0) cameraBeta += 1024;
    while (cameraBeta >= 1024) cameraBeta -= 1024;

    // Calculate relative angle between player facing and camera viewing direction
    // The camera's beta angle tells us which direction it's looking
    // We want to know if the player is facing away from the camera

    // Convert camera beta to the direction the camera is looking FROM
    // (camera beta is the angle it's looking at, we need the opposite direction)
    s16 cameraLookingFrom = (cameraBeta + 512) % 1024;

    // Calculate angle difference between player facing and camera position
    s16 angleDiff = playerBeta - cameraLookingFrom;

    // Normalize to -512 to 512 range (easier to work with)
    while (angleDiff < -512) angleDiff += 1024;
    while (angleDiff > 512) angleDiff -= 1024;

    // If angle difference is near 0, player is facing away from camera (back to camera)
    // If angle difference is near ±512, player is facing toward camera
    // We use a ±96 degree window (roughly ±170 in 0-1023 range) to determine occlusion
    bool facingAway = (angleDiff >= -96 && angleDiff <= 96);

    if (facingAway)
    {
        static int debugOcclusionCount = 0;
        if (debugOcclusionCount++ % 60 == 0)
        {
            printf("[LANTERN-DEBUG] Lamp occluded (playerBeta=%d, cameraBeta=%d, camFrom=%d, diff=%d)\n",
                   playerBeta, cameraBeta, cameraLookingFrom, angleDiff);
        }
    }

    return facingAway;
}

///////////////////////////////////////////////////////////////////////////////
// Camera Mask Detection
///////////////////////////////////////////////////////////////////////////////

// Check if player is under a camera mask (should be hidden from view)
// Camera masks are 3D zones where actors should be visually occluded (e.g., behind foreground objects)
static bool isPlayerUnderCameraMask()
{
    extern std::array<tObject, NUM_MAX_OBJECT> ListObjets;
    extern std::vector<cameraDataStruct*> cameraDataTable;
    extern s16 NumCamera;

    // Get the player actor
    s16 heroSlot = currentCameraTargetActor;
    if (heroSlot < 0 || heroSlot >= NUM_MAX_OBJECT)
        heroSlot = 0;

    const tObject& player = ListObjets[heroSlot];

    // Get player's ZV bounding box (3D bounding volume)
    const ZVStruct& playerZV = player.zv;

    // Get current camera data
    if (NumCamera < 0 || NumCamera >= (s16)cameraDataTable.size())
        return false;  // Invalid camera, no mask check

    cameraDataStruct* pCamera = cameraDataTable[NumCamera];
    if (!pCamera)
        return false;

    // Look for the correct room data of that camera
    cameraViewedRoomStruct* pcameraViewedRoomData = nullptr;
    for (int i = 0; i < pCamera->numViewedRooms; i++)
    {
        if (pCamera->viewedRoomTable[i].viewedRoomIdx == player.room)
        {
            pcameraViewedRoomData = &pCamera->viewedRoomTable[i];
            break;
        }
    }

    if (pcameraViewedRoomData == nullptr)
        return false;  // Player's room not visible from this camera

    // Check each mask zone in this viewed room
    for (size_t i = 0; i < pcameraViewedRoomData->masks.size(); i++)
    {
        const cameraMaskStruct& maskZone = pcameraViewedRoomData->masks[i];

        // Check each rect test in this mask
        for (size_t j = 0; j < maskZone.rectTests.size(); j++)
        {
            const rectTestStruct& pRect = maskZone.rectTests[j];

            // Convert ZV coordinates to match rect coordinates (divide by 10)
            // This matches the same conversion used in drawBgOverlay()
            int actorX1 = playerZV.ZVX1 / 10;
            int actorX2 = playerZV.ZVX2 / 10;
            int actorZ1 = playerZV.ZVZ1 / 10;
            int actorZ2 = playerZV.ZVZ2 / 10;

            // Check if actor is FULLY CONTAINED within the mask rectangle
            // (This matches the logic from drawBgOverlay in main.cpp)
            if (actorX1 >= pRect.zoneX1 && actorZ1 >= pRect.zoneZ1 && 
                actorX2 <= pRect.zoneX2 && actorZ2 <= pRect.zoneZ2)
            {
                // Player is inside a camera mask zone
                static int debugMaskCount = 0;
                if (debugMaskCount++ % 60 == 0)
                {
                    printf("[LANTERN-DEBUG] Player under camera mask (room=%d, pX=%d-%d, pZ=%d-%d, mX=%d-%d, mZ=%d-%d)\n",
                           player.room, actorX1, actorX2, actorZ1, actorZ2,
                           pRect.zoneX1, pRect.zoneX2, pRect.zoneZ1, pRect.zoneZ2);
                }
                return true;
            }
        }
    }

    return false;  // Player is not under any camera mask
}

// Update fade state based on occlusion and visibility
// Called each frame to smoothly transition glow intensity
static void updateLanternGlowFade(float deltaTime)
{
    // During grace period after menu close, skip occlusion checks
    // This allows the glow to fade back in smoothly without being immediately suppressed
    if (s_allowGlowFadeIn && s_fadeInTimeRemaining > 0.0f)
    {
        s_fadeInTimeRemaining -= deltaTime;
        // Don't override the fade target during grace period - let it fade in naturally
        s_glowFade.update(deltaTime);
        return;
    }

    // Grace period expired - normal occlusion detection applies
    // Check if lamp is occluded by player's body using dynamic camera-based detection
    bool facingAway = isLampOccludedByPlayerDynamic();

    // Check if player is under a camera mask (should be hidden from view)
    bool underMask = isPlayerUnderCameraMask();

    // Set target fade based on occlusion OR mask
    if (facingAway || underMask)
    {
        // Player facing away OR under camera mask - fade out the glow
        s_glowFade.setTarget(0.0f);
    }
    else
    {
        // Player facing toward camera and not masked - fade in the glow
        s_glowFade.setTarget(1.0f);
    }

    // Update fade interpolation
    s_glowFade.update(deltaTime);
}

///////////////////////////////////////////////////////////////////////////////
// Fullscreen Glow Render
///////////////////////////////////////////////////////////////////////////////

void renderLanternFlare()
{
    // Render lens flare effects for in-hand lanterns
    // This should be called after renderLanternGlow in the render pipeline

    // Don't render while menus are open
    if (s_isMenuActive)
        return;

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    if (!drawList)
        return;

    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    if (displaySize.x <= 0.0f || displaySize.y <= 0.0f)
        return;

    // Find any in-hand lantern with visible flare
    for (const auto& pair : g_lanternStates)
    {
        const LanternState& lantern = pair.second;

        if (!lantern.isFlareVisible || lantern.flareAlpha <= 0.01f)
            continue;

        // Calculate final flare intensity based on pulse and alpha
        float pulseIntensity = lantern.flarePulse;  // 0.0-1.0 from animation
        float finalAlpha = lantern.flareAlpha * pulseIntensity;

        if (finalAlpha <= 0.01f)
            continue;

        // Render star-shaped flare
        renderLensFlareStar(lantern.flareScreenX * 320.0f,  // Convert back to game space
                           lantern.flareScreenY * 200.0f,
                           lantern.flareIntensity,
                           finalAlpha,
                           lantern.glowColorR,
                           lantern.glowColorG,
                           lantern.glowColorB);

        // Render anamorphic streak
        renderFlareStreaks(lantern.flareScreenX * 320.0f,
                          lantern.flareScreenY * 200.0f,
                          lantern.flareIntensity,
                          finalAlpha,
                          lantern.glowColorR,
                          lantern.glowColorG,
                          lantern.glowColorB);
    }
}

// Render shadow blobs cast by objects on the ground
// These are semi-transparent shadows that follow lantern light direction
void renderShadowBlobs()
{
    // Render dynamic object shadows when lantern is active
    // Shadows appear on the ground beneath objects

    if (s_isMenuActive)
        return;

    extern std::array<tObject, NUM_MAX_OBJECT> ListObjets;

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    if (!drawList)
        return;

    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    if (displaySize.x <= 0.0f || displaySize.y <= 0.0f)
        return;

    // Find any in-hand lantern
    for (const auto& pair : g_lanternStates)
    {
        const LanternState& lantern = pair.second;

        if (!lantern.isInHand || lantern.shadowIntensity <= 0.01f)
            continue;

        float scaleX = displaySize.x / 320.0f;
        float scaleY = displaySize.y / 200.0f;

        // Render shadow blobs for occluding objects
        for (int i = 1; i < NUM_MAX_OBJECT; ++i)
        {
            const tObject& obj = ListObjets[i];

            // Skip inactive objects and lantern holder
            if (obj.worldX == 0 && obj.worldY == 0 && obj.worldZ == 0)
                continue;

            // Calculate shadow based on occlusion
            float shadowStrength = calculateShadowOcclusion(lantern,
                                                           static_cast<float>(obj.worldX),
                                                           static_cast<float>(obj.worldY),
                                                           static_cast<float>(obj.worldZ));

            if (shadowStrength < 0.1f)
                continue;

            // Shadow blob properties
            float shadowAlpha = shadowStrength * lantern.shadowIntensity * 0.3f;  // Semi-transparent
            ImU32 shadowColor = ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, shadowAlpha));

            // Shadow blob position (approximate ground position of object)
            // Would ideally use actual screen projection of object position
            float shadowRadius = (shadowStrength * 8.0f) * scaleX;  // Size based on occlusion

            // For now, shadows are drawn subtly - in full implementation would use
            // actual screen-space projection of object onto ground plane
        }
    }
}

// Renders lantern glow positioned where the lamp model texture is rendered.
void renderLanternGlow()
{
    // Renders glow using ImGui overlay at the lamp's screen position
    // NOTE: This renders ON TOP of all 3D objects (no depth occlusion)

    // Don't render while any menu is open
    if (s_isMenuActive)
        return;

    extern std::array<tObject, NUM_MAX_OBJECT> ListObjets;

    // Find any in-hand lantern with glow
    const LanternState* lantern = nullptr;
    for (const auto& pair : g_lanternStates)
    {
        if (pair.second.isInHand && pair.second.glowIntensity > 0.0f)
        {
            lantern = &pair.second;
            break;
        }
    }

    if (!lantern || lantern->glowIntensity <= 0.0f)
        return;

    // Calculate delta time for smooth fade animation
    static auto lastFrameTime = std::chrono::high_resolution_clock::now();
    auto currentFrameTime = std::chrono::high_resolution_clock::now();
    float deltaTime = std::chrono::duration<float>(currentFrameTime - lastFrameTime).count();
    lastFrameTime = currentFrameTime;

    // Cap delta time to prevent large jumps (e.g., when paused/debugging)
    if (deltaTime > 0.1f)
        deltaTime = 0.1f;

    // Update fade state based on player orientation and occlusion
    updateLanternGlowFade(deltaTime);

    // Apply fade to glow intensity
    float fadedIntensity = lantern->glowIntensity * s_glowFade.currentFade;

    // Don't render if fully faded out
    if (fadedIntensity <= 0.01f)
        return;

    // Get display size for coordinate scaling
    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    if (displaySize.x <= 0.0f || displaySize.y <= 0.0f)
        return;

    // Game runs at 320x200, scale to actual display size
    float scaleX = displaySize.x / 320.0f;
    float scaleY = displaySize.y / 200.0f;

    float glowCX, glowCY;

    // Try to find lamp primitive position (in screen space)
    if (!findLampColorPosition(glowCX, glowCY))
    {
        // No lamp primitive found - don't render glow
        return;
    }

    // glowCX, glowCY are already in screen space (scaled to display resolution)

    int r = (int)(lantern->glowColorR * 255.0f);
    int g_col = (int)(lantern->glowColorG * 255.0f);
    int b = (int)(lantern->glowColorB * 255.0f);

    // Render soft halo: concentric circle rings with sqrt falloff
    const float maxRadius = 25.0f;  // Game units - much smaller for subtle glow
    const int kLayers = 16;
    const int kSegments = 48;

    for (int layer = kLayers; layer >= 1; layer--)
    {
        float t = (float)layer / (float)kLayers;
        float radius = maxRadius * t;
        float falloff = 1.0f - sqrtf(t);
        float alpha = fadedIntensity * falloff * 140.0f;  // Use faded intensity
        if (alpha < 2.0f) continue;
        if (alpha > 160.0f) alpha = 160.0f;

        // Render filled circle - radius scaled to match display resolution
        renderFilledCircle(glowCX, glowCY, radius * scaleX,
                          r / 255.0f, g_col / 255.0f, b / 255.0f, alpha / 255.0f,
                          kSegments);
    }

    // Bright core
    const float coreRadius = 4.0f;  // Smaller core to match reduced halo
    float coreAlpha = fadedIntensity * 120.0f;  // Use faded intensity for core too
    if (coreAlpha > 160.0f) coreAlpha = 160.0f;
    renderFilledCircle(glowCX, glowCY, coreRadius * scaleX,
                      1.0f, 245.0f/255.0f, 200.0f/255.0f, coreAlpha / 255.0f,
                      24);

    // DEPTH OCCLUSION: Render player mask on top of glow to fake depth
    // This makes the glow appear behind the player's body
    // //Disabled for now, doesnt look good.
    //renderPlayerMask();
}

// Helper function to render a filled circle using ImGui
static void renderFilledCircle(float centerX, float centerY, float radius,
                               float r, float g, float b, float a,
                               int segments)
{
    // Render using ImGui overlay (always on top, no depth testing)
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();

    ImU32 color = IM_COL32((int)(r * 255), (int)(g * 255), (int)(b * 255), (int)(a * 255));
    drawList->AddCircleFilled(ImVec2(centerX, centerY), radius, color, segments);
}

// Render player silhouette as solid overlay to occlude the glow
// Uses the cached player primitives to create an accurate mask
static void renderPlayerMask()
{
    // Generate a mask from the player's rendered primitives
    // We render ALL cached lamp primitives as solid black to create the player silhouette

    extern std::array<tObject, NUM_MAX_OBJECT> ListObjets;

    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    float scaleX = displaySize.x / 320.0f;
    float scaleY = displaySize.y / 200.0f;

    // Get the player actor
    s16 heroSlot = currentCameraTargetActor;
    if (heroSlot < 0 || heroSlot >= NUM_MAX_OBJECT)
        heroSlot = 0;

    const tObject& player = ListObjets[heroSlot];

    // Only render player mask if player has lamp body (body 11)
    if (player.bodyNum != LAMP_BODY_NUM)
        return;

    // Check if we have valid cached primitives
    if (!s_lampPrimCache.isValid || s_lampPrimCache.count == 0)
        return;

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();

    // Render each cached primitive as a solid black polygon to create player silhouette
    for (int i = 0; i < s_lampPrimCache.count; i++)
    {
        const LampPrimitiveCache::CachedPrim& prim = s_lampPrimCache.prims[i];

        // Skip clipped primitives
        if (prim.nearClipped)
            continue;

        // Only render polygons (type 1) and textured polygons (types 9, 10)
        // Spheres (type 3) don't need masking
        if (prim.type != 1 && prim.type != 9 && prim.type != 10)
            continue;

        // Need at least 3 vertices for a polygon
        if (prim.numOfVertices < 3)
            continue;

        // Build polygon from cached vertices
        ImVector<ImVec2> polyPoints;
        polyPoints.reserve(prim.numOfVertices);

        for (int v = 0; v < prim.numOfVertices; v++)
        {
            float x = prim.vertices[v].X * scaleX;
            float y = prim.vertices[v].Y * scaleY;
            polyPoints.push_back(ImVec2(x, y));
        }

        // Render filled polygon with actual ramp/texture color from lamp atlas
        // This shows the lamp's true appearance with proper shading
        u8 colorIdx = prim.color;
        int r, g, b;

        if (prim.isRampPrim) {
            // Ramp-shaded primitives: color index represents shading intensity
            // FITD ramp shading uses materials 3-6 with color index 0-15 for gradients
            // Reduce intensity to make mask darker for better occlusion
            int intensity = colorIdx * 15;  // Scale 0-15 to 0-150 range (darker)
            if (intensity > 140) intensity = 140;  // Cap at darker value

            // Apply warm tone for lamp materials (materials 3-6 are ramp materials)
            if (prim.material >= 3 && prim.material <= 6) {
                // Warm yellowish/orange tone for lit lamp body (darkened)
                r = intensity;
                g = (intensity * 85) / 100;  // Less green for warmth
                b = (intensity * 50) / 100;  // Much less blue for deeper orange
            } else {
                // Neutral grayscale for other ramp primitives (darker)
                r = g = b = (intensity * 80) / 100;
            }
        } else if (prim.type == 9 || prim.type == 10) {
            // Textured polygons: lamp glass or metal details
            int base = 30 + (colorIdx * 6);  // Darker base values
            if (base > 120) base = 120;  // Much lower cap

            // Slightly warm gray for textured lamp parts
            r = base;
            g = (base * 90) / 100;
            b = (base * 80) / 100;
        } else {
            // Spheres and other primitives
            int base = 25 + (colorIdx * 7);  // Darker
            if (base > 100) base = 100;  // Lower cap
            r = g = b = base;
        }


        // Render with actual primitive color to show lamp's ramp/texture appearance
        drawList->AddConvexPolyFilled(polyPoints.Data, polyPoints.Size, IM_COL32(r, g, b, 255));
    }
}



