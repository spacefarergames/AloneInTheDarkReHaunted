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
                              int floorNumber, int cameraIdx,
                              const unsigned char* paletted,
                              const palette_t& palette)
{
    char filePath[1024];
    snprintf(filePath, sizeof(filePath),
             "%s/%s_F%02d_C%02d.png",
             outDir, baseName, floorNumber, cameraIdx);

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

void dumpAllBackgrounds()
{
    char dirPath[512];
    snprintf(dirPath, sizeof(dirPath), "%sbackgrounds_dump", homePath);
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
                convertPaletteIfRequired(bgPalette);
            }
            else
            {
                bgPalette = fallbackPalette;
            }

            if (dumpOneBackground(dirPath, pakBase, floor, (int)idx,
                                  paletted, bgPalette))
            {
                totalDumped++;
            }

            free(buf);
        }
    }

    // AITD1 also stores some special / overridden cameras inside ITD_RESS
    // (e.g. AITD1_CAM06000, AITD1_CAM07000). Best-effort: scan a small range
    // of indices after 64000-byte payloads.
    if (g_gameId == AITD1)
    {
        unsigned int numFiles = PAK_getNumFiles("ITD_RESS");
        for (unsigned int idx = 0; idx < numFiles; idx++)
        {
            int size = getPakSize("ITD_RESS", idx);
            if (size != 64000)
                continue;

            char* buf = loadPak("ITD_RESS", idx);
            if (!buf)
                continue;

            char filePath[1024];
            snprintf(filePath, sizeof(filePath),
                     "%s/ITD_RESS_%04u.png", dirPath, idx);

            unsigned char* rgba = (unsigned char*)malloc(320 * 200 * 4);
            if (rgba)
            {
                const unsigned char* paletted = (const unsigned char*)buf;
                for (int i = 0; i < 320 * 200; i++)
                {
                    unsigned char p = paletted[i];
                    rgba[i * 4 + 0] = fallbackPalette[p][0];
                    rgba[i * 4 + 1] = fallbackPalette[p][1];
                    rgba[i * 4 + 2] = fallbackPalette[p][2];
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
}
