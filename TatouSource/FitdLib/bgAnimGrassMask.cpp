///////////////////////////////////////////////////////////////////////////////
// Animated Background Grass-Mask Generator - Implementation
//
// For selected animated HD backgrounds (currently CAMERA07_004), this module
// auto-generates a per-frame R8 grayscale mask that isolates green grass
// pixels which should occlude 3D actors. The mask is fed back to the
// renderer (via osystem_drawAnimatedGrassMask) which draws it as a
// foreground layer on top of the actors.
//
// Generation strategy:
//   - HSV thresholding of the source RGB(A) frame.
//   - Pixels classified as grass when hue is in [60, 160] degrees, saturation
//     >= 0.18 and value in [0.05, 0.95]. These thresholds capture the bright,
//     yellow-green to teal-green grass strands while ignoring stone, sky,
//     shadowed soil, and pure-white highlights.
//
// Caching:
//   - In-memory cache only. Masks are generated on-demand from the HD
//     background frames and kept in RAM for the lifetime of the active
//     animated background. No disk I/O.
//
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "bgAnimGrassMask.h"
#include "hdBackground.h"
#include "consoleLog.h"

#include <vector>
#include <string>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <cerrno>

#ifdef _WIN32
#include <direct.h>
#define GRASS_MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define GRASS_MKDIR(p) mkdir(p, 0755)
#endif

// stb_image_write is implemented (with STB_IMAGE_WRITE_STATIC) in
// rendererBGFX.cpp; including the header here without the IMPLEMENTATION
// macro would still pull in the prototypes but the static linkage means the
// definitions live in another TU and aren't visible. To keep this module
// self-contained (and avoid touching renderer internals just for I/O) we
// hand-roll a tiny PNG writer for 1-channel images using stb's deflate.
//
// In practice we already have stb_image_write available globally; the
// simplest workable path is to drop the _STATIC qualifier in this TU and
// re-include the header WITHOUT the implementation macro - prototypes have
// external linkage by default so the symbols resolve against the impl in
// rendererBGFX.cpp... except STB_IMAGE_WRITE_STATIC there marks them static.
//
// To avoid any cross-TU coupling we bring our OWN compilation of
// stb_image_write into this TU, also as static. That gives us a private,
// duplicate-free implementation - the rendererBGFX.cpp copy stays static
// inside that TU and ours stays static inside this one.
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../ThirdParty/bgfx.cmake/bimg/3rdparty/stb/stb_image_write.h"

// stb_image: same approach, private to this TU.
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "../ThirdParty/bgfx.cmake/bimg/3rdparty/stb/stb_image.h"

// ============================================================================
// Module state
// ============================================================================

namespace
{
    struct GrassMaskState
    {
        bool                       active = false;
        std::string                bgName;          // e.g. "CAMERA07"
        int                        cameraIdx = -1;  // e.g. 4
        HDBackgroundInfo*          bg = nullptr;    // borrowed pointer
        int                        width = 0;
        int                        height = 0;
        int                        version = 0;     // bumps each time `active` toggles or bg changes
        // Per-frame mask (R8). Empty vector == not yet generated/loaded.
        std::vector<std::vector<unsigned char>> cachedMasks;
    };

    GrassMaskState g_state;

    // Whitelist: which animated backgrounds get a grass mask.
    bool isTargetBackground(const char* name, int cameraIdx)
    {
        if (!name)
            return false;
        if (cameraIdx == 4 && std::strcmp(name, "CAMERA07") == 0)
            return true;
        return false;
    }

    // Build the cache directory path for the active background.
    std::string makeCacheDir()
    {
        char path[512];
        std::snprintf(path, sizeof(path), "%s/anim_%s_%03d_grassmask",
                      getHDBackgroundFolder(),
                      g_state.bgName.c_str(),
                      g_state.cameraIdx);
        return std::string(path);
    }

    std::string makeCacheFilePath(int frameIdx)
    {
        // Source frames are named "ezgif-frame-NNN.png" with NNN = 1..N.
        // Match that naming for the masks to keep the mapping obvious on
        // disk.
        char path[640];
        std::snprintf(path, sizeof(path), "%s/grassmask_%03d.png",
                      makeCacheDir().c_str(), frameIdx + 1);
        return std::string(path);
    }

    void ensureCacheDirExists()
    {
        // Defensively create the parent first in case it doesn't exist yet.
        GRASS_MKDIR(getHDBackgroundFolder());
        std::string dir = makeCacheDir();
        int rc = GRASS_MKDIR(dir.c_str());
        static bool s_logged = false;
        if (!s_logged)
        {
            printf(LIFE_TAG "Grass-mask: cache dir = '%s' (mkdir rc=%d, errno=%d)" CON_RESET "\n",
                   dir.c_str(), rc, errno);
            s_logged = true;
        }
    }

