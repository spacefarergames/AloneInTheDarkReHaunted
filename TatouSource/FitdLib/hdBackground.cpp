///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// High-definition background image loading
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "hdBackground.h"
#include "hdArchive.h"
#include "configRemaster.h"
#include "consoleLog.h"
#include "memoryManager.h"
#include "exceptionHandler.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

// Use static linkage to avoid conflicts with bimg's stb_image implementation
// This makes the stb_image functions local to this translation unit
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "../ThirdParty/bgfx.cmake/bimg/3rdparty/stb/stb_image.h"

// Returns the folder path for HD backgrounds
const char* getHDBackgroundFolder()
{
    return "backgrounds_hd";
}

// Check if HD backgrounds are enabled in configuration
bool isHDBackgroundEnabled()
{
    return g_remasterConfig.graphics.enableHDBackgrounds;
}

// Ensure the HD archive is open (lazy init, called on first use)
static void ensureArchiveOpen()
{
    static bool s_tried = false;
    if (s_tried) return;
    s_tried = true;
    HDArchive::open("backgrounds_hd.hda");
}

// Load an image from an archive entry using stb_image
static unsigned char* loadFromArchiveEntry(const HDArchiveEntry* entry, int* width, int* height, int* channels)
{
    size_t bufSize = 0;
    unsigned char* buf = HDArchive::readEntryAlloc(entry, &bufSize);
    if (!buf) return nullptr;

    unsigned char* imageData = nullptr;
    PROTECTED_CALL({
        imageData = stbi_load_from_memory(buf, (int)bufSize, width, height, channels, 0);
        if (imageData)
        {
            TRACK_ALLOCATION(imageData, (*width) * (*height) * (*channels), "stbi_load_from_memory");
        }
    }, "stbi_load_from_memory HD background (archive)");
    free(buf);
    return imageData;
}

// Helper function to scan directory for image files
static std::vector<std::string> scanAnimationDirectory(const char* dirPath)
{
    std::vector<std::string> files;

#ifdef _WIN32
    WIN32_FIND_DATAA findData;
    char searchPath[512];
    snprintf(searchPath, sizeof(searchPath), "%s/*.*", dirPath);

    HANDLE hFind = FindFirstFileA(searchPath, &findData);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        return files;
    }

    do
    {
        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            const char* fileName = findData.cFileName;
            size_t len = strlen(fileName);

            // Check for image extensions
            if (len > 4)
            {
                const char* ext = fileName + len - 4;
                if (_stricmp(ext, ".png") == 0 || 
                    _stricmp(ext, ".tga") == 0 || 
                    _stricmp(ext, ".bmp") == 0)
                {
                    files.push_back(std::string(fileName));
                }
            }
        }
    } while (FindNextFileA(hFind, &findData));

    FindClose(hFind);
#else
    DIR* dir = opendir(dirPath);
    if (!dir)
    {
        return files;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr)
    {
        if (entry->d_type == DT_REG)
        {
            const char* fileName = entry->d_name;
            size_t len = strlen(fileName);

            // Check for image extensions
            if (len > 4)
            {
                const char* ext = fileName + len - 4;
                if (strcasecmp(ext, ".png") == 0 || 
                    strcasecmp(ext, ".tga") == 0 || 
                    strcasecmp(ext, ".bmp") == 0)
                {
                    files.push_back(std::string(fileName));
                }
            }
        }
    }

    closedir(dir);
#endif

    // Sort files alphabetically to ensure consistent frame order
    std::sort(files.begin(), files.end());

    return files;
}

