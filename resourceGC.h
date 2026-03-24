///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Resource garbage collector declarations
///////////////////////////////////////////////////////////////////////////////

#pragma once

struct HDBackgroundInfo;

// Lightweight deferred-free garbage collector for large HD background assets.
//
// Problem this solves:
//   loadHDBackground() loads a new animated background (potentially hundreds of MB)
//   BEFORE the old one is freed by setCurrentAnimatedHDBackground(). This peak
//   of old + new simultaneously in RAM causes OOM crashes on camera transitions.
//
// Usage:
//   - Call ResourceGC::tick() once per rendered frame (in osystem_startFrame).
//   - Call ResourceGC::scheduleHDBackground() to defer-free a background that is
//     no longer needed but may still be referenced by an in-flight bgfx frame.
//   - Call ResourceGC::flush() before loading a new large asset to guarantee all
//     previously scheduled frees have completed and RAM is available.
//   - Call ResourceGC::onShutdown() at program exit.

namespace ResourceGC
{
    // Process pending deferred frees. Call once per rendered frame.
    void tick();

    // Schedule an HDBackgroundInfo for deferred free (freed after DEFERRED_FRAMES frames).
    void scheduleHDBackground(HDBackgroundInfo* bg);

    // Immediately free all items currently sitting in the deferred queue.
    // Safe to call at any time; use before loading a new large asset.
    void flush();

    // Full cleanup at program exit; equivalent to flush().
    void onShutdown();

    // Returns the number of items currently waiting in the deferred queue.
    size_t getPendingCount();
}
