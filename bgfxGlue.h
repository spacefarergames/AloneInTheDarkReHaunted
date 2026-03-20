///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Author: Jake Jackson (jake@spacefarergames.com)
//
// BGFX initialization and window management declarations
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <SDL.h>

int initBgfxGlue(int argc, char* argv[]);
void deleteBgfxGlue();

void StartFrame();
void EndFrame();

extern int gFrameLimit;
extern bool gCloseApp;

extern SDL_Window* gWindowBGFX;

extern bool gIsFullscreen;
void toggleFullscreen();