// Helper function to load a single frame from file (optimized with memory mapping)
static unsigned char* loadSingleFrame(const char* filePath, int* width, int* height, int* channels)
{
    unsigned char* imageData = nullptr;

#ifdef _WIN32
    // Use memory-mapped file for faster loading (Windows) - ~2x faster
    HANDLE hFile = INVALID_HANDLE_VALUE;
    HANDLE hMapping = NULL;
    void* pView = nullptr;
    DWORD fileSize = 0;

    PROTECTED_CALL({
        hFile = CreateFileA(filePath, GENERIC_READ, FILE_SHARE_READ, 
                            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE)
        {
            fileSize = GetFileSize(hFile, NULL);
            if (fileSize > 0)
            {
                hMapping = CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
                if (hMapping)
                {
                    pView = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
                    if (pView)
                    {
                        // Load from memory instead of disk
                        imageData = stbi_load_from_memory((unsigned char*)pView, fileSize, 
                                                          width, height, channels, 0);

                        if (imageData)
                        {
                            TRACK_ALLOCATION(imageData, (*width) * (*height) * (*channels), "stbi_load");
                        }

                        UnmapViewOfFile(pView);
                    }
                    CloseHandle(hMapping);
                }
            }
            CloseHandle(hFile);
        }
    }, "stbi_load HD background frame (memory-mapped)");
#else
    // Fallback to regular file loading (Linux/Mac)
    PROTECTED_CALL({
        imageData = stbi_load(filePath, width, height, channels, 0);
        if (imageData)
        {
            TRACK_ALLOCATION(imageData, (*width) * (*height) * (*channels), "stbi_load");
        }
    }, "stbi_load HD background frame");
#endif

    return imageData;
}

// Load an image file from the HD backgrounds folder by filename
// Returns pixel data (caller must free with freeHDImageData), or nullptr on failure
unsigned char* loadHDImageFile(const char* filename, int* width, int* height, int* channels)
{
    // Try archive first
    ensureArchiveOpen();
    if (HDArchive::isOpen())
    {
        const HDArchiveEntry* entry = HDArchive::findEntry(filename);
        if (entry)
        {
            return loadFromArchiveEntry(entry, width, height, channels);
        }
    }

    // Filesystem fallback
    char filePath[512];
    snprintf(filePath, sizeof(filePath), "%s/%s", getHDBackgroundFolder(), filename);

    FILE* testFile = fopen(filePath, "rb");
    if (!testFile)
        return nullptr;
    fclose(testFile);

    return loadSingleFrame(filePath, width, height, channels);
}

// Free image data returned by loadHDImageFile
void freeHDImageData(unsigned char* data)
{
    if (data)
    {
        PROTECTED_CALL({
            UNTRACK_ALLOCATION(data, "stbi_image_free");
            stbi_image_free(data);
        }, "stbi_image_free HD image");
    }
}

// Load an image file from an arbitrary filesystem path
// Returns pixel data (caller must free with freeHDImageData), or nullptr on failure
unsigned char* loadImageFile(const char* fullPath, int* width, int* height, int* channels)
{
    return loadSingleFrame(fullPath, width, height, channels);
}

// Load an image from a memory buffer (e.g. embedded PNG data)
// Returns pixel data (caller must free with freeHDImageData), or nullptr on failure
unsigned char* loadImageFromMemory(const unsigned char* buffer, int bufferSize, int* width, int* height, int* channels)
{
    unsigned char* imageData = nullptr;
    PROTECTED_CALL({
        imageData = stbi_load_from_memory(buffer, bufferSize, width, height, channels, 0);
        if (imageData)
        {
            TRACK_ALLOCATION(imageData, (*width) * (*height) * (*channels), "stbi_load_from_memory");
        }
    }, "stbi_load_from_memory");
    return imageData;
}

