///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// SDL-based operating system abstraction layer
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "memoryManager.h"
#include "exceptionHandler.h"
#include "consoleLog.h"
#include "updateChecker.h"
#include "embedded/embeddedData.h"
#include <bgfx/platform.h>

/***************************************************************************
mainSDL.cpp  -  description
-------------------
begin                : Mon Jun 3 2002
copyright            : (C) 2002 by Yaz0r
email                : yaz0r@yaz0r.net
***************************************************************************/

/***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************/

#include "bgfxGlue.h"
#include "jobSystemInit.h"
#include "reshadecompat.h"
#include "steamOverlay.h"
#include "gameDataCopy.h"
#include "configRemaster.h"
#include <SDL.h>
#include <SDL_thread.h>
#include <SDL_mutex.h>
#include "osystem.h"
#include "osystemAL.h"
#include <backends/imgui_impl_sdl3.h>

void detectGame(void);
void renderGameWindow();

int osystem_mouseRight;
int osystem_mouseLeft;

void osystem_delay(int time)
{
    SDL_Delay(time);
}

void osystem_updateImage()
{
}

/*void OSystem::getMouseStatus(mouseStatusStruct * mouseData)
{

SDL_GetMouseState(&mouseData->X, &mouseData->Y);

mouseData->left = mouseLeft;
mouseData->right = mouseRight;

mouseLeft = 0;
mouseRight = 0;
}*/

#ifdef WIN32
#define CALLBACK __stdcall
#else
#define CALLBACK
#endif

void OPL_musicPlayer(void *udata, Uint8 *stream, int len)
{
    musicUpdate(udata, stream, len);
}

extern "C" {
    void Sound_Quit(void);
}

void osystemAL_deinit();

void Sound_Quit(void)
{
    osystemAL_deinit();
}

extern "C" {
    char homePath[512] = "";
    int FitdMain(void* unkused);
}

// SEH wrapper around FitdMain to catch any unhandled exceptions on the game thread.
// This is a belt-and-suspenders approach on top of the VEH in ExceptionHandler.
// Must be a plain C-style function (no C++ destructors on stack) for __try/__except.
static int FitdMainProtected(void* unused)
{
    __try
    {
        return FitdMain(unused);
    }
    __except (ExceptionHandler::HandleException(GetExceptionInformation(), "FitdMainThread"))
    {
        printf(SDL_ERR "CRITICAL: Exception caught at FitdMainThread top level - game continuing" CON_RESET "\n");
        OutputDebugStringA("CRITICAL: FitdMainThread top-level exception caught and handled\n");
        return -1;
    }
}

SDL_Semaphore* startOfRender = NULL;
SDL_Semaphore* endOfRender = NULL;

//SDL_sem* emptyCount = NULL;
//SDL_sem* fullCount = NULL;

bool bFirst = true;

