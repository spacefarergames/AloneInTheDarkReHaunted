///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// HD background renderer declarations
///////////////////////////////////////////////////////////////////////////////

#ifndef _HD_BACKGROUND_RENDERER_H_
#define _HD_BACKGROUND_RENDERER_H_

#include <bgfx/bgfx.h>

struct HDBackgroundInfo; // Forward declaration

// HD background texture state
extern int g_currentBackgroundWidth;
extern int g_currentBackgroundHeight;
extern bool g_currentBackgroundIsHD;

// Separate HD background texture handle
extern bgfx::TextureHandle g_hdBackgroundTexture;

// Functions for HD background rendering
void updateBackgroundTextureHD(unsigned char* data, int width, int height, int channels);
void recreateBackgroundTexture(int width, int height);
bgfx::TextureHandle getActiveBackgroundTexture();
void cleanupHDBackgroundResources(); // Cleanup function to prevent heap corruption

// Preview data management
extern unsigned char* g_hdBackgroundPreviewData;
extern int g_hdBackgroundPreviewWidth;
extern int g_hdBackgroundPreviewHeight;
void freeHDBackgroundPreviewData();

// Animated HD background management
extern HDBackgroundInfo* g_currentAnimatedHDBackground;
extern bool g_resetAnimationTiming;
void updateAnimatedHDBackground(float deltaTime);
void setCurrentAnimatedHDBackground(HDBackgroundInfo* bgInfo);
void pauseCurrentAnimatedHDBackground();
void resumeCurrentAnimatedHDBackground();

// -----------------------------------------------------------------------
// Luminance-guided lantern lighting for HD backgrounds
// Call once per frame from the lantern system with current light state.
//   screenX/Y    : lantern position in screen UV space (0..1)
//   intensity    : effective glow intensity with flicker applied (0..1)
//   lightR/G/B   : warm light color (e.g. 1.0, 0.75, 0.25 for oil lantern)
//   influence    : backgroundLightInfluence from LanternState (0..1)
//   lightRadius  : radius in UV space over which the light falls off (~0.35-0.5)
//   ambientDarken: how deeply unlit shadow areas are darkened (0=black, 1=no change)
// -----------------------------------------------------------------------
void setHDBackgroundLanternUniforms(float screenX, float screenY, float intensity,
                                    float lightR, float lightG, float lightB,
                                    float influence, float lightRadius, float ambientDarken);

#endif // _HD_BACKGROUND_RENDERER_H_