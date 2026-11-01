///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Automatic original-PAK background dumping (debug/asset extraction)
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "consoleLog.h"
#include "palette.h"
#include "pak.h"
#include "backgroundDump.h"
#include "hdAutoUpscale.h"

// stb_image_write (kept static to avoid linker conflicts with other TUs that
// also include the implementation, e.g. sequence.cpp / rendererBGFX.cpp).
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../ThirdParty/bgfx.cmake/bimg/3rdparty/stb/stb_image_write.h"

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <string>

static bool bgDumpFileExists(const char* path)
{
    FILE* file = nullptr;
#ifdef _WIN32
    fopen_s(&file, path, "rb");
#else
    file = fopen(path, "rb");
#endif
    if (!file) return false;
    fclose(file);
    return true;
}

extern "C" { extern char homePath[512]; }

static void ensureBgDumpDirectory(const char* dir)
{
#ifdef _WIN32
    _mkdir(dir);
#else
    mkdir(dir, 0755);
#endif
}

static bool dumpOneBackground(const char* outDir, const char* baseName,
                              int cameraIdx,
                              const unsigned char* paletted,
                              const palette_t& palette,
                              bool overwriteExisting)
{
    char filePath[1024];
    snprintf(filePath, sizeof(filePath),
             "%s/%s_%03d.png",
             outDir, baseName, cameraIdx);
    if (!overwriteExisting && bgDumpFileExists(filePath)) return false;

    unsigned char* rgba = (unsigned char*)malloc(320 * 200 * 4);
    if (!rgba)
        return false;

    for (int i = 0; i < 320 * 200; i++)
    {
        unsigned char idx = paletted[i];
        rgba[i * 4 + 0] = palette[idx][0];
        rgba[i * 4 + 1] = palette[idx][1];
        rgba[i * 4 + 2] = palette[idx][2];
        rgba[i * 4 + 3] = 255;
    }

    bool ok = stbi_write_png(filePath, 320, 200, 4, rgba, 320 * 4) != 0;
    free(rgba);
    return ok;
}