    // RGB -> HSV (hue in degrees [0, 360), s/v in [0,1]).
    inline void rgbToHsv(unsigned char r8, unsigned char g8, unsigned char b8,
                         float& h, float& s, float& v)
    {
        float r = r8 / 255.0f;
        float g = g8 / 255.0f;
        float b = b8 / 255.0f;
        float maxC = (std::max)(r, (std::max)(g, b));
        float minC = (std::min)(r, (std::min)(g, b));
        float d = maxC - minC;
        v = maxC;
        s = (maxC > 0.0f) ? (d / maxC) : 0.0f;
        if (d <= 1e-6f)
        {
            h = 0.0f;
        }
        else
        {
            float hh;
            if (maxC == r)      hh = (g - b) / d + (g < b ? 6.0f : 0.0f);
            else if (maxC == g) hh = (b - r) / d + 2.0f;
            else                hh = (r - g) / d + 4.0f;
            h = hh * 60.0f; // [0, 360)
        }
    }

    // Classify a single pixel as grass (returns 255) or non-grass (0).
    inline unsigned char classifyGrass(unsigned char r, unsigned char g, unsigned char b)
    {
        float h, s, v;
        rgbToHsv(r, g, b, h, s, v);

        // Grass band: widened to capture yellow-green (~40) through teal-green (~180).
        if (h < 40.0f || h > 180.0f) return 0;
        if (s < 0.15f)              return 0;
        if (v < 0.05f || v > 0.95f) return 0;

        // Note: original `g <= r` rejection was too strict and dropped
        // yellow-tipped grass strands; rely on the hue band instead.
        return 255;
    }

    // Generate a mask for the given source frame buffer. Source can be 3 or
    // 4 channel; alpha is ignored.
    std::vector<unsigned char> generateMask(const unsigned char* src,
                                            int w, int h, int channels)
    {
        std::vector<unsigned char> mask(static_cast<size_t>(w) * h, 0);
        if (!src || channels < 3)
            return mask;

        const int stride = channels;
        for (int y = 0; y < h; ++y)
        {
            const unsigned char* row = src + (size_t)y * w * stride;
            unsigned char* outRow = mask.data() + (size_t)y * w;
            for (int x = 0; x < w; ++x)
            {
                outRow[x] = classifyGrass(row[0], row[1], row[2]);
                row += stride;
            }
        }
        return mask;
    }

    // DISABLED: Disk caching turned off - all masks generated in memory only
    // Try to load a previously-cached mask PNG from disk. On success fills
    // `out` and returns true.
    bool tryLoadCachedMask(int frameIdx, int expectedW, int expectedH,
                           std::vector<unsigned char>& out)
    {
        // Disk caching disabled - always return false to force in-memory generation
        return false;

        // Original disk load code (disabled):
        /*
        std::string path = makeCacheFilePath(frameIdx);
        int w = 0, hh = 0, ch = 0;
        unsigned char* pixels = stbi_load(path.c_str(), &w, &hh, &ch, 1);
        if (!pixels)
            return false;
        if (w != expectedW || hh != expectedH)
        {
            stbi_image_free(pixels);
            return false;
        }
        out.assign(pixels, pixels + (size_t)w * hh);
        stbi_image_free(pixels);
        return true;
        */
    }

    void writeCachedMask(int frameIdx, const std::vector<unsigned char>& mask,
                         int w, int h)
    {
        // Disk caching disabled - no-op
        // Original disk write code (disabled):
        /*
        ensureCacheDirExists();
        std::string path = makeCacheFilePath(frameIdx);
        // 1-channel PNG.
        int rc = stbi_write_png(path.c_str(), w, h, 1, mask.data(), w);
        if (rc == 0)
        {
            printf(LIFE_WARN "Grass-mask: stbi_write_png FAILED for '%s' (%dx%d, errno=%d)" CON_RESET "\n",
                   path.c_str(), w, h, errno);
        }
        else
        {
            printf(LIFE_OK "Grass-mask: wrote '%s' (%dx%d)" CON_RESET "\n", path.c_str(), w, h);
        }
        */
    }

