///////////////////////////////////////////////////////////////////////////////
// LANTERN OIL LIGHTING SYSTEM - INTEGRATION GUIDE
// 
// This document explains how to integrate the lantern oil lighting system
// into your game rendering pipeline.
///////////////////////////////////////////////////////////////////////////////

OVERVIEW
========

The lantern lighting system provides:

1. **Bloom Glow Effect** - Makes the lantern body glow warm orange/yellow 
   when it contains oil, especially visible when held in hand

2. **Real-Time Shadow Casting** - The lantern casts dynamic light that 
   illuminates and shadows the HD background based on color values

3. **Oil State Tracking** - Automatically tracks when lanterns have oil and
   whether they're currently held in-hand

4. **Dynamic Light Position** - Light position updates based on lantern 
   location (in-hand = higher intensity, wider radius)


FILES ADDED
===========

Header Files:
- lanternLighting.h         - Public API declarations

Implementation Files:
- lanternLighting.cpp       - Core lighting system implementation

Shader Files:
- lantern_bloom_vs.sc       - Vertex shader for bloom glow
- lantern_bloom_ps.sc       - Fragment shader for bloom glow
- lantern_shadow_vs.sc      - Vertex shader for shadow casting
- lantern_shadow_ps.sc      - Fragment shader for shadow casting


INTEGRATION STEPS
=================

1. INITIALIZE SYSTEM (in main.cpp or initialization code):
   
   ```cpp
   #include "lanternLighting.h"
   
   // During engine startup:
   initLanternLighting();
   
   // During engine shutdown:
   shutdownLanternLighting();
   ```

2. UPDATE LANTERN STATE (call once per frame):

   ```cpp
   // In your main game loop:
   updateLanternLighting();
   ```

3. DETECT LANTERN PICKUP/PUTDOWN:

   In inventory.cpp or where you handle item pickup/putdown:
   
   ```cpp
   #include "lanternLighting.h"
   
   // When player picks up an item:
   if (itemBody == 269) {  // 269 is the lantern foundBody number (tWorldObject.foundBody)
       setLanternInHand(objectIndex, true);
       
       // Set oil state (assumes you have oil tracking elsewhere)
       setLanternOil(objectIndex, true, 0.8f);  // 80% oil
   }
   
   // When player puts down an item:
   if (itemBody == 269) {
       setLanternInHand(objectIndex, false);
   }
   ```

4. SET LANTERN OIL STATE:

   When lantern oil changes (pickups, consumption, refills):
   
   ```cpp
   // Lantern gained oil:
   setLanternOil(objectIndex, true, 1.0f);
   
   // Lantern oil depleted:
   setLanternOil(objectIndex, false, 0.0f);
   
   // Lantern oil partially consumed:
   setLanternOil(objectIndex, true, 0.4f);  // 40% remaining
   ```

5. RENDER LANTERN BLOOM EFFECT (in rendererBGFX.cpp):

   When rendering the in-hand lantern object:

   ```cpp
   #include "lanternLighting.h"

   // After rendering lantern 3D model:
   if (isLanternWithOil(lanternObjectIndex)) {
       LanternState* lantern = getLanternState(lanternObjectIndex);
       if (lantern && lantern->isInHand) {
           applyLanternBloom(lanternTexture, *lantern);
       }
   }
   ```

6. RENDER LENS FLARE EFFECTS (in main rendering loop):

   Render flare effects after glow (between imguiBeginFrame and imguiEndFrame):

   ```cpp
   #include "lanternLighting.h"

   // In EndFrame, after renderLanternGlow():
   renderLanternFlare();
   ```

7. APPLY SHADOW CASTING TO BACKGROUND (in hdBackgroundRenderer.cpp):

   After rendering the HD background, apply dynamic lighting:

   ```cpp
   #include "lanternLighting.h"

   extern std::map<int, LanternState> g_lanternStates;
   extern std::array<tObject, NUM_MAX_OBJECT> ListObjets;

   // After background is rendered, apply lantern lighting:
   for (auto& pair : g_lanternStates) {
       LanternState& lantern = pair.second;
       if (lantern.isInHand && lantern.glowIntensity > 0.0f) {
           // Get player position (typically object 0)
           glm::vec3 playerPos(
               ListObjets[0].worldX,
               ListObjets[0].worldY,
               ListObjets[0].worldZ
           );

           // Apply lighting to background
           applyLanternLightingToBackground(
               backgroundTexture,
               lantern,
               playerPos.x,
               playerPos.y,
               playerPos.z
           );
       }
   }
   ```