int dumpAllBackgroundsTo(const char* outputDirectory, bool overwriteExisting)
{
    char dirPath[512];
    snprintf(dirPath, sizeof(dirPath), "%s", outputDirectory);
    ensureBgDumpDirectory(dirPath);

    printf(MAIN_TAG "Dumping all original PAK backgrounds to %s ...\n", dirPath);

    int totalDumped = 0;

    // For non-AITD1 titles each background carries its own palette at
    // offset 64000 inside the PAK entry. For AITD1 we fall back to the
    // already-loaded global game palette (loaded from ITD_RESS).
    palette_t fallbackPalette = currentGamePalette;

    // CAMERAxx PAKs are typically named CAMERA00..CAMERA09 in shipping data,
    // but probe a wide range so we capture any non-standard floors too.
    for (int floor = 0; floor < 32; floor++)
    {
        char pakBase[16];
        snprintf(pakBase, sizeof(pakBase), "CAMERA%02d", floor);

        unsigned int numFiles = PAK_getNumFiles(pakBase);
        if (numFiles == 0 || numFiles > 4096)
            continue;

        for (unsigned int idx = 0; idx < numFiles; idx++)
        {
            char outputPath[1024];
            snprintf(outputPath, sizeof(outputPath), "%s/%s_%03u.png", dirPath, pakBase, idx);
            if (!overwriteExisting && bgDumpFileExists(outputPath)) continue;

            int size = getPakSize(pakBase, idx);
            if (size < 64000)
                continue;

            char* buf = loadPak(pakBase, idx);
            if (!buf)
                continue;

            const unsigned char* paletted = (const unsigned char*)buf;

            palette_t bgPalette;
            if (g_gameId >= JACK && size >= 64000 + 256 * 3)
            {
                copyPalette(buf + 64000, bgPalette);
                prepareCameraPalette(bgPalette);
            }
            else
            {
                bgPalette = fallbackPalette;
            }

            if (dumpOneBackground(dirPath, pakBase, (int)idx,
                                  paletted, bgPalette, overwriteExisting))
            {
                totalDumped++;
            }

            free(buf);
        }
    }

    // Menu, book, inventory and special camera screens also live in ITD_RESS
    // in every trilogy title. Entries are a 320x200 indexed image, optionally
    // followed by the AITD2+ per-image palette.
    {
        unsigned int numFiles = PAK_getNumFiles("ITD_RESS");
        for (unsigned int idx = 0; idx < numFiles; idx++)
        {
            int size = getPakSize("ITD_RESS", idx);
            if (size < 64000)
                continue;

            char filePath[1024];
            snprintf(filePath, sizeof(filePath),
                     "%s/ITD_RESS_%03u.png", dirPath, idx);
            if (!overwriteExisting && bgDumpFileExists(filePath)) continue;

            char* buf = loadPak("ITD_RESS", idx);
            if (!buf)
                continue;

            unsigned char* rgba = (unsigned char*)malloc(320 * 200 * 4);
            if (rgba)
            {
                const unsigned char* paletted = (const unsigned char*)buf;
                palette_t resourcePalette = fallbackPalette;
                if (g_gameId >= JACK && size >= 64000 + 256 * 3)
                {
                    copyPalette(buf + 64000, resourcePalette);
                    convertPaletteIfRequired(resourcePalette);
                }
                for (int i = 0; i < 320 * 200; i++)
                {
                    unsigned char p = paletted[i];
                    rgba[i * 4 + 0] = resourcePalette[p][0];
                    rgba[i * 4 + 1] = resourcePalette[p][1];
                    rgba[i * 4 + 2] = resourcePalette[p][2];
                    rgba[i * 4 + 3] = 255;
                }
                if (stbi_write_png(filePath, 320, 200, 4, rgba, 320 * 4))
                    totalDumped++;
                free(rgba);
            }

            free(buf);
        }
    }

    printf(MAIN_TAG "Background dump complete: %d image(s) written.\n", totalDumped);
    return totalDumped;
}

void dumpAllBackgrounds()
{
    char dirPath[512];
    snprintf(dirPath, sizeof(dirPath), "%sbackgrounds_dump", homePath);
    dumpAllBackgroundsTo(dirPath);
}

namespace
{
struct IncrementalBackgroundDump
{
    std::string outputDirectory;
    std::string upscaleOutputDirectory;
    palette_t fallbackPalette{};
    int floor = 0;
    unsigned int entry = 0;
    bool resources = false;
    bool overwrite = false;
};

std::deque<IncrementalBackgroundDump>& incrementalBackgroundDumps()
{
    static std::deque<IncrementalBackgroundDump> requests;
    return requests;
}

static void queueBackgroundUpscale(const IncrementalBackgroundDump& request,
                                   const char* baseName, unsigned int index)
{
    if (request.upscaleOutputDirectory.empty()) return;
    char sourcePath[1024], targetPath[1024];
    snprintf(sourcePath, sizeof(sourcePath), "%s/%s_%03u.png",
             request.outputDirectory.c_str(), baseName, index);
    snprintf(targetPath, sizeof(targetPath), "%s/%s_%03u.png",
             request.upscaleOutputDirectory.c_str(), baseName, index);
    hdAutoUpscale_QueueFile(sourcePath, targetPath);
}
}

void beginIncrementalBackgroundDump(const char* outputDirectory,
                                    const char* upscaleOutputDirectory,
                                    bool overwriteExisting)
{
    if (!outputDirectory || !*outputDirectory) return;
    for (const IncrementalBackgroundDump& request : incrementalBackgroundDumps())
        if (request.outputDirectory == outputDirectory) return;

    IncrementalBackgroundDump request;
    request.outputDirectory = outputDirectory;
    if (upscaleOutputDirectory) request.upscaleOutputDirectory = upscaleOutputDirectory;
    request.fallbackPalette = currentGamePalette;
    request.overwrite = overwriteExisting;
    ensureBgDumpDirectory(request.outputDirectory.c_str());
    incrementalBackgroundDumps().push_back(std::move(request));
    printf(MAIN_TAG "Queued non-blocking background dump to %s\n", outputDirectory);
}