// Try to load HD version of background
// Returns nullptr if no HD version found
HDBackgroundInfo* loadHDBackground(const char* backgroundName, int cameraIdx, const char* suffix)
{
    if (!isHDBackgroundEnabled())
    {
        return nullptr;
    }

    // Try archive first, then fall back to filesystem
    ensureArchiveOpen();

    // Build the archive-relative animation prefix (e.g. "anim_CAMERA03_015/")
    char animPrefix[256];
    if (suffix && suffix[0] != '\0')
    {
        snprintf(animPrefix, sizeof(animPrefix), "anim_%s_%03d_%s/",
                 backgroundName, cameraIdx, suffix);
    }
    else
    {
        snprintf(animPrefix, sizeof(animPrefix), "anim_%s_%03d/",
                 backgroundName, cameraIdx);
    }

    // Also build the filesystem directory path for fallback
    char animDirPath[512];
    snprintf(animDirPath, sizeof(animDirPath), "%s/%.*s",
             getHDBackgroundFolder(),
             (int)(strlen(animPrefix) - 1), animPrefix); // strip trailing '/'

    printf(HDBG_TAG "Looking for animated background: %s (name='%s', camera=%d)\n", 
           animDirPath, backgroundName, cameraIdx);

    // ---- Try animated background from archive ----
    std::vector<const HDArchiveEntry*> archiveAnimEntries;
    if (HDArchive::isOpen())
    {
        archiveAnimEntries = HDArchive::listByPrefix(animPrefix);
    }

    // Choose source: archive entries or filesystem scan
    size_t totalFrames = 0;
    std::vector<std::string> animFiles; // only used for filesystem fallback

    if (!archiveAnimEntries.empty())
    {
        totalFrames = archiveAnimEntries.size();
        printf(HDBG_OK "Found animated background in archive: %s with %zu frames\n",
               animPrefix, totalFrames);
    }
    else
    {
        // Filesystem fallback for animation directory
        animFiles = scanAnimationDirectory(animDirPath);
        totalFrames = animFiles.size();
        if (totalFrames > 0)
        {
            printf(HDBG_OK "Found animated background directory: %s with %zu frames\n",
                   animDirPath, totalFrames);
        }
    }

    if (totalFrames > 0)
    {
        // Allocate frame pointers array
        unsigned char** frames = (unsigned char**)malloc(sizeof(unsigned char*) * totalFrames);
        if (!frames)
        {
            return nullptr;
        }

        // Initialize all frame pointers to nullptr
        for (size_t i = 0; i < totalFrames; i++)
        {
            frames[i] = nullptr;
        }

        int width = 0, height = 0, channels = 0;
        int loadedFrames = 0;

        // Determine minimum frames needed for smooth playback
        size_t thirtyPercent = totalFrames * 3 / 10;
        size_t minFramesToLoad = (totalFrames < 10) ? totalFrames : 
                                  (thirtyPercent > 3 ? thirtyPercent : 3);

        printf(HDBG_TAG "Pre-loading first %zu frames for immediate playback...\n", minFramesToLoad);

        for (size_t i = 0; i < minFramesToLoad && i < totalFrames; i++)
        {
            int frameWidth, frameHeight, frameChannels;
            unsigned char* frameData = nullptr;

            if (!archiveAnimEntries.empty())
            {
                frameData = loadFromArchiveEntry(archiveAnimEntries[i],
                                                 &frameWidth, &frameHeight, &frameChannels);
            }
            else
            {
                char frameFilePath[512];
                snprintf(frameFilePath, sizeof(frameFilePath), "%s/%s",
                         animDirPath, animFiles[i].c_str());
                frameData = loadSingleFrame(frameFilePath,
                                            &frameWidth, &frameHeight, &frameChannels);
            }

            if (!frameData)
            {
                printf(HDBG_WARN "Failed to load initial frame %zu" CON_RESET "\n", i);
                continue;
            }

            if (loadedFrames == 0)
            {
                width = frameWidth;
                height = frameHeight;
                channels = frameChannels;
            }
            else if (frameWidth != width || frameHeight != height || frameChannels != channels)
            {
                printf(HDBG_WARN "Frame %zu has inconsistent dimensions (%dx%d, %d channels), expected (%dx%d, %d channels)" CON_RESET "\n",
                       i, frameWidth, frameHeight, frameChannels, width, height, channels);
                stbi_image_free(frameData);
                continue;
            }

            frames[i] = frameData;
            loadedFrames++;
        }

        if (loadedFrames == 0 || frames[0] == nullptr)
        {
            for (size_t i = 0; i < totalFrames; i++)
            {
                if (frames[i])
                {
                    stbi_image_free(frames[i]);
                }
            }
            free(frames);
            printf(HDBG_ERR "Failed to load any frames from animated background" CON_RESET "\n");
        }
        else
        {
            printf(HDBG_OK "Loaded %d initial frames, starting playback...\n", loadedFrames);

            bool allInitialFramesLoaded = (loadedFrames >= (int)totalFrames);

            if (!allInitialFramesLoaded)
            {
                printf(HDBG_TAG "Loading remaining %zu frames...\n", totalFrames - loadedFrames);

                for (size_t i = minFramesToLoad; i < totalFrames; i++)
                {
                    int frameWidth, frameHeight, frameChannels;
                    unsigned char* frameData = nullptr;

                    if (!archiveAnimEntries.empty())
                    {
                        frameData = loadFromArchiveEntry(archiveAnimEntries[i],
                                                         &frameWidth, &frameHeight, &frameChannels);
                    }
                    else
                    {
                        char frameFilePath[512];
                        snprintf(frameFilePath, sizeof(frameFilePath), "%s/%s",
                                 animDirPath, animFiles[i].c_str());
                        frameData = loadSingleFrame(frameFilePath,
                                                    &frameWidth, &frameHeight, &frameChannels);
                    }

                    if (!frameData)
                    {
                        printf(HDBG_WARN "Failed to load frame %zu" CON_RESET "\n", i);
                        continue;
                    }

                    if (frameWidth != width || frameHeight != height || frameChannels != channels)
                    {
                        printf(HDBG_WARN "Frame %zu has inconsistent dimensions, skipping" CON_RESET "\n", i);
                        stbi_image_free(frameData);
                        continue;
                    }

                    frames[i] = frameData;
                    loadedFrames++;
                }
            }

            HDBackgroundInfo* bgInfo = (HDBackgroundInfo*)SAFE_MALLOC(sizeof(HDBackgroundInfo), __FILE__, __LINE__);
            if (!bgInfo)
            {
                for (size_t i = 0; i < totalFrames; i++)
                {
                    if (frames[i])
                    {
                        stbi_image_free(frames[i]);
                    }
                }
                free(frames);
                return nullptr;
            }

            bgInfo->data = frames[0];
            bgInfo->width = width;
            bgInfo->height = height;
            bgInfo->channels = channels;
            bgInfo->isIndexed = false;
            bgInfo->isAnimated = true;
            bgInfo->frameCount = totalFrames;
            bgInfo->frames = frames;
            bgInfo->currentFrame = 0;
            bgInfo->frameTimer = 0.0f;
            bgInfo->frameTime = 0.08f;
            bgInfo->isPaused = false;
            bgInfo->minFramesForPlayback = minFramesToLoad;
            bgInfo->loadedFrameCount = loadedFrames;
            bgInfo->allFramesLoaded = (loadedFrames >= (int)totalFrames);

            if (bgInfo->allFramesLoaded)
            {
                printf(HDBG_OK "Loaded animated HD background: %s (%dx%d, %d channels, %d/%d frames - READY)\n", 
                       animDirPath, width, height, channels, loadedFrames, (int)totalFrames);
            }
            else
            {
                printf(HDBG_OK "Loaded animated HD background: %s (%dx%d, %d channels, %d/%d frames - STREAMING)\n", 
                       animDirPath, width, height, channels, loadedFrames, (int)totalFrames);
            }

            return bgInfo;
        }
    }

    // No animated background found or failed to load, try static background
    const char* extensions[] = { ".png", ".tga", ".bmp" };

    // ---- Try static background from archive ----
    if (HDArchive::isOpen())
    {
        for (int i = 0; i < 3; i++)
        {
            char entryName[256];
            if (suffix && suffix[0] != '\0')
            {
                snprintf(entryName, sizeof(entryName), "%s_%03d_%s%s",
                         backgroundName, cameraIdx, suffix, extensions[i]);
            }
            else
            {
                snprintf(entryName, sizeof(entryName), "%s_%03d%s",
                         backgroundName, cameraIdx, extensions[i]);
            }

            const HDArchiveEntry* entry = HDArchive::findEntry(entryName);
            if (!entry)
                continue;

            int width, height, channels;
            unsigned char* imageData = loadFromArchiveEntry(entry, &width, &height, &channels);
            if (!imageData)
            {
                printf(HDBG_ERR "Failed to load HD background from archive: %s" CON_RESET "\n", entryName);
                continue;
            }

            int scale = g_remasterConfig.graphics.backgroundScale;
            int expectedWidth = 320 * scale;
            int expectedHeight = 200 * scale;

            if (width != expectedWidth || height != expectedHeight)
            {
                printf(HDBG_WARN "HD background %s has unexpected resolution %dx%d (expected %dx%d)" CON_RESET "\n",
                       entryName, width, height, expectedWidth, expectedHeight);
            }

            HDBackgroundInfo* bgInfo = (HDBackgroundInfo*)SAFE_MALLOC(sizeof(HDBackgroundInfo), __FILE__, __LINE__);
            if (!bgInfo)
            {
                stbi_image_free(imageData);
                return nullptr;
            }

            bgInfo->data = imageData;
            bgInfo->width = width;
            bgInfo->height = height;
            bgInfo->channels = channels;
            bgInfo->isIndexed = false;
            bgInfo->isAnimated = false;
            bgInfo->frameCount = 1;
            bgInfo->frames = nullptr;
            bgInfo->currentFrame = 0;
            bgInfo->frameTimer = 0.0f;
            bgInfo->frameTime = 0.08f;
            bgInfo->isPaused = false;
            bgInfo->allFramesLoaded = true;
            bgInfo->minFramesForPlayback = 1;
            bgInfo->loadedFrameCount = 1;

            printf(HDBG_OK "Loaded HD background from archive: %s (%dx%d, %d channels)\n",
                   entryName, width, height, channels);

            return bgInfo;
        }
    }

    // ---- Filesystem fallback for static background ----
    char filePath[512];

    for (int i = 0; i < 3; i++)
    {
        if (suffix && suffix[0] != '\0')
        {
            snprintf(filePath, sizeof(filePath), "%s/%s_%03d_%s%s", 
                     getHDBackgroundFolder(), backgroundName, cameraIdx, suffix, extensions[i]);
        }
        else
        {
            snprintf(filePath, sizeof(filePath), "%s/%s_%03d%s", 
                     getHDBackgroundFolder(), backgroundName, cameraIdx, extensions[i]);
        }

        FILE* testFile = fopen(filePath, "rb");
        if (!testFile)
        {
            continue;
        }
        fclose(testFile);

        int width, height, channels;
        unsigned char* imageData = loadSingleFrame(filePath, &width, &height, &channels);

        if (!imageData)
        {
            printf(HDBG_ERR "Failed to load HD background: %s" CON_RESET "\n", filePath);
            continue;
        }

        int scale = g_remasterConfig.graphics.backgroundScale;
        int expectedWidth = 320 * scale;
        int expectedHeight = 200 * scale;

        if (width != expectedWidth || height != expectedHeight)
        {
            printf(HDBG_WARN "HD background %s has unexpected resolution %dx%d (expected %dx%d)" CON_RESET "\n",
                   filePath, width, height, expectedWidth, expectedHeight);
        }

        HDBackgroundInfo* bgInfo = (HDBackgroundInfo*)SAFE_MALLOC(sizeof(HDBackgroundInfo), __FILE__, __LINE__);
        if (!bgInfo)
        {
            stbi_image_free(imageData);
            return nullptr;
        }

        bgInfo->data = imageData;
        bgInfo->width = width;
        bgInfo->height = height;
        bgInfo->channels = channels;
        bgInfo->isIndexed = false;
        bgInfo->isAnimated = false;
        bgInfo->frameCount = 1;
        bgInfo->frames = nullptr;
        bgInfo->currentFrame = 0;
        bgInfo->frameTimer = 0.0f;
        bgInfo->frameTime = 0.08f;
        bgInfo->isPaused = false;
        bgInfo->allFramesLoaded = true;
        bgInfo->minFramesForPlayback = 1;
        bgInfo->loadedFrameCount = 1;

        printf(HDBG_OK "Loaded HD background: %s (%dx%d, %d channels)\n", 
               filePath, width, height, channels);

        return bgInfo;
    }

    // No HD background found
    return nullptr;
}
// Free HD background data
void freeHDBackground(HDBackgroundInfo* bgInfo)
{
    if (bgInfo)
    {
        if (bgInfo->isAnimated && bgInfo->frames)
        {
            // Free all animation frames
            for (int i = 0; i < bgInfo->frameCount; i++)
            {
                if (bgInfo->frames[i])
                {
                    PROTECTED_CALL({
                        UNTRACK_ALLOCATION(bgInfo->frames[i], "stbi_image_free");
                        stbi_image_free(bgInfo->frames[i]);
                    }, "stbi_image_free HD background frame");
                    bgInfo->frames[i] = nullptr; // Explicitly null each frame
                }
            }
            free(bgInfo->frames);
            bgInfo->frames = nullptr;
            bgInfo->data = nullptr; // data points to first frame, already freed
            bgInfo->frameCount = 0; // Reset frame count
            bgInfo->currentFrame = 0; // Reset current frame
            bgInfo->isAnimated = false; // Clear animation flag
        }
        else if (bgInfo->data)
        {
            // Free single frame
            PROTECTED_CALL({
                UNTRACK_ALLOCATION(bgInfo->data, "stbi_image_free");
                stbi_image_free(bgInfo->data);
                bgInfo->data = nullptr; // Prevent double-free
            }, "stbi_image_free HD background");
        }

        // Clear all remaining fields before freeing the structure
        bgInfo->width = 0;
        bgInfo->height = 0;
        bgInfo->channels = 0;
        bgInfo->allFramesLoaded = false;
        bgInfo->isPaused = false;
        bgInfo->frameTimer = 0.0f;

        SAFE_FREE(bgInfo, __FILE__, __LINE__);
    }
}