    // Ensure the in-memory mask for `frameIdx` exists. Returns nullptr if the
    // source frame is not yet loaded (animated backgrounds stream frames
    // asynchronously).
    const std::vector<unsigned char>* ensureMaskForFrame(int frameIdx)
    {
        if (!g_state.active || !g_state.bg)
            return nullptr;
        if (frameIdx < 0 || frameIdx >= g_state.bg->frameCount)
            return nullptr;
        if (frameIdx >= (int)g_state.cachedMasks.size())
            return nullptr;

        std::vector<unsigned char>& slot = g_state.cachedMasks[frameIdx];
        if (!slot.empty())
            return &slot;

        // 1) Disk cache.
        if (g_state.width > 0 && g_state.height > 0 &&
            tryLoadCachedMask(frameIdx, g_state.width, g_state.height, slot))
        {
            return &slot;
        }

        // 2) Generate from source frame, if it has been streamed in.
        unsigned char* srcFrame = nullptr;
        if (g_state.bg->frames && frameIdx < g_state.bg->loadedFrameCount)
        {
            srcFrame = g_state.bg->frames[frameIdx];
        }
        if (!srcFrame)
        {
            static int s_lastReported = -1;
            if (frameIdx != s_lastReported)
            {
                printf(LIFE_TAG "Grass-mask: frame %d not streamed yet (loadedFrameCount=%d)" CON_RESET "\n",
                       frameIdx, g_state.bg->loadedFrameCount);
                s_lastReported = frameIdx;
            }
            return nullptr;
        }

        printf(LIFE_TAG "Grass-mask: generating mask for frame %d (%dx%d ch=%d) [in-memory only]" CON_RESET "\n",
               frameIdx, g_state.bg->width, g_state.bg->height, g_state.bg->channels);
        slot = generateMask(srcFrame,
                            g_state.bg->width,
                            g_state.bg->height,
                            g_state.bg->channels);
        // Disk caching disabled - mask stays in memory only
        // writeCachedMask(frameIdx, slot, g_state.bg->width, g_state.bg->height);
        return &slot;
    }
}

// ============================================================================
// Public API
// ============================================================================

void bgAnimGrassMask_onAnimatedBackgroundLoaded(const char* backgroundName,
                                                int cameraIdx,
                                                HDBackgroundInfo* bgInfo)
{
    // Reset state on every call. Even if the new bg matches the old, the
    // pointer may have changed (animated bgs are reloaded fresh).
    g_state.cachedMasks.clear();
    g_state.bg = nullptr;
    g_state.bgName.clear();
    g_state.cameraIdx = -1;
    g_state.width = 0;
    g_state.height = 0;
    bool wasActive = g_state.active;
    g_state.active = false;

    if (!backgroundName || !bgInfo || !bgInfo->isAnimated)
    {
        if (wasActive)
            g_state.version++;
        return;
    }

    if (!isTargetBackground(backgroundName, cameraIdx))
    {
        if (wasActive)
            g_state.version++;
        return;
    }

    g_state.active    = true;
    g_state.bg        = bgInfo;
    g_state.bgName    = backgroundName;
    g_state.cameraIdx = cameraIdx;
    g_state.width     = bgInfo->width;
    g_state.height    = bgInfo->height;
    g_state.cachedMasks.assign(bgInfo->frameCount > 0 ? bgInfo->frameCount : 0,
                               std::vector<unsigned char>{});
    g_state.version++;

    printf(LIFE_OK "Grass-mask: enabled for %s_%03d (%dx%d, %d frames)" CON_RESET "\n",
           backgroundName, cameraIdx,
           g_state.width, g_state.height, bgInfo->frameCount);
}

bool bgAnimGrassMask_isActive()
{
    return g_state.active && g_state.bg != nullptr;
}

const unsigned char* bgAnimGrassMask_getCurrentFrameMask(int* outW,
                                                         int* outH,
                                                         int* outVersion,
                                                         int* outFrameId)
{
    if (outW)       *outW = g_state.width;
    if (outH)       *outH = g_state.height;
    if (outVersion) *outVersion = g_state.version;
    if (outFrameId) *outFrameId = -1;

    if (!bgAnimGrassMask_isActive())
        return nullptr;

    int frameIdx = g_state.bg->currentFrame;
    if (frameIdx < 0)
        frameIdx = 0;

    const std::vector<unsigned char>* mask = ensureMaskForFrame(frameIdx);
    if (!mask)
        return nullptr;

    if (outFrameId) *outFrameId = frameIdx;
    return mask->data();
}

void bgAnimGrassMask_shutdown()
{
    g_state.cachedMasks.clear();
    g_state.bg = nullptr;
    g_state.bgName.clear();
    g_state.cameraIdx = -1;
    g_state.width = 0;
    g_state.height = 0;
    if (g_state.active)
        g_state.version++;
    g_state.active = false;
}
