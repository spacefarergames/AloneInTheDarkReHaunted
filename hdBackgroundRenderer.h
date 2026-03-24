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

#endif // _HD_BACKGROUND_RENDERER_H_