bool isIncrementalBackgroundDumpActive()
{
    return !incrementalBackgroundDumps().empty();
}

bool pumpIncrementalBackgroundDump()
{
    std::deque<IncrementalBackgroundDump>& requests = incrementalBackgroundDumps();
    if (requests.empty()) return false;
    IncrementalBackgroundDump& request = requests.front();

    while (!request.resources && request.floor < 32)
    {
        char pakBase[16];
        snprintf(pakBase, sizeof(pakBase), "CAMERA%02d", request.floor);
        const unsigned int count = PAK_getNumFiles(pakBase);
        if (count == 0 || count > 4096 || request.entry >= count)
        {
            ++request.floor;
            request.entry = 0;
            continue;
        }

        const unsigned int index = request.entry++;
        char outputPath[1024];
        snprintf(outputPath, sizeof(outputPath), "%s/%s_%03u.png",
                 request.outputDirectory.c_str(), pakBase, index);
        if (!request.overwrite && bgDumpFileExists(outputPath))
        {
            queueBackgroundUpscale(request, pakBase, index);
            continue;
        }
        const int size = getPakSize(pakBase, index);
        char* buffer = size >= 64000 ? loadPak(pakBase, index) : nullptr;
        if (!buffer) continue;

        palette_t palette = request.fallbackPalette;
        if (g_gameId >= JACK && size >= 64000 + 256 * 3)
        {
            copyPalette(buffer + 64000, palette);
            prepareCameraPalette(palette);
        }
        const bool written = dumpOneBackground(request.outputDirectory.c_str(), pakBase,
                                               (int)index, (const unsigned char*)buffer,
                                               palette, request.overwrite);
        free(buffer);
        if (written) queueBackgroundUpscale(request, pakBase, index);
        return true;
    }

    request.resources = true;
    const unsigned int resourceCount = PAK_getNumFiles("ITD_RESS");
    while (request.entry < resourceCount)
    {
        const unsigned int index = request.entry++;
        char outputPath[1024];
        snprintf(outputPath, sizeof(outputPath), "%s/ITD_RESS_%03u.png",
                 request.outputDirectory.c_str(), index);
        if (!request.overwrite && bgDumpFileExists(outputPath))
        {
            queueBackgroundUpscale(request, "ITD_RESS", index);
            continue;
        }
        const int size = getPakSize("ITD_RESS", index);
        char* buffer = size >= 64000 ? loadPak("ITD_RESS", index) : nullptr;
        if (!buffer) continue;

        palette_t palette = request.fallbackPalette;
        if (g_gameId >= JACK && size >= 64000 + 256 * 3)
        {
            copyPalette(buffer + 64000, palette);
            convertPaletteIfRequired(palette);
        }
        const bool written = dumpOneBackground(request.outputDirectory.c_str(), "ITD_RESS",
                                               (int)index, (const unsigned char*)buffer,
                                               palette, request.overwrite);
        free(buffer);
        if (written) queueBackgroundUpscale(request, "ITD_RESS", index);
        return true;
    }

    // Mark the corrected-palette extraction complete only after every entry
    // has actually been visited. Interrupted runs safely resume next launch.
    char recipePath[1024];
    snprintf(recipePath, sizeof(recipePath), "%s/.fitd_dump_recipe",
             request.outputDirectory.c_str());
    FILE* recipe = nullptr;
#ifdef _WIN32
    fopen_s(&recipe, recipePath, "wb");
#else
    recipe = fopen(recipePath, "wb");
#endif
    if (recipe)
    {
        fputs("camera-palette-v2\n", recipe);
        fclose(recipe);
    }
    printf(MAIN_TAG "Background dump complete: %s\n", request.outputDirectory.c_str());
    requests.pop_front();
    return !requests.empty();
}