8. RENDER SHADOW BLOBS (in EndFrame):

   Render object shadows cast by lantern light:

   ```cpp
   #include "lanternLighting.h"

   // After renderLanternGlow() and renderLanternFlare():
   renderShadowBlobs();
   ```


FLICKERING FLAME EFFECT
=======================

The lantern now includes a realistic flame flicker effect that simulates the natural
variation in flame intensity. This makes the lit lantern appear more alive and organic.

How It Works:
- The flame flicker is calculated using a combination of sine waves at different 
  frequencies (slow breathing, fast licking, turbulent movement)
- The effect is applied every frame during updateLanternLighting()
- Flicker intensity ranges from 0.7 to 1.0, so the flame never fully extinguishes
- The flicker only applies when the lantern has oil and is lit

Fields in LanternState:
- flickerIntensity: Current flicker value (0.7-1.0)
- flickerTime: Time accumulator for the flicker calculation
- effectiveGlowIntensity: The final glow intensity with flicker applied (glowIntensity * flickerIntensity)

Implementation Details:
- The calculateFlameFlicker(time) function uses:
  - slow wave at 2.5 Hz (primary flame breathing)
  - fast wave at 7.3 Hz (flame licking detail)
  - very fast wave at 13.7 Hz (turbulent movement)
- These frequencies combine to create organic, natural-looking variation
- The flicker is reset when the lantern is extinguished and resumes when re-lit

Performance:
- Minimal overhead: just a few sine/cosine calculations per lantern per frame
- Only calculated when lantern has oil and glowIntensity > 0
- Automatically disabled when lantern is empty or not lit


LENS FLARE EFFECTS
==================

When looking directly at the lantern (with it in hand), a realistic lens flare effect
appears on screen, simulating light refracting through camera optics. This enhances
visual immersion and indicates when the player is looking at the light source.

How It Works:
- The lens flare is most prominent when the lantern is near the screen center
- The flare fades smoothly when the lantern moves toward screen edges
- Multiple visual elements compose the effect:
  - Star-shaped flare with 4 main spikes (+ orientation)
  - Diagonal secondary spikes (X orientation)
  - Bright central core
  - Concentric glow rings for halo effect
  - Horizontal anamorphic streaks (light rays)

Fields in LanternState:
- isFlareVisible: Whether flare is currently on-screen and visible
- flareScreenX, flareScreenY: Screen-space position (0.0-1.0, center = 0.5)
- flareIntensity: Flare brightness multiplier (0.0-1.0)
- flareAlpha: Opacity for smooth fade-in/fade-out
- flarePulse: Pulsing animation value for organic variation

Implementation Details:
- calculateFlareVisibility(): Determines flare strength based on distance from screen center
  - Full visibility at screen center
  - Linear falloff toward screen edges
  - Zero visibility when lantern is off-screen
- updateFlareAnimation(): Adds pulsing effect at 2 Hz for organic appearance
- renderLensFlareStar(): Draws star pattern with glow rings using ImGui draw list
- renderFlareStreaks(): Renders horizontal anamorphic lens artifacts

Screen Space Rendering:
- Flares are rendered on-screen using ImGui's background draw list
- Drawn after renderLanternGlow() to appear on top of glow
- Uses game coordinate space (320x200) scaled to display resolution
- Does NOT affect 3D rendering or shadow casting

Performance:
- Screen-space only: minimal GPU impact
- Only updated and rendered for in-hand lanterns
- Smooth animation with configurable pulse frequency
- Works simultaneously with bloom glow and shadow effects


DYNAMIC SHADOW OCCLUSION AND HD BACKGROUND LIGHTING
===================================================

The lantern system now features advanced dynamic shadow occlusion and intelligent
background lighting that creates realistic illumination and shadow effects on the
HD background and environmental objects.

How It Works:

1. **Shadow Occlusion**
   - Objects between the lantern and camera cast dynamic shadows
   - Shadow strength is distance-based: closer objects cast darker shadows
   - Shadow intensity varies with lantern oil level and glow intensity
   - Objects are tracked for occlusion automatically each frame

