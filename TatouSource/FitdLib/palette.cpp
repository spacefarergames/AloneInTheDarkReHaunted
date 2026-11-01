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

namespace
{
const unsigned char kDefaultCameraPalette[0x30] =
{
    0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x0C, 0x0C, 0x0E,
    0x30, 0x2F, 0x3F, 0x23, 0x2C, 0x23, 0x2A, 0x1D, 0x2A,
    0x2A, 0x21, 0x18, 0x3F, 0x05, 0x2A, 0x12, 0x14, 0x18,
    0x31, 0x15, 0x17, 0x15, 0x25, 0x15, 0x15, 0x2F, 0x3F,
    0x3F, 0x22, 0x15, 0x2B, 0x15, 0x3F, 0x3F, 0x3F, 0x21,
    0x3F, 0x3F, 0x3F
};

// AITD3 stores these fixed colors as VGA DAC values already multiplied by
// four. convertPaletteIfRequired deliberately preserves those byte values.
const unsigned char kDefaultCameraPaletteAITD3[0x30] =
{
    0x00, 0x00, 0x00, 0xFC, 0xFC, 0xFC, 0x30, 0x30, 0x38,
    0xC0, 0xBC, 0xFC, 0x78, 0x58, 0x3C, 0x00, 0x00, 0x00,
    0xF0, 0x70, 0x10, 0xFC, 0xFC, 0xFC, 0x48, 0x50, 0x60,
    0xC4, 0x54, 0x5C, 0x54, 0x94, 0x54, 0x54, 0xBC, 0xFC,
    0xFC, 0x88, 0x54, 0xAC, 0x54, 0xFC, 0xFC, 0xFC, 0xFC,
    0xFC, 0xFC, 0xF8
};
}

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
    // palette_t is std::array<std::array<unsigned char, 3>, 256> -> 768 contiguous bytes.
    memcpy(dest.data(), source, 256 * 3);
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

void prepareCameraPalette(palette_t& palette)
{
    if (g_gameId >= JACK && g_gameId <= AITD3)
    {
        const unsigned char* fixed = g_gameId == AITD3
            ? kDefaultCameraPaletteAITD3 : kDefaultCameraPalette;
        for (int i = 0; i < 16; ++i)
        {
            palette[i][0] = fixed[i * 3 + 0];
            palette[i][1] = fixed[i * 3 + 1];
            palette[i][2] = fixed[i * 3 + 2];
        }
    }
    convertPaletteIfRequired(palette);
}
