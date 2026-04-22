///////////////////////////////////////////////////////////////////////////////
// Lantern Oil Lighting System
// 
// Handles dynamic bloom and shadow casting for the lantern when it has oil
// and is being held in the player's hand.
//
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <bgfx/bgfx.h>
#include <map>

// foundBody values from tWorldObject for the lantern in each state
// (confirmed via runtime [LANTERN-DEBUG] logging)
static const int LANTERN_BODY_NUM     = 269;  // unlit lantern (no oil/matches)
static const int LANTERN_LIT_BODY_NUM = 10;   // lit lantern (oil + matches applied)

// Oil state tracking for lanterns
struct LanternState
{
    bool hasOil;                    // Whether lantern contains oil
    float oilLevel;                 // Oil level (0.0-1.0)
    float glowIntensity;            // Bloom/glow intensity (without flicker)
    float glowColorR;               // Glow color R component
    float glowColorG;               // Glow color G component
    float glowColorB;               // Glow color B component
    float lightPosX;                // Light position X (world space)
    float lightPosY;                // Light position Y (world space)
    float lightPosZ;                // Light position Z (world space)
    float lightRadius;              // Radius of light effect
    bool isInHand;                  // Whether lantern is currently held

    // Flame flicker effect
    float flickerIntensity;         // Current flicker intensity (0.0-1.0)
    float flickerTime;              // Time accumulator for flicker effect
    float effectiveGlowIntensity;   // glowIntensity with flicker applied

    // Light flare effect
    bool isFlareVisible;            // Whether flare is currently on-screen
    float flareScreenX;             // Screen-space X position of flare (0-1, center = 0.5)
    float flareScreenY;             // Screen-space Y position of flare (0-1, center = 0.5)
    float flareIntensity;           // Flare brightness (0.0-1.0)
    float flareAlpha;               // Flare opacity for fade-in/fade-out
    float flarePulse;               // Pulse animation value (0.0-1.0)

    // Shadow occlusion and background lighting
    float shadowIntensity;          // Shadow strength (0.0-1.0)
    float lightDarkness;            // How much lantern darkens unlit areas (0.0-1.0)
    float lightBrightness;          // How much lantern brightens lit areas (0.0-1.0)
    int occludingObjectCount;       // Number of objects currently occluding light
    float backgroundLightInfluence; // How much lantern affects background (0.0-1.0)
};

// Global lantern states
extern std::map<int, LanternState> g_lanternStates;

///////////////////////////////////////////////////////////////////////////////
// Lantern Lighting API
///////////////////////////////////////////////////////////////////////////////

// Initialize lantern lighting system
void initLanternLighting();

// Shutdown lantern lighting system
void shutdownLanternLighting();

// Update lantern state (call once per frame)
void updateLanternLighting();

// Check if a specific object is a lantern with oil
bool isLanternWithOil(int objectIdx);

// Get lantern state by object index
LanternState* getLanternState(int objectIdx);

// Set lantern oil state
void setLanternOil(int objectIdx, bool hasOil, float oilLevel = 1.0f);

// Set lantern in-hand status
void setLanternInHand(int objectIdx, bool inHand);

// Apply lantern lighting to HD background
// This darkens/illuminates the background based on lantern position and color
void applyLanternLightingToBackground(bgfx::TextureHandle backgroundTexture, 
                                      const LanternState& lantern,
                                      float playerX, float playerY, float playerZ);

// Get bloom shader for lantern glow
bgfx::ProgramHandle getLanternBloomShader();

// Apply bloom effect to lantern when in-hand
void applyLanternBloom(bgfx::TextureHandle lanternTexture, 
                       const LanternState& lantern);

// Render lantern glow overlay (call from EndFrame, between imguiBeginFrame and imguiEndFrame)
// Draws a radial warm glow centred on the held lantern's screen position via ImGui draw list.
void renderLanternGlow();

// Render lens flare effects (call from EndFrame after renderLanternGlow)
// Draws star-shaped lens flare and anamorphic streaks when looking at lantern
void renderLanternFlare();

// Render shadow blobs cast by objects (call from EndFrame)
// Draws semi-transparent shadows beneath objects in lantern light
void renderShadowBlobs();

// Populate lamp primitive cache (call immediately after rendering body 11)
// CRITICAL: Must be called while primTable[] still contains body 11's primitives
void populateLampPrimitiveCache();

// Notify the lantern system that a menu is open/closed.
// While active, renderLanternGlow() will skip rendering so the glow
// doesn't bleed through inventory/system-menu UI.
void setLanternMenuActive(bool active);