2. **Depth-Based Shadow Filtering**
   - Objects at different heights cast shadows with varying hardness
   - Objects at same height as lantern cast sharp, defined shadows
   - Objects higher than lantern cast softer, more diffuse shadows
   - Creates realistic perspective-aware shadow projection

3. **Background Illumination**
   - Lantern light brightens the HD background near the light source
   - Illumination effect falls off with distance from lantern
   - Warm orange/yellow light colors blend naturally with environment
   - Shadow areas (away from light) slightly darken for contrast

4. **Dynamic Light Influence**
   - Background lighting intensity scales with lantern's effective glow
   - Flickering flame effect also affects background brightness
   - Oil level modulates both shadow strength and light brightness
   - Smooth transitions prevent jarring changes

Fields in LanternState:
- shadowIntensity: Current shadow strength (0.0-1.0)
- lightDarkness: How much lantern darkens unlit areas (0.0-1.0)
- lightBrightness: How much lantern brightens lit areas (0.0-1.0)
- occludingObjectCount: Number of objects currently occluding light
- backgroundLightInfluence: How much lantern affects background (0.0-1.0)

Implementation Details:
- calculateShadowOcclusion(): Distance-based shadow calculation
  - Returns 0.0 for objects beyond light radius
  - Returns 1.0 for objects very close to lantern
  - Linear interpolation between

- calculateDepthShadowFilter(): Height-aware shadow softening
  - Analyzes relative Y-positions (heights) of lantern and objects
  - Adjusts shadow hardness based on height difference
  - Objects much higher cast very soft shadows (less visually prominent)

- updateShadowOcclusion(): Scans scene each frame for occluding objects
  - Checks all active objects in ListObjets
  - Applies depth filtering to each potential occluder
  - Accumulates maximum shadow from all objects

- updateBackgroundLighting(): Calculates illumination effect
  - Computes distance from lantern to background
  - Determines light influence based on radius and distance
  - Calculates brightening and darkening effects
  - Scales with effective glow intensity

3. **Background Rendering Integration**
   - applyLanternLightingToBackground() uses occlusion data
   - Shadow effects reduce overall light radius when objects occlude
   - Illumination colors are enhanced by brightness multiplier
   - Unlit areas slightly darkened by darkness multiplier
   - Shader uniforms updated each frame for smooth animation

4. **Shadow Blob Rendering**
   - renderShadowBlobs() draws semi-transparent shadows on ground
   - Shadow visibility and size determined by occlusion strength
   - Placed beneath occluding objects in approximate screen space
   - Opacity modulated by shadow intensity and depth filter
   - Optional feature for additional visual feedback

Screen Space Effects:
- Shadow blobs drawn via ImGui background draw list
- Scales with display resolution automatically
- Rendered after flares for proper layering
- Uses semi-transparent black for subtle, natural appearance
- Can be disabled without affecting background lighting

Performance:
- Shadow occlusion: O(N) per frame where N = number of objects
  - Typical FITD scenes have 30-50 active objects
  - Minimal overhead with early exit for inactive objects

- Background lighting: constant time calculation
  - Single distance computation per lantern per frame
  - Uses existing shader system, no additional overhead

- Shadow blobs: O(N) screen space rendering
  - Only rendered for occluding objects (typically 2-5)
  - ImGui rendering very efficient

- Total impact: negligible, < 1ms per frame typical


CUSTOMIZATION
=============

Oil Glow Color:
- Edit LANTERN_GLOW_COLOR in lanternLighting.cpp
  Default: glm::vec3(1.0f, 0.8f, 0.3f) - warm orange/yellow
  Adjust RGB values (0.0-1.0) for different colors

Light Radius:
- Edit LANTERN_BASE_RADIUS in lanternLighting.cpp
  Default: 200.0f world units
  Increase for wider light spread, decrease for focused beam

Bloom Intensity:
- Edit shader files to adjust how bright the glow effect is
- lantern_bloom_ps.sc - increase bloomIntensity multiplier
- lantern_shadow_ps.sc - adjust light contribution amount

Flame Flicker Effect:
- Flicker strength: Edit the "0.15f" value in calculateFlameFlicker()
  Lower values = more stable flame, higher values = more dramatic flicker
