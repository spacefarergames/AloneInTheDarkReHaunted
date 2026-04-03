///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// TrueType font rendering declarations
///////////////////////////////////////////////////////////////////////////////

#ifndef _FONT_TTF_H_
#define _FONT_TTF_H_

#include "common.h"
#include <string>

// TTF font rendering system - matches original font placement but renders with smooth TTF fonts

// Stores text rendering commands to be rendered later with ImGui
struct TTFTextCommand
{
    int x;
    int y;
    std::string text;
    int color;
    bool shadow;
    int shadowColor;
};

// Initialize TTF font system
void initTTFFont();

// Shutdown TTF font system
void shutdownTTFFont();

// Calculate the width of text using TTF font (matching original behavior)
int getTTFTextWidth(u8* string);

// Queue text for TTF rendering (matches original font positions)
void queueTTFText(int x, int y, u8* string, int color, bool shadow = false, int shadowColor = 0);

// Increment frame counter (call once per frame)
void incrementTTFFrameCounter();

// Notify when menu selection changes (for clearing stuck text)
void notifyTTFMenuSelectionChanged();

// Notify when menu/pause state changes (for clearing text on state transitions)
void notifyTTFMenuStateChanged(bool inMenu, bool isPaused);

// Render all queued TTF text (called during ImGui rendering)
void renderTTFText();

// Clear text queue
void clearTTFTextQueue();

#endif // _FONT_TTF_H_
