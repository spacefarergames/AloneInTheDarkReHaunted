///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Color palette management, fading, and conversion
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "palette.h"

float g_fadeLevel = 1.0f; // 0.0 = black, 1.0 = full brightness

void paletteFill(palette_t& palette, unsigned char r, unsigned char g, unsigned b)
{
    r <<= 1;
    g <<= 1;
    b <<= 1;

    for (int i = 0; i < 256; i++)
    {
        palette[i][0] = r;
        palette[i][1] = g;
        palette[i][2] = b;
    }
}

void setPalette(palette_t& sourcePal)
{
    osystem_setPalette(&sourcePal);
}

void copyPalette(palette_t& source, palette_t& dest)
{
    for (int i = 0; i < 256; i++)
    {
        dest[i] = source[i];
    }
}

void copyPalette(void* source, palette_t& dest)
{
    for (int i = 0; i < 256; i++)
    {
        for (int j = 0; j < 3; j++) {
            dest[i][j] = ((uint8*)source)[i * 3 + j];
        }
    }
}

void convertPaletteIfRequired(palette_t& lpalette)
{
    if (g_gameId >= JACK && g_gameId <= AITD3)
    {
        for (int i = 0; i < 256; i++)
        {
            for (int j = 0; j < 3; j++)
            {
                lpalette[i][j] = (((unsigned int)lpalette[i][j] * 255) / 63) & 0xFF;
            }
        }
    }
}

void SetLevelDestPal(palette_t& inPalette, palette_t& outPalette, int coef)
{
    // Update global fade level for HD backgrounds (coef 0-256 -> 0.0-1.0)
    g_fadeLevel = coef / 256.0f;

    for (int i = 0; i < 256; i++)
    {
        for (int j = 0; j < 3; j++) {
            outPalette[i][j] = (inPalette[i][j] * coef) >> 8;
        }
    }
}