- Flicker speed: Adjust the frequency constants (2.5f, 7.3f, 13.7f)
  Higher values = faster flicker, lower values = slower breathing
- Minimum intensity: Edit the "0.85f" base value to change minimum brightness
  0.85f means flame stays between 70-100% brightness (0.85 - 0.85*0.15 to 0.85 + 0.85*0.15)

Lens Flare Effect:
- Flare visibility: Edit calculateFlareVisibility() distance formula
  Default "1.5f" multiplier controls falloff from screen center
  Increase for wider visible area, decrease for tighter flare zone
- Flare star size: Edit "starSize = intensity * 8.0f" in renderLensFlareStar()
  Adjust the "8.0f" multiplier for larger or smaller flare
- Flare streak length: Edit "streakLength = intensity * 40.0f" in renderFlareStreaks()
  Adjust the "40.0f" multiplier for longer or shorter horizontal rays
- Flare pulse speed: Edit the "2.0f" frequency in updateFlareAnimation()
  Higher values = faster pulsing, lower values = slower breathing
- Flare visibility range: Edit "0.01f" threshold in renderLanternFlare()
  Lower threshold = flare visible longer during fade-out, higher = disappears faster

Shadow Occlusion Effect:
- Shadow strength: Edit the multiplier in updateShadowOcclusion()
  Default scales shadow intensity with glow: "maxShadow * lantern.effectiveGlowIntensity"
  Multiply by additional factor (0.5f for lighter, 2.0f for darker shadows)

- Shadow radius: Controlled by lantern.lightRadius
  Larger radius = shadows visible further away
  Modify LANTERN_BASE_RADIUS for overall shadow reach

- Depth shadow filter hardness: Edit calculateDepthShadowFilter()
  Adjust the "heightDifference / (lanternHeightAboveGround * 2.0f)" formula
  Increase divisor for softer shadows, decrease for sharper shadows

- Background light boost: Edit updateBackgroundLighting()
  "lantern.lightBrightness = lantern.effectiveGlowIntensity * 0.8f"
  Adjust the 0.8f multiplier for more/less bright illumination

- Background dark effect: Edit updateBackgroundLighting()
  "lantern.lightDarkness = lantern.effectiveGlowIntensity * 0.4f"
  Adjust 0.4f multiplier for more/less shadow in dark areas

- Shadow blob opacity: Edit renderShadowBlobs()
  Default: "shadowAlpha = shadowStrength * lantern.shadowIntensity * 0.3f"
  Multiply 0.3f by larger factor (e.g., 0.5f) for darker blobs on ground

- Shadow blob radius: Edit "shadowRadius = (shadowStrength * 8.0f) * scaleX"
  Adjust the 8.0f multiplier for larger/smaller blob sizes

Lantern Body Number:
- LANTERN_BODY_NUM     = 269  (unlit lantern, confirmed via runtime logging)
- LANTERN_LIT_BODY_NUM = 10   (lit lantern with oil+matches, confirmed via runtime logging)
- Both are defined in lanternLighting.h. Glow only activates for the lit state (foundBody=10).


TROUBLESHOOTING
===============

Q: Lantern doesn't glow even when it has oil
A: - Verify initLanternLighting() was called during startup
   - Check that shaders compiled successfully (check output logs)
   - Verify lantern body number (40) matches your game data
   - Ensure setLanternOil() is being called with correct object index

Q: Bloom effect is too subtle/too strong
A: - Adjust bloomIntensity multiplier in lantern_bloom_ps.sc
   - Modify LANTERN_GLOW_COLOR for different color intensity
   - Increase/decrease glowIntensity calculation in setLanternOil()

Q: Flame flicker is too rapid/too slow
A: - Adjust frequency constants in calculateFlameFlicker():
     - Reduce (2.5f, 7.3f, 13.7f) values for slower flicker
     - Increase values for faster flicker
   - Adjust the time increment (0.016f) in updateLanternLighting()
     - Reduce for slower time progression, increase for faster

Q: Flame flicker is too subtle/too strong
A: - Edit the flicker strength constant in calculateFlameFlicker():
     - Change "0.15f" to a smaller value for more stable flame
     - Change to a larger value for more dramatic flicker variation
   - Adjust the minimum intensity "0.85f" in calculateFlameFlicker()

