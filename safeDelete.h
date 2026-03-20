///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Safe memory deletion utilities
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <windows.h>

// Safe deletion wrapper that catches heap corruption
namespace SafeDelete {

    // Statistics for debugging
    struct Stats {
        int successfulDeletes = 0;
        int caughtExceptions = 0;
        int doubleDeletesPrevented = 0;
    };

    inline Stats stats;

    // Safe delete for sBody objects with heap corruption protection
    template<typename T>
    inline bool SafeDeleteObject(T*& ptr) {
        if (!ptr) {
            return true; // Already null
        }

        __try {
            // Check if memory is accessible
            volatile char test = *((char*)ptr);
            (void)test;

            // Try to delete
            delete ptr;
            ptr = nullptr;
            stats.successfulDeletes++;
            return true;
        }
        __except (GetExceptionCode() == STATUS_ACCESS_VIOLATION ||
                  GetExceptionCode() == STATUS_HEAP_CORRUPTION ? 
                  EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
            
            stats.caughtExceptions++;
            
            #ifdef _DEBUG
            char msg[256];
            sprintf_s(msg, sizeof(msg), 
                "SafeDelete caught exception 0x%08lX for pointer 0x%p - continuing\n", 
                GetExceptionCode(), ptr);
            OutputDebugStringA(msg);
            #endif

            // Set to null and continue
            ptr = nullptr;
            return false;
        }
    }

    // Get statistics
    inline const Stats& GetStats() {
        return stats;
    }

    // Reset statistics
    inline void ResetStats() {
        stats = Stats();
    }
}
