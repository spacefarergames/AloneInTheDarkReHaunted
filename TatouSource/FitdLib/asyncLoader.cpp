///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Async Asset Loading Implementation
///////////////////////////////////////////////////////////////////////////////

#include "asyncLoader.h"
#include "common.h"
#include "pak.h"
#include "jobSystem.h"
#include <mutex>
#include <unordered_map>
#include <condition_variable>
#include <memory>
#include <cstring>

struct PakLoadData {
    std::string filename;
    int index;
    AssetLoadCallback callback;
    void* userData;
};

struct VocLoadData {
    std::string vocName;
    AssetLoadCallback callback;
    void* userData;
};

struct HdBgLoadData {
    std::string bgName;
    AssetLoadCallback callback;
    void* userData;
};

JobHandle asyncLoadPakFile(const char* filename, int index, JobSystem::Priority priority,
    AssetLoadCallback callback, void* userData) {
    auto data = std::make_shared<PakLoadData>();
    data->filename = filename;
    data->index = index;
    data->callback = callback;
    data->userData = userData;
    return JobSystem::instance().scheduleJob(JobSystem::JobType::LOAD_PAK, priority, [data]() {
        char* result = loadPak(data->filename.c_str(), data->index);
        size_t size = 0;
        if (result) {
            size = getPakSize(data->filename.c_str(), data->index);
        }
        if (data->callback) {
            data->callback(data->userData, result, size);
        }
    });
}

JobHandle asyncLoadPakFileWithDependency(const char* filename, int primaryIndex, int dependentIndex,
    JobSystem::Priority priority, AssetLoadCallback primaryCallback, AssetLoadCallback dependentCallback,
    void* userData) {
    auto primaryData = std::make_shared<PakLoadData>();
    primaryData->filename = filename;
    primaryData->index = primaryIndex;
    primaryData->callback = primaryCallback;
    primaryData->userData = userData;
    auto dependentData = std::make_shared<PakLoadData>();
    dependentData->filename = filename;
    dependentData->index = dependentIndex;
    dependentData->callback = dependentCallback;
    dependentData->userData = userData;
    JobHandle primaryHandle = JobSystem::instance().scheduleJob(JobSystem::JobType::LOAD_PAK, priority, [primaryData]() {
        char* result = loadPak(primaryData->filename.c_str(), primaryData->index);
        size_t size = 0;
        if (result) {
            size = getPakSize(primaryData->filename.c_str(), primaryData->index);
        }
        if (primaryData->callback) {
            primaryData->callback(primaryData->userData, result, size);
        }
    });
    JobHandle dependentHandle = JobSystem::instance().scheduleJobWithDependencies(
        JobSystem::JobType::LOAD_PAK, priority, [dependentData]() {
        char* result = loadPak(dependentData->filename.c_str(), dependentData->index);
        size_t size = 0;
        if (result) {
            size = getPakSize(dependentData->filename.c_str(), dependentData->index);
        }
        if (dependentData->callback) {
            dependentData->callback(dependentData->userData, result, size);
        }
    }, std::vector<JobHandle>{ primaryHandle });
    return dependentHandle;
}

JobHandle asyncLoadHDBackground(const char* bgName, JobSystem::Priority priority, AssetLoadCallback callback, void* userData) {
    auto data = std::make_shared<HdBgLoadData>();
    data->bgName = bgName;
    data->callback = callback;
    data->userData = userData;
    return JobSystem::instance().scheduleJob(JobSystem::JobType::LOAD_HD_BACKGROUND, priority, [data]() {
        if (data->callback) {
            data->callback(data->userData, nullptr, 0);
        }
    });
}

JobHandle asyncLoadVocFile(const char* vocName, JobSystem::Priority priority, AssetLoadCallback callback, void* userData) {
    auto data = std::make_shared<VocLoadData>();
    data->vocName = vocName;
    data->callback = callback;
    data->userData = userData;
    return JobSystem::instance().scheduleJob(JobSystem::JobType::LOAD_VOC, priority, [data]() {
        if (data->callback) {
            data->callback(data->userData, nullptr, 0);
        }
    });
}

