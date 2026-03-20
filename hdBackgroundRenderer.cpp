///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Author: Jake Jackson (jake@spacefarergames.com)
//
// HD background texture rendering with BGFX
///////////////////////////////////////////////////////////////////////////////

#ifndef AITD_UE4
#include "common.h"
#include "hdBackgroundRenderer.h"
#include "hdBackground.h"
#include "hdArchive.h"
#include "configRemaster.h"
#include "memoryManager.h"
#include "exceptionHandler.h"
#include "resourceGC.h"
#include <bgfx/bgfx.h>

// Global variables for HD background tracking
int g_currentBackgroundWidth = 320;
int g_currentBackgroundHeight = 200;
bool g_currentBackgroundIsHD = false;

// Separate HD background texture - NEVER recycle g_backgroundTexture handle!
bgfx::TextureHandle g_hdBackgroundTexture = BGFX_INVALID_HANDLE;

// HD background preview data (RGBA copy for menu preview)
unsigned char* g_hdBackgroundPreviewData = nullptr;
int g_hdBackgroundPreviewWidth = 0;
int g_hdBackgroundPreviewHeight = 0;

// Current animated HD background (kept alive for animation updates)
HDBackgroundInfo* g_currentAnimatedHDBackground = nullptr;

// Flag to reset animation timing on next frame (prevents speed-up after camera transitions)
bool g_resetAnimationTiming = false;

// External references to background texture
extern bgfx::TextureHandle g_backgroundTexture;
extern unsigned char physicalScreen[320 * 200];
extern bool g_bgfxMainResourcesInitialized;

// Get the appropriate background texture based on current mode
bgfx::TextureHandle getActiveBackgroundTexture()
{
    if (g_currentBackgroundIsHD && bgfx::isValid(g_hdBackgroundTexture))
    {
        return g_hdBackgroundTexture;
    }
    return g_backgroundTexture;
}

// Recreate background texture with new dimensions
void recreateBackgroundTexture(int width, int height)
{
    if (!g_bgfxMainResourcesInitialized)
    {
        g_currentBackgroundWidth = width;
        g_currentBackgroundHeight = height;
        g_currentBackgroundIsHD = (width > 320 || height > 200);
        return;
    }
    
    if (width > 320 || height > 200)
    {
        g_currentBackgroundIsHD = true;

        if (g_currentBackgroundWidth != width || g_currentBackgroundHeight != height || !bgfx::isValid(g_hdBackgroundTexture))
        {
            if (bgfx::isValid(g_hdBackgroundTexture))
            {
                bgfx::destroy(g_hdBackgroundTexture);
                g_hdBackgroundTexture = BGFX_INVALID_HANDLE; // Prevent double-free
            }

            g_hdBackgroundTexture = bgfx::createTexture2D(
                width, height, false, 1,
                bgfx::TextureFormat::RGBA8,
                BGFX_TEXTURE_NONE | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP
            );

            printf("HD background texture created: %dx%d\n", width, height);
        }
    }
    else
    {
        g_currentBackgroundIsHD = false;
    }
    
    g_currentBackgroundWidth = width;
    g_currentBackgroundHeight = height;
}

// Free HD background preview data
void freeHDBackgroundPreviewData()
{
    if (g_hdBackgroundPreviewData)
    {
        SAFE_FREE(g_hdBackgroundPreviewData, __FILE__, __LINE__);
    }
    g_hdBackgroundPreviewWidth = 0;
    g_hdBackgroundPreviewHeight = 0;
}

// Cleanup HD background resources
void cleanupHDBackgroundResources()
{
    PROTECTED_CALL({
        // Flush any deferred GC items before doing direct cleanup
        ResourceGC::flush();

        // Clean up animated background first
        if (g_currentAnimatedHDBackground)
        {
            freeHDBackground(g_currentAnimatedHDBackground);
            g_currentAnimatedHDBackground = nullptr;
        }

        // Clean up HD texture
        if (bgfx::isValid(g_hdBackgroundTexture))
        {
            bgfx::destroy(g_hdBackgroundTexture);
            g_hdBackgroundTexture = BGFX_INVALID_HANDLE;
        }

        // Clean up preview data
        freeHDBackgroundPreviewData();

        // Close HD archive
        HDArchive::close();

        // Reset state
        g_currentBackgroundIsHD = false;
        g_currentBackgroundWidth = 320;
        g_currentBackgroundHeight = 200;
    }, "cleanup HD background resources");
}