// Update animated background frame based on delta time
// Returns true if frame changed
bool updateHDBackgroundAnimation(HDBackgroundInfo* bgInfo, float deltaTime)
{
    // Don't animate if not ready or paused
    // Allow animation once minimum frames are loaded (not just all frames)
    if (!bgInfo || !bgInfo->isAnimated || bgInfo->frameCount <= 1 || bgInfo->isPaused)
    {
        return false;
    }

    // Check if we have enough frames loaded to start animating
    if (bgInfo->loadedFrameCount < (int)bgInfo->minFramesForPlayback)
    {
        return false;
    }

    // Cap deltaTime to prevent speed-up when the game catches up after loading
    // or when frames are delayed (e.g., camera transition). This ensures the
    // animation never advances more than one frame per update.
    if (deltaTime > bgInfo->frameTime)
    {
        deltaTime = bgInfo->frameTime;
    }

    bgInfo->frameTimer += deltaTime;

    if (bgInfo->frameTimer >= bgInfo->frameTime)
    {
        // Advance to next frame - reset timer cleanly to avoid drift
        bgInfo->frameTimer = 0.0f;

        int nextFrame = (bgInfo->currentFrame + 1) % bgInfo->frameCount;

        // Find next valid frame (skip null frames)
        int attempts = 0;
        while (bgInfo->frames[nextFrame] == nullptr && attempts < bgInfo->frameCount)
        {
            nextFrame = (nextFrame + 1) % bgInfo->frameCount;
            attempts++;
        }

        // Only update if we found a valid frame
        if (bgInfo->frames[nextFrame] != nullptr)
        {
            bgInfo->currentFrame = nextFrame;
            bgInfo->data = bgInfo->frames[nextFrame];
            return true;
        }
    }

    return false;
}