int FitdInit(int argc, char* argv[])
{
#ifdef WIN32
    //_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF | _CRTDBG_CHECK_ALWAYS_DF);

    // Initialize exception handler for crash logging and graceful recovery
    // Set to true to try continuing on heap corruption (logs error and continues)
    enableConsoleColors();
    ExceptionHandler::Initialize(true);

    printf(SDL_OK "Exception handler initialized - crash logs will be written to crash_log.txt\n");
    printf(SDL_TAG "Memory tracking active - memory debug log will be written to memory_debug.log\n");

    // Initialize ReShade compatibility checking and crash logging
    InitReShadeCompatibility();

    // Check for a newer release on GitHub (non-blocking background thread)
    CheckForUpdatesAsync();
#endif
    startOfRender = SDL_CreateSemaphore(0);
    endOfRender = SDL_CreateSemaphore(0);

    osystem_init();

    // Load remaster config early so steamless setting is available before data copy
    loadRemasterConfig();

    // Auto-detect and copy original AITD game data (PAK/ITD) from
    // Steam, GOG, or CD-ROM if not already present in the working directory.
    if (!g_remasterConfig.gameData.steamless)
    {
        gameDataCopy_EnsureDataFiles();
    }

    // Initialize job system for multithreaded asset loading
    initJobSystem();

    // Initialize Steam overlay BEFORE creating the SDL window and D3D11 device.
    // Steam's overlay hooks intercept D3D11 device creation, so SteamAPI_Init()
    // must be called first.  If the DLL is absent this is a harmless no-op.
    if (!g_remasterConfig.gameData.steamless)
    {
        steamOverlay_Init();
        if (steamOverlay_ShouldRelaunch())
        {
            // Steam needs to relaunch us through the Steam client — exit now.
            exit(0);
        }
    }

    unsigned int flags = 0;
    flags |= SDL_WINDOW_RESIZABLE;
    //flags |= SDL_WINDOW_ALLOW_HIGHDPI;

#ifdef __IPHONEOS__
    flags |= SDL_WINDOW_FULLSCREEN;
#endif

    // Game is running in dos resolution 13h, ie 320x200x256, but is displayed in 4:3, so pixel are not square (1.6:1)
    // We still need to create a 4:3 window for the actual display on screen.
    int resolution[2] = { 1410, 890 };

    gWindowBGFX = SDL_CreateWindow("FITD", resolution[0], resolution[1], flags);

#ifdef _WIN32
    // Now that the main window is visible, show the console window
    // positioned so it doesn't overlap the game window
    HWND hConsole = GetConsoleWindow();
    if (hConsole)
    {
        int wx, wy;
        SDL_GetWindowPosition(gWindowBGFX, &wx, &wy);

        // Place the console to the right of the main window
        int consoleX = wx + resolution[0] + 8;

        // If it would go off-screen, place it below the main window instead
        int screenW = GetSystemMetrics(SM_CXSCREEN);
        RECT consoleRect;
        GetWindowRect(hConsole, &consoleRect);
        int consoleW = consoleRect.right - consoleRect.left;
        int consoleH = consoleRect.bottom - consoleRect.top;

        if (consoleX + consoleW > screenW)
        {
            // Doesn't fit to the right — place below
            SetWindowPos(hConsole, NULL, wx, wy + resolution[1] + 8, 0, 0,
                SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
        else
        {
            SetWindowPos(hConsole, NULL, consoleX, wy, 0, 0,
                SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }

        ShowWindow(hConsole, SW_SHOWNOACTIVATE);
    }
#endif

    char version[256];

    getVersion(version);

    printf(SDL_TAG "%s", version);

    detectGame();

    // Call renderFrame before creating the game thread to signal bgfx
    // to not create its own internal render thread. This keeps all D3D11
    // calls (device creation, Present) on the main thread so Steam
    // Overlay can hook them properly.
    bgfx::renderFrame();

    SDL_CreateThread(FitdMainProtected, "FitdMainThread", NULL);

    // During init, bgfx::init() calls frame() internally which needs
    // multiple renderFrame() calls from this thread. Pump renderFrame
    // in a polling loop until the game thread signals its first
    // endOfRender (meaning init is done and the game loop has started).
    SDL_SignalSemaphore(startOfRender);
    do {
        bgfx::renderFrame(100);
    } while (!SDL_TryWaitSemaphore(endOfRender));

    // The bgfx device is now created and has presented at least one frame.
    // Notify Steam so the overlay can attach to the D3D11 swap chain.
    if (!g_remasterConfig.gameData.steamless)
    {
        steamOverlay_NotifyGraphicsReady();
    }

    unsigned long int t_start = SDL_GetTicks();
    unsigned long int t_lastUpdate = t_start;

    int FRAMES_PER_SECOND = 25;

    u32 startOfPreviousFrame = SDL_GetTicks();
    bool bFirstFrame = true;

    while (1)
    {
        u32 startOfFrame = SDL_GetTicks();

        assert(startOfPreviousFrame <= startOfFrame);

        u32 tickDifference = startOfFrame - startOfPreviousFrame;

        if (tickDifference < 1000 / FRAMES_PER_SECOND)
        {
           // SDL_Delay((1000 / FRAMES_PER_SECOND) - tickDifference);
        }

        startOfPreviousFrame = startOfFrame;

        unsigned long int t_sinceStart = SDL_GetTicks();

        int delta = 0;

        //if(t_sinceStart + 10 > t_lastUpdate)
        {
            delta = (t_sinceStart - t_lastUpdate);
            t_lastUpdate = t_sinceStart;
        }

        osystemAL_udpate();

        // Pump Steam overlay callbacks once per frame on the main thread.
        // This keeps the Shift+Tab overlay responsive and processes Steam events.
        if (!g_remasterConfig.gameData.steamless)
        {
            steamOverlay_RunCallbacks();
        }

        SDL_PumpEvents();

        SDL_GL_MakeCurrent(NULL, NULL);

        // Don't process events on first frame to avoid race condition with the init code
        if(!bFirstFrame)
        {
            readKeyboard();
        }
        else {
            bFirstFrame = false;
        }

        // Process deferred fullscreen toggle on the main/window thread
        if (g_pendingFullscreenToggle)
        {
            g_pendingFullscreenToggle = false;
            toggleFullscreen();
        }

        SDL_SignalSemaphore(startOfRender);

        // Normal operation: one renderFrame per game frame. Blocks
        // until the game thread submits via bgfx::frame(), then
        // WaitSemaphore picks up the endOfRender signal immediately.
        bgfx::renderFrame();
        SDL_WaitSemaphore(endOfRender);
    }

    return 0;
}

u32 lastFrameTime = 0;

#define FRAMES_PER_SECOND 25

u32 osystem_startOfFrame()
{
    SDL_WaitSemaphore(startOfRender);

    StartFrame();
    osystem_startFrame();

    static bool firstFrame = true;
    if (firstFrame)
    {
        //
#ifdef USE_IMGUI
        //ImGui_ImplSdlGL3_Init(sdl_window);
#endif
        lastFrameTime = SDL_GetTicks();

        firstFrame = false;
    }

    u32 numFramesToAdvance = (SDL_GetTicks() - lastFrameTime) / (1000 / FRAMES_PER_SECOND);

    if (numFramesToAdvance == 0)
        numFramesToAdvance = 1;

    lastFrameTime = SDL_GetTicks();

#ifdef USE_IMGUI
    //ImGui_ImplSdlGL3_NewFrame(sdl_window);
#endif

    return numFramesToAdvance;
}

void osystem_endOfFrame()
{
    osystem_flushPendingPrimitives();

    osystem_drawUILayer();

#ifdef FITD_DEBUGGER
    debugger_draw();
#endif

#ifdef USE_IMGUI
    //ImGui::Render();
#endif

   // osystem_flip(NULL);

    //renderGameWindow();

    EndFrame();

    if (bFirst)
        bFirst = false;

    SDL_SignalSemaphore(endOfRender);
    //SDL_SemPost(emptyCount);

}

bool fileExists(const char* name)
{
    // Check embedded data first
    if (getEmbeddedFile(name, nullptr, nullptr))
        return true;

    FILE* fHandle;

    fHandle = fopen(name, "rb");

    if (fHandle)
    {
        fclose(fHandle);
        return 1;
    }
    return 0;
}

void osystem_init()  // that's the constructor of the system dependent
// object used for the SDL port
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        fprintf(stderr, "Couldn't initialize SDL: %s\n", SDL_GetError());
        assert(0);
    }

    // SDL_ShowCursor (SDL_DISABLE);

    // SDL_EnableUNICODE (SDL_ENABLE); // not much used in fact

    SDL_PumpEvents();

    int screen_width = 800;
    int screen_height = 600;

    Uint32 windowFlags = SDL_WINDOW_OPENGL;

#ifdef RUN_FULLSCREEN
    windowFlags |= SDL_WINDOW_FULLSCREEN | SDL_WINDOW_ALLOW_HIGHDPI;
#endif

    osystem_mouseLeft = 0;
    osystem_mouseRight = 0;

    osystem_initGL(screen_width, screen_height);

    osystemAL_init();
}

int posInStream = 0;
volatile bool deviceStatus = false;