// Update background texture with HD data
void updateBackgroundTextureHD(unsigned char* data, int width, int height, int channels)
{
    if (!data || !g_bgfxMainResourcesInitialized)
        return;
    
    if (width != g_currentBackgroundWidth || height != g_currentBackgroundHeight)
    {
        recreateBackgroundTexture(width, height);
    }
    
    if (g_currentBackgroundIsHD)
    {
        if (!bgfx::isValid(g_hdBackgroundTexture))
            return;
        
        // Free previous preview data
        freeHDBackgroundPreviewData();
        
        if (channels == 3)
        {
            unsigned char* rgbaData = (unsigned char*)SAFE_MALLOC((size_t)width * height * 4, __FILE__, __LINE__);
            if (rgbaData)
            {
                for (size_t i = 0; i < (size_t)width * height; i++)
                {
                    rgbaData[i * 4 + 0] = data[i * 3 + 0];
                    rgbaData[i * 4 + 1] = data[i * 3 + 1];
                    rgbaData[i * 4 + 2] = data[i * 3 + 2];
                    rgbaData[i * 4 + 3] = 255;
                }
                
                bgfx::updateTexture2D(g_hdBackgroundTexture, 0, 0, 0, 0, width, height, 
                                      bgfx::copy(rgbaData, (uint32_t)((size_t)width * height * 4)));
                
                // Store copy for menu preview
                g_hdBackgroundPreviewData = rgbaData;
                g_hdBackgroundPreviewWidth = width;
                g_hdBackgroundPreviewHeight = height;
            }
        }
        else if (channels == 4)
        {
            bgfx::updateTexture2D(g_hdBackgroundTexture, 0, 0, 0, 0, width, height, 
                                  bgfx::copy(data, (uint32_t)((size_t)width * height * 4)));

            // Store copy for menu preview
            g_hdBackgroundPreviewData = (unsigned char*)SAFE_MALLOC((size_t)width * height * 4, __FILE__, __LINE__);
            if (g_hdBackgroundPreviewData)
            {
                memcpy(g_hdBackgroundPreviewData, data, (size_t)width * height * 4);
                g_hdBackgroundPreviewWidth = width;
                g_hdBackgroundPreviewHeight = height;
            }
        }
    }
}

// Set the current animated HD background
void setCurrentAnimatedHDBackground(HDBackgroundInfo* bgInfo)
{
    // Free the previous animated background if it exists
    if (g_currentAnimatedHDBackground)
    {
        // Ensure we don't try to set the same background
        if (g_currentAnimatedHDBackground == bgInfo)
        {
            return; // Already set, nothing to do
        }

        // Schedule for deferred free rather than freeing immediately.
        // The caller should have already called ResourceGC::flush() before loading
        // the new background, ensuring this item is freed before the new load begins.
        ResourceGC::scheduleHDBackground(g_currentAnimatedHDBackground);
        g_currentAnimatedHDBackground = nullptr; // Explicitly null before setting new one
    }

    // Set the new background (can be nullptr to clear)
    g_currentAnimatedHDBackground = bgInfo;

    // Reset animation timing so the first frame displays without speed-up
    if (bgInfo && bgInfo->isAnimated)
    {
        bgInfo->frameTimer = 0.0f;
        bgInfo->currentFrame = 0;
        if (bgInfo->frames && bgInfo->frames[0])
        {
            bgInfo->data = bgInfo->frames[0];
        }
        g_resetAnimationTiming = true;
    }
}

// Update animated HD background each frame
void updateAnimatedHDBackground(float deltaTime)
{
    // Safety check: ensure background is valid before accessing
    if (!g_currentAnimatedHDBackground || !g_currentAnimatedHDBackground->isAnimated)
    {
        return;
    }

    // Additional safety: verify frames array is still valid
    if (!g_currentAnimatedHDBackground->frames || 
        g_currentAnimatedHDBackground->currentFrame >= g_currentAnimatedHDBackground->frameCount)
    {
        return;
    }

    // Update the animation frame
    if (updateHDBackgroundAnimation(g_currentAnimatedHDBackground, deltaTime))
    {
        // Frame changed, verify data is valid before updating texture
        if (g_currentAnimatedHDBackground->data)
        {
            updateBackgroundTextureHD(
                g_currentAnimatedHDBackground->data,
                g_currentAnimatedHDBackground->width,
                g_currentAnimatedHDBackground->height,
                g_currentAnimatedHDBackground->channels
            );
        }
    }
}

// Pause the current animated HD background
void pauseCurrentAnimatedHDBackground()
{
    if (g_currentAnimatedHDBackground && g_currentAnimatedHDBackground->isAnimated)
    {
        pauseHDBackgroundAnimation(g_currentAnimatedHDBackground);
    }
}

// Resume the current animated HD background
void resumeCurrentAnimatedHDBackground()
{
    if (g_currentAnimatedHDBackground && g_currentAnimatedHDBackground->isAnimated)
    {
        resumeHDBackgroundAnimation(g_currentAnimatedHDBackground);
    }
}

#endif // AITD_UE4