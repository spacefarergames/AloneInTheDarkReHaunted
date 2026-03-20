///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Memory management declarations
///////////////////////////////////////////////////////////////////////////////

#pragma once
#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdio.h>
#include <mutex>
#include <unordered_map>
#include <string>

// Memory manager to track allocations and prevent heap corruption
class MemoryManager
{
private:
    struct AllocationInfo
    {
        size_t size;
        const char* file;
        int line;
        const char* type; // "new", "new[]", "malloc", "stbi"
        uint32_t canary; // Guard value to detect corruption
    };

    static const uint32_t CANARY_VALUE = 0xDEADBEEF;
    std::unordered_map<void*, AllocationInfo> allocations;
    std::mutex mutex;
    bool enableLogging;
    FILE* logFile;

    static MemoryManager& instance()
    {
        static MemoryManager mgr;
        return mgr;
    }

    MemoryManager() : enableLogging(true), logFile(nullptr)
    {
        logFile = fopen("memory_debug.log", "w");
        if (logFile)
        {
            fprintf(logFile, "=== Memory Manager Initialized ===\n");
            fflush(logFile);
        }
    }

    ~MemoryManager()
    {
        if (logFile)
        {
            // Report any remaining allocations
            std::lock_guard<std::mutex> lock(mutex);
            if (!allocations.empty())
            {
                fprintf(logFile, "\n=== WARNING: %zu leaked allocations detected ===\n", allocations.size());
                for (const auto& pair : allocations)
                {
                    fprintf(logFile, "Leaked: %p, size %zu, type %s, allocated at %s:%d\n",
                        pair.first, pair.second.size, pair.second.type,
                        pair.second.file ? pair.second.file : "unknown", pair.second.line);
                }
                fflush(logFile);
            }
            fclose(logFile);
        }
    }

public:
    static void* trackAllocation(void* ptr, size_t size, const char* type, const char* file, int line)
    {
        if (!ptr) return nullptr;

        auto& mgr = instance();
        std::lock_guard<std::mutex> lock(mgr.mutex);

        AllocationInfo info;
        info.size = size;
        info.file = file;
        info.line = line;
        info.type = type;
        info.canary = CANARY_VALUE;

        mgr.allocations[ptr] = info;

        if (mgr.logFile && mgr.enableLogging)
        {
            fprintf(mgr.logFile, "ALLOC: %p, size %zu, type %s, at %s:%d\n",
                ptr, size, type, file ? file : "unknown", line);
            fflush(mgr.logFile);
        }

        return ptr;
    }

    static bool validateAllocation(void* ptr, const char* operation, const char* file, int line)
    {
        if (!ptr) return true; // nullptr is valid for delete

        auto& mgr = instance();
        std::lock_guard<std::mutex> lock(mgr.mutex);

        auto it = mgr.allocations.find(ptr);
        if (it == mgr.allocations.end())
        {
            if (mgr.logFile)
            {
                fprintf(mgr.logFile, "ERROR: %s on UNTRACKED pointer %p at %s:%d\n",
                    operation, ptr, file ? file : "unknown", line);
                fprintf(mgr.logFile, "  This could be a double-free or freeing invalid memory!\n");
                fflush(mgr.logFile);
            }
            return false;
        }

        if (it->second.canary != CANARY_VALUE)
        {
            if (mgr.logFile)
            {
                fprintf(mgr.logFile, "ERROR: CORRUPTED canary for pointer %p (expected 0x%X, got 0x%X) at %s:%d\n",
                    ptr, CANARY_VALUE, it->second.canary, file ? file : "unknown", line);
                fprintf(mgr.logFile, "  Original allocation: size %zu, type %s, at %s:%d\n",
                    it->second.size, it->second.type,
                    it->second.file ? it->second.file : "unknown", it->second.line);
                fflush(mgr.logFile);
            }
            return false;
        }

        return true;
    }

    static void untrackAllocation(void* ptr, const char* operation, const char* file, int line)
    {
        if (!ptr) return;

        auto& mgr = instance();
        std::lock_guard<std::mutex> lock(mgr.mutex);

        auto it = mgr.allocations.find(ptr);
        if (it != mgr.allocations.end())
        {
            if (mgr.logFile && mgr.enableLogging)
            {
                fprintf(mgr.logFile, "FREE: %p, size %zu, type %s (was %s:%d), freed via %s at %s:%d\n",
                    ptr, it->second.size, it->second.type,
                    it->second.file ? it->second.file : "unknown", it->second.line,
                    operation, file ? file : "unknown", line);
                fflush(mgr.logFile);
            }
            mgr.allocations.erase(it);
        }
    }

    static void setLogging(bool enabled)
    {
        instance().enableLogging = enabled;
    }

    static void dumpAllocations()
    {
        auto& mgr = instance();
        std::lock_guard<std::mutex> lock(mgr.mutex);

        if (mgr.logFile)
        {
            fprintf(mgr.logFile, "\n=== Current Allocations: %zu ===\n", mgr.allocations.size());
            for (const auto& pair : mgr.allocations)
            {
                fprintf(mgr.logFile, "  %p: size %zu, type %s, at %s:%d\n",
                    pair.first, pair.second.size, pair.second.type,
                    pair.second.file ? pair.second.file : "unknown", pair.second.line);
            }
            fprintf(mgr.logFile, "=== End of Allocations ===\n\n");
            fflush(mgr.logFile);
        }
    }
};

// Safe memory allocation macros with tracking
#define SAFE_NEW(type, file, line) \
    (type*)MemoryManager::trackAllocation(malloc(sizeof(type)), sizeof(type), "new", file, line)

#define SAFE_NEW_ARRAY(type, count, file, line) \
    (type*)MemoryManager::trackAllocation(malloc(sizeof(type) * (count)), sizeof(type) * (count), "new[]", file, line)

#define SAFE_MALLOC(size, file, line) \
    MemoryManager::trackAllocation(malloc(size), size, "malloc", file, line)

#define SAFE_DELETE(ptr, file, line) \
    do { \
        if (MemoryManager::validateAllocation(ptr, "delete", file, line)) { \
            MemoryManager::untrackAllocation(ptr, "delete", file, line); \
            free(ptr); \
        } \
        ptr = nullptr; \
    } while(0)

#define SAFE_DELETE_ARRAY(ptr, file, line) \
    do { \
        if (MemoryManager::validateAllocation(ptr, "delete[]", file, line)) { \
            MemoryManager::untrackAllocation(ptr, "delete[]", file, line); \
            free(ptr); \
        } \
        ptr = nullptr; \
    } while(0)

#define SAFE_FREE(ptr, file, line) \
    do { \
        if (MemoryManager::validateAllocation(ptr, "free", file, line)) { \
            MemoryManager::untrackAllocation(ptr, "free", file, line); \
            free(ptr); \
        } \
        ptr = nullptr; \
    } while(0)

// Track external allocations (e.g., stbi_image)
#define TRACK_ALLOCATION(ptr, size, type) \
    MemoryManager::trackAllocation(ptr, size, type, __FILE__, __LINE__)

#define UNTRACK_ALLOCATION(ptr, type) \
    MemoryManager::untrackAllocation(ptr, type, __FILE__, __LINE__)

#endif // MEMORY_MANAGER_H
