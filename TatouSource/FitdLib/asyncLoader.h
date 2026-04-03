///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Async Asset Loading - High-level wrappers for async asset loading
///////////////////////////////////////////////////////////////////////////////

#pragma once
#ifndef ASYNCLOADER_H
#define ASYNCLOADER_H

#include "jobSystem.h"
#include <functional>

// Asset load completion callback: (userData, assetData, assetSize)
using AssetLoadCallback = std::function<void(void*, char*, size_t)>;

JobHandle asyncLoadPakFile(
    const char* filename, int index, JobSystem::Priority priority,
    AssetLoadCallback callback, void* userData);

JobHandle asyncLoadPakFileWithDependency(
    const char* filename, int primaryIndex, int dependentIndex,
    JobSystem::Priority priority, AssetLoadCallback primaryCallback,
    AssetLoadCallback dependentCallback, void* userData);

JobHandle asyncLoadHDBackground(
    const char* bgName, JobSystem::Priority priority,
    AssetLoadCallback callback, void* userData);

JobHandle asyncLoadVocFile(
    const char* vocName, JobSystem::Priority priority,
    AssetLoadCallback callback, void* userData);

JobHandle asyncLoadFloorData(
    int floorNumber, JobSystem::Priority priority,
    AssetLoadCallback roomCallback, AssetLoadCallback cameraCallback,
    void* userData);

inline bool isAssetLoadComplete(JobHandle handle)
{
    return JobSystem::instance().isJobComplete(handle);
}

inline void waitForAssetLoad(JobHandle handle)
{
    JobSystem::instance().waitForJob(handle);
}

char* syncLoadPakFile(const char* filename, int index);

// Preload cache API
void preloadPak(const char* name, int index);
char* tryGetPreloadedPak(const char* name, int index);
char* waitForPreloadedPak(const char* name, int index);

#endif // ASYNCLOADER_H