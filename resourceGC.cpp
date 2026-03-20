///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Resource garbage collector for texture and memory cleanup
///////////////////////////////////////////////////////////////////////////////

#ifndef AITD_UE4
#include "common.h"
#include "resourceGC.h"
#include "hdBackground.h"
#include <vector>
#include <stdio.h>

namespace ResourceGC
{
    // Number of frames to wait before actually freeing a scheduled item.
    // 2 frames ensures any in-flight bgfx upload of that frame data has completed.
    static const int DEFERRED_FRAMES = 2;

    struct PendingItem
    {
        HDBackgroundInfo* hdBg = nullptr;
        int framesLeft = DEFERRED_FRAMES;

        // Estimated CPU memory in bytes (for logging only)
        size_t estimatedBytes = 0;
    };

    static std::vector<PendingItem> g_pending;

    static size_t estimateBytes(HDBackgroundInfo* bg)
    {
        if (!bg) return 0;
        size_t frames = (bg->isAnimated && bg->frameCount > 0) ? (size_t)bg->frameCount : 1;
        return (size_t)bg->width * bg->height * bg->channels * frames;
    }

    void tick()
    {
        for (auto it = g_pending.begin(); it != g_pending.end(); )
        {
            it->framesLeft--;
            if (it->framesLeft <= 0)
            {
                if (it->hdBg)
                {
                    printf("GC: Freeing deferred HDBackground %p (~%zu MB)\n",
                        it->hdBg, it->estimatedBytes / (1024 * 1024));
                    freeHDBackground(it->hdBg);
                    it->hdBg = nullptr;
                }
                it = g_pending.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    void scheduleHDBackground(HDBackgroundInfo* bg)
    {
        if (!bg) return;

        PendingItem item;
        item.hdBg = bg;
        item.framesLeft = DEFERRED_FRAMES;
        item.estimatedBytes = estimateBytes(bg);
        g_pending.push_back(item);

        printf("GC: Scheduled HDBackground %p (~%zu MB) for deferred free in %d frames\n",
            bg, item.estimatedBytes / (1024 * 1024), DEFERRED_FRAMES);
    }

    void flush()
    {
        if (g_pending.empty()) return;

        size_t totalBytes = 0;
        for (auto& item : g_pending)
            totalBytes += item.estimatedBytes;

        printf("GC: Flushing %zu pending item(s) (~%zu MB total)\n",
            g_pending.size(), totalBytes / (1024 * 1024));

        for (auto& item : g_pending)
        {
            if (item.hdBg)
            {
                freeHDBackground(item.hdBg);
                item.hdBg = nullptr;
            }
        }
        g_pending.clear();
    }

    void onShutdown()
    {
        flush();
    }

    size_t getPendingCount()
    {
        return g_pending.size();
    }
}

#endif // AITD_UE4