// Get current frame data for rendering
unsigned char* getHDBackgroundCurrentFrame(HDBackgroundInfo* bgInfo)
{
    if (!bgInfo)
    {
        return nullptr;
    }

    if (bgInfo->isAnimated && bgInfo->frames && bgInfo->currentFrame < bgInfo->frameCount)
    {
        return bgInfo->frames[bgInfo->currentFrame];
    }

    return bgInfo->data;
}

// HD Frame border storage
static HDFrameBorderInfo g_hdFrameBorders[FRAME_BORDER_COUNT] = {};
static bool g_hdFrameBordersInitialized = false;

// Frame border file names (expected in game folder)
static const char* g_frameBorderFileNames[FRAME_BORDER_COUNT] = {
    "frame_top.png",
    "frame_bottom.png",
    "frame_left.png",
    "frame_right.png"
};

// Load HD frame border textures from PNG files
bool loadHDFrameBorders()
{
    if (!isHDBackgroundEnabled())
    {
        return false;
    }
    
    // Free any previously loaded borders
    freeHDFrameBorders();
    
    bool anyLoaded = false;
    
    for (int i = 0; i < FRAME_BORDER_COUNT; i++)
    {
        g_hdFrameBorders[i].loaded = false;
        g_hdFrameBorders[i].data = nullptr;
        
        // Try to load from game folder
        char filePath[512];
        snprintf(filePath, sizeof(filePath), "%s", g_frameBorderFileNames[i]);
        
        // Check if file exists
        FILE* testFile = fopen(filePath, "rb");
        if (!testFile)
        {
            // Try in backgrounds_hd folder
            snprintf(filePath, sizeof(filePath), "%s/%s", getHDBackgroundFolder(), g_frameBorderFileNames[i]);
            testFile = fopen(filePath, "rb");
            if (!testFile)
            {
                continue;
            }
        }
        fclose(testFile);
        
        // Load the image using stb_image
        int width, height, channels;
        unsigned char* imageData = nullptr;

        PROTECTED_CALL({
            imageData = stbi_load(filePath, &width, &height, &channels, 4); // Force RGBA
            if (imageData)
            {
                // Track this allocation for debugging
                TRACK_ALLOCATION(imageData, width * height * 4, "stbi_load");
            }
        }, "stbi_load HD frame border");

        if (!imageData)
        {
            printf(HDBG_ERR "Failed to load HD frame border: %s" CON_RESET "\n", filePath);
            continue;
        }
        
        g_hdFrameBorders[i].data = imageData;
        g_hdFrameBorders[i].width = width;
        g_hdFrameBorders[i].height = height;
        g_hdFrameBorders[i].channels = 4; // We forced RGBA
        g_hdFrameBorders[i].loaded = true;
        
        printf(HDBG_OK "Loaded HD frame border: %s (%dx%d)\n", filePath, width, height);
        anyLoaded = true;
    }
    
    g_hdFrameBordersInitialized = anyLoaded;
    return anyLoaded;
}