JobHandle asyncLoadFloorData(int floorNumber, JobSystem::Priority priority,
    AssetLoadCallback roomCallback, AssetLoadCallback cameraCallback, void* userData) {
    char floorNameBuf[32];
    sprintf_s(floorNameBuf, "ETAGE%02d", floorNumber);
    std::string floorName = floorNameBuf;
    JobHandle roomHandle = asyncLoadPakFile(floorName.c_str(), 0, priority, roomCallback, userData);
    auto cameraPakData = std::make_shared<PakLoadData>();
    cameraPakData->filename = floorName;
    cameraPakData->index = 1;
    cameraPakData->callback = cameraCallback;
    cameraPakData->userData = userData;
    JobHandle cameraHandle = JobSystem::instance().scheduleJobWithDependencies(
        JobSystem::JobType::LOAD_PAK, priority, [cameraPakData]() {
        char* result = loadPak(cameraPakData->filename.c_str(), cameraPakData->index);
        size_t size = 0;
        if (result) {
            size = getPakSize(cameraPakData->filename.c_str(), cameraPakData->index);
        }
        if (cameraPakData->callback) {
            cameraPakData->callback(cameraPakData->userData, result, size);
        }
    }, std::vector<JobHandle>{ roomHandle });
    return cameraHandle;
}

char* syncLoadPakFile(const char* filename, int index) {
    return loadPak(filename, index);
}

namespace {
    struct PreloadEntry {
        char* data = nullptr;
        bool ready = false;
        std::mutex mtx;
        std::condition_variable cv;
    };
    static std::unordered_map<std::string, std::shared_ptr<PreloadEntry>> s_preloadCache;
    static std::mutex s_cacheMutex;
    std::string makeCacheKey(const char* name, int index) {
        char keyBuf[512];
        sprintf_s(keyBuf, "%s:%d", name, index);
        return std::string(keyBuf);
    }
}

void preloadPak(const char* name, int index) {
    std::string key = makeCacheKey(name, index);
    { std::lock_guard<std::mutex> lock(s_cacheMutex);
        if (s_preloadCache.count(key)) return; }
    auto entry = std::make_shared<PreloadEntry>();
    { std::lock_guard<std::mutex> lock(s_cacheMutex);
        s_preloadCache[key] = entry; }
    std::string nameCopy = name;
    int indexCopy = index;
    JobSystem::instance().scheduleJob(JobSystem::JobType::LOAD_PAK, JobSystem::Priority::HIGH, [entry, nameCopy, indexCopy]() {
        char* result = loadPak(nameCopy.c_str(), indexCopy);
        { std::lock_guard<std::mutex> lock(entry->mtx);
            entry->data = result;
            entry->ready = true; }
        entry->cv.notify_all();
    });
}

char* tryGetPreloadedPak(const char* name, int index) {
    std::string key = makeCacheKey(name, index);
    std::shared_ptr<PreloadEntry> entry;
    { std::lock_guard<std::mutex> lock(s_cacheMutex);
        auto it = s_preloadCache.find(key);
        if (it == s_preloadCache.end()) return nullptr;
        entry = it->second;
        s_preloadCache.erase(it); }
    char* result = nullptr;
    { std::lock_guard<std::mutex> lock(entry->mtx);
        if (entry->ready) result = entry->data; }
    return result;
}

char* waitForPreloadedPak(const char* name, int index) {
    std::string key = makeCacheKey(name, index);
    std::shared_ptr<PreloadEntry> entry;
    { std::lock_guard<std::mutex> lock(s_cacheMutex);
        auto it = s_preloadCache.find(key);
        if (it == s_preloadCache.end()) return nullptr;
        entry = it->second;
        s_preloadCache.erase(it); }
    char* result = nullptr;
    { std::unique_lock<std::mutex> lock(entry->mtx);
        entry->cv.wait(lock, [entry] { return entry->ready; });
        result = entry->data; }
    return result;
}