Q: Lens flare doesn't appear when looking at lantern
A: - Verify renderLanternFlare() is being called in the render pipeline
   - Check that lantern is in-hand and has oil (glowIntensity > 0)
   - Ensure menu is not open (flare is hidden while menus active)
   - Try moving lantern toward screen center to make flare more visible

Q: Lens flare is too bright/too dim
A: - Adjust flare alpha multipliers in renderLensFlareStar() and renderFlareStreaks()
   - Change "alpha" parameter usage to multiply by additional factor (0.5f for dimmer, 2.0f for brighter)
   - Adjust "pulseIntensity" weight in renderLanternFlare()

Q: Lens flare pulsing is too fast/too slow
A: - Edit the frequency in updateFlareAnimation():
     - Change "2.0f * 3.14159f" to smaller value for slower pulse (e.g., 1.0f for half speed)
     - Change to larger value for faster pulse (e.g., 3.0f for 1.5x speed)

Q: Lens flare streaks are too long/too short
A: - Edit "streakLength = intensity * 40.0f" in renderFlareStreaks()
   - Reduce the "40.0f" multiplier for shorter streaks
   - Increase for longer streaks

Q: Lens flare visible off-screen at screen edges
A: - Adjust the "1.5f" distance multiplier in calculateFlareVisibility()
   - Increase value to make falloff sharper (flare disappears sooner at edges)
   - Decrease value to make falloff more gradual

Q: Objects don't cast shadows with lantern
A: - Verify renderShadowBlobs() is being called in EndFrame
   - Check that lantern is in-hand and has oil
   - Verify objects are active (not all zeroed positions)
   - Check occludingObjectCount in lantern state during debug
   - Shadow visibility depends on shadowIntensity > 0.01f

Q: Shadows are too dark/too light
A: - Adjust shadow strength in updateShadowOcclusion()
   - Modify the shadow multiplier in applyLanternLightingToBackground()
   - Check shadowIntensity calculation is scaling properly
   - Adjust lightDarkness and lightBrightness multipliers

Q: Background lighting not responding to lantern
A: - Verify applyLanternLightingToBackground() is called
   - Check that lantern has oil and glow intensity > 0
   - Ensure backgroundLightInfluence is > 0.01f
   - Verify shader receives updated uniforms correctly
   - Check lightBrightness and lightDarkness are calculated

Q: Depth-based shadow filtering not working
A: - Verify objects have correct worldY (height) values
   - Check calculateDepthShadowFilter() is being called in updateShadowOcclusion()
   - Adjust the height difference formula in the function
   - Objects must be at different Y positions to show effect

Q: Shadow occlusion performance impact
A: - Shadow occlusion checks all objects each frame: O(N)
   - With 30-50 typical objects, impact is minimal
   - Shadow blobs only rendered for occluding objects (2-5 typical)
   - Verify no infinite loops in object iteration
   - Consider reducing NUM_MAX_OBJECT if many inactive slots

Q: Shadow casting affects wrong areas of screen
A: - Verify object positions are updated correctly
   - Check calculateShadowOcclusion() distance formula
   - Shadows affect background shader, not screen directly
   - Verify applyLanternLightingToBackground() positions are correct

Q: Performance impact
A: - Shadow casting shader runs every frame only if lantern in hand
   - Bloom effect only applies to lantern itself, not full screen
   - Flicker calculation is minimal (just sine/cosine operations)
   - Lens flare is screen-space only, very efficient
   - Shadow occlusion adds ~1ms per 30-50 objects
   - Total impact: negligible, < 1ms typically
   - Consider caching lantern states or only updating visible lanterns


FUTURE ENHANCEMENTS
====================

1. Multi-lantern support with overlapping light areas
2. Oil consumption over time (visual brightness reduction)
3. ✓ Flickering flame effect for more realism (IMPLEMENTED)
4. ✓ Dynamic shadow occlusion for detailed objects (IMPLEMENTED)
5. Colored oil variants (blue, green) with different glow colors
6. ✓ Light flare effects when looking at lantern directly (IMPLEMENTED)
7. Performance optimization with light-space shadow mapping
8. Real-time shadow mapping for static geometry
9. Volumetric light rays (god rays/crepuscular rays)
10. Interactive flame in lantern with sprite animation

