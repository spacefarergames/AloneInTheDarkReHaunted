///////////////////////////////////////////////////////////////////////////////
// Animated Background Grass-Mask Generator
//
// Auto-generates per-frame grayscale "grass" masks for selected animated HD
// backgrounds (currently CAMERA07_004). The mask isolates green vegetation
// pixels in each frame so the renderer can use it as a foreground overlay
// that occludes 3D actors. Generated masks are cached on disk under
// `backgrounds_hd/anim_<name>_<cam>_grassmask/` so subsequent runs reuse
// them without recomputation.
//
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
///////////////////////////////////////////////////////////////////////////////

#ifndef _BG_ANIM_GRASS_MASK_H_
#define _BG_ANIM_GRASS_MASK_H_

struct HDBackgroundInfo;

// Notify the grass-mask system that an animated HD background just became
// (or stopped being) the active background. Pass nullptr/-1/nullptr to
// indicate no animated background is active.
//   backgroundName : e.g. "CAMERA07"
//   cameraIdx      : camera index (e.g. 4 for CAMERA07_004)
//   bgInfo         : the HD background info (kept alive by hdBackgroundRenderer)
void bgAnimGrassMask_onAnimatedBackgroundLoaded(const char* backgroundName,
                                                int cameraIdx,
                                                HDBackgroundInfo* bgInfo);

// True iff grass-mask generation is configured and active for the current
// animated HD background.
bool bgAnimGrassMask_isActive();

// Returns a pointer to the R8 (single-channel, 0 or 255) mask data for the
// CURRENT animation frame, generating it on demand if needed. Returns
// nullptr if not active or the source frame is not yet loaded.
//   *outW, *outH : mask dimensions (matches source frame dimensions)
//   *outVersion  : opaque counter that bumps whenever the active background
//                  changes (renderer uses this to know when to recreate
//                  its bgfx texture)
//   *outFrameId  : current frame index (changes as animation advances; the
//                  renderer uses this to decide when to re-upload data)
const unsigned char* bgAnimGrassMask_getCurrentFrameMask(int* outW,
                                                         int* outH,
                                                         int* outVersion,
                                                         int* outFrameId);

// Free all cached masks and reset state.
void bgAnimGrassMask_shutdown();

// Renderer-side hook (implemented in rendererBGFX.cpp). Draws the current
// grass-mask frame as a full-screen foreground overlay on top of already-
// rendered 3D actors, using the existing HD mask shader. No-op when the
// grass-mask system is inactive.
void osystem_drawAnimatedGrassMask();

#endif // _BG_ANIM_GRASS_MASK_H_