// Free HD frame border textures
void freeHDFrameBorders()
{
    for (int i = 0; i < FRAME_BORDER_COUNT; i++)
    {
        if (g_hdFrameBorders[i].data)
        {
            PROTECTED_CALL({
                UNTRACK_ALLOCATION(g_hdFrameBorders[i].data, "stbi_image_free");
                stbi_image_free(g_hdFrameBorders[i].data);
                g_hdFrameBorders[i].data = nullptr; // Prevent double-free
            }, "stbi_image_free frame border");
        }
        g_hdFrameBorders[i].loaded = false;
    }
    g_hdFrameBordersInitialized = false;
}

// Check if HD frame borders are available
bool areHDFrameBordersLoaded()
{
    return g_hdFrameBordersInitialized;
}

// Get HD frame border info for a specific border type
HDFrameBorderInfo* getHDFrameBorder(FrameBorderType type)
{
    if (type >= 0 && type < FRAME_BORDER_COUNT && g_hdFrameBorders[type].loaded)
    {
        return &g_hdFrameBorders[type];
    }
    return nullptr;
}

// Pause HD background animation
void pauseHDBackgroundAnimation(HDBackgroundInfo* bgInfo)
{
    if (bgInfo && bgInfo->isAnimated)
    {
        bgInfo->isPaused = true;
    }
}

// Resume HD background animation
void resumeHDBackgroundAnimation(HDBackgroundInfo* bgInfo)
{
    if (bgInfo && bgInfo->isAnimated)
    {
        bgInfo->isPaused = false;
    }
}
