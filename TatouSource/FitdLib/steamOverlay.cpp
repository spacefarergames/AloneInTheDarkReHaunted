///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Steam overlay passthrough — dynamically loads steam_api64.dll at runtime
// so the build does not require the Steamworks SDK as a compile-time
// dependency.  When running outside of Steam (or when the DLL is absent)
// every public function is a harmless no-op.
///////////////////////////////////////////////////////////////////////////////

#include "steamOverlay.h"
#include "consoleLog.h"

#include <stdio.h>

#ifdef _WIN32
#include <windows.h>

// ---------------------------------------------------------------------------
// Function-pointer typedefs for the subset of the Steamworks API we need.
// Legacy (pre-1.55):  SteamAPI_Init()          -> bool
// Modern (1.55+):     SteamAPI_InitEx(char*)   -> int  (0 = k_ESteamAPIInitResult_OK)
// ---------------------------------------------------------------------------
typedef bool (__cdecl *pfn_SteamAPI_Init)();
typedef int  (__cdecl *pfn_SteamAPI_InitEx)(char *pOutErrMsg);
typedef void (__cdecl *pfn_SteamAPI_Shutdown)();
typedef void (__cdecl *pfn_SteamAPI_RunCallbacks)();
typedef bool (__cdecl *pfn_SteamAPI_IsSteamRunning)();
typedef bool (__cdecl *pfn_SteamAPI_RestartAppIfNecessary)(unsigned int unOwnAppID);

// ---------------------------------------------------------------------------
// Module-local state
// ---------------------------------------------------------------------------
static HMODULE              s_steamDLL              = nullptr;
static pfn_SteamAPI_Init    s_SteamAPI_Init         = nullptr;
static pfn_SteamAPI_InitEx  s_SteamAPI_InitEx       = nullptr;
static pfn_SteamAPI_Shutdown s_SteamAPI_Shutdown     = nullptr;
static pfn_SteamAPI_RunCallbacks s_SteamAPI_RunCallbacks = nullptr;
static pfn_SteamAPI_IsSteamRunning s_SteamAPI_IsSteamRunning = nullptr;
static pfn_SteamAPI_RestartAppIfNecessary s_SteamAPI_RestartAppIfNecessary = nullptr;
static bool                 s_steamActive           = false;
static bool                 s_steamNeedsRelaunch    = false;

// ---------------------------------------------------------------------------
// steamOverlay_Init
// ---------------------------------------------------------------------------
bool steamOverlay_Init()
{
    if (s_steamActive)
        return true;

    // Try to load steam_api64.dll from the application directory.
    // This is the standard Steamworks redistributable DLL that ships next to
    // the game executable when distributed through Steam.
    s_steamDLL = LoadLibraryA("steam_api64.dll");
    if (!s_steamDLL)
    {
        // 32-bit fallback (unlikely but harmless to try)
        s_steamDLL = LoadLibraryA("steam_api.dll");
    }

    if (!s_steamDLL)
    {
        printf(STEAM_TAG "steam_api64.dll not found — running without Steam overlay\n");
        return false;
    }

    printf(STEAM_OK "steam_api64.dll loaded successfully\n");

    // Resolve entry points — try legacy, InitEx, InitFlat, and internal names
    s_SteamAPI_Init         = (pfn_SteamAPI_Init)GetProcAddress(s_steamDLL, "SteamAPI_Init");
    s_SteamAPI_InitEx       = (pfn_SteamAPI_InitEx)GetProcAddress(s_steamDLL, "SteamAPI_InitEx");
    if (!s_SteamAPI_InitEx)
        s_SteamAPI_InitEx   = (pfn_SteamAPI_InitEx)GetProcAddress(s_steamDLL, "SteamAPI_InitFlat");
    if (!s_SteamAPI_InitEx)
        s_SteamAPI_InitEx   = (pfn_SteamAPI_InitEx)GetProcAddress(s_steamDLL, "SteamInternal_SteamAPI_Init");
    s_SteamAPI_Shutdown     = (pfn_SteamAPI_Shutdown)GetProcAddress(s_steamDLL, "SteamAPI_Shutdown");
    s_SteamAPI_RunCallbacks = (pfn_SteamAPI_RunCallbacks)GetProcAddress(s_steamDLL, "SteamAPI_RunCallbacks");
    s_SteamAPI_IsSteamRunning = (pfn_SteamAPI_IsSteamRunning)GetProcAddress(s_steamDLL, "SteamAPI_IsSteamRunning");
    s_SteamAPI_RestartAppIfNecessary = (pfn_SteamAPI_RestartAppIfNecessary)GetProcAddress(s_steamDLL, "SteamAPI_RestartAppIfNecessary");

    // Need at least one Init variant plus Shutdown and RunCallbacks
    if ((!s_SteamAPI_Init && !s_SteamAPI_InitEx) || !s_SteamAPI_Shutdown || !s_SteamAPI_RunCallbacks)
    {
        printf(STEAM_ERR "Failed to resolve required SteamAPI entry points:" CON_RESET "\n");
        if (!s_SteamAPI_Init && !s_SteamAPI_InitEx)
            printf(STEAM_ERR "  - SteamAPI_Init / SteamAPI_InitEx / SteamAPI_InitFlat: NOT FOUND" CON_RESET "\n");
        if (!s_SteamAPI_Shutdown)
            printf(STEAM_ERR "  - SteamAPI_Shutdown: NOT FOUND" CON_RESET "\n");
        if (!s_SteamAPI_RunCallbacks)
            printf(STEAM_ERR "  - SteamAPI_RunCallbacks: NOT FOUND" CON_RESET "\n");
        FreeLibrary(s_steamDLL);
        s_steamDLL = nullptr;
        return false;
    }

    if (s_SteamAPI_Init)
        printf(STEAM_OK "Resolved SteamAPI_Init (legacy)\n");
    if (s_SteamAPI_InitEx)
        printf(STEAM_OK "Resolved modern Init (InitEx/InitFlat)\n");

    // SteamAPI_RestartAppIfNecessary returns true when the game was NOT
    // launched through the Steam client and should be restarted via Steam.
    if (s_SteamAPI_RestartAppIfNecessary && s_SteamAPI_RestartAppIfNecessary(548090))
    {
        printf(STEAM_TAG "SteamAPI_RestartAppIfNecessary returned true — process should exit for Steam relaunch\n");
        FreeLibrary(s_steamDLL);
        s_steamDLL = nullptr;
        s_steamNeedsRelaunch = true;
        return false;
    }

    // Call SteamAPI_Init BEFORE the D3D11 device is created.
    // Steam's overlay installs API hooks during Init; if the device already
    // exists the overlay will miss its chance to intercept Present().
    bool initOk = false;
    if (s_SteamAPI_InitEx)
    {
        // Modern SDK (1.55+): SteamAPI_InitEx returns 0 on success
        char errMsg[1024] = {};
        int result = s_SteamAPI_InitEx(errMsg);
        if (result == 0)
        {
            initOk = true;
        }
        else
        {
            printf(STEAM_WARN "SteamAPI_InitEx failed (code %d): %s" CON_RESET "\n", result, errMsg[0] ? errMsg : "unknown error");
        }
    }
    else if (s_SteamAPI_Init)
    {
        // Legacy SDK: SteamAPI_Init returns true on success
        initOk = s_SteamAPI_Init();
        if (!initOk)
            printf(STEAM_WARN "SteamAPI_Init failed — Steam may not be running or steam_appid.txt is missing" CON_RESET "\n");
    }

    if (!initOk)
    {
        FreeLibrary(s_steamDLL);
        s_steamDLL = nullptr;
        return false;
    }

    s_steamActive = true;
    printf(STEAM_OK "SteamAPI initialized — overlay hooks are active\n");
    return true;
}

// ---------------------------------------------------------------------------
// steamOverlay_RunCallbacks
// ---------------------------------------------------------------------------
void steamOverlay_RunCallbacks()
{
    if (s_steamActive && s_SteamAPI_RunCallbacks)
    {
        s_SteamAPI_RunCallbacks();
    }
}

// ---------------------------------------------------------------------------
// steamOverlay_NotifyGraphicsReady
// ---------------------------------------------------------------------------
void steamOverlay_NotifyGraphicsReady()
{
    if (!s_steamActive)
        return;

    // The Steam overlay attaches to the D3D11 swap chain on the first
    // Present() call it observes after SteamAPI_Init().  By the time this
    // function is called bgfx has already created the device and submitted
    // at least one renderFrame(), so the overlay should be hooked.
    //
    // Pump one extra round of callbacks so the overlay processes the
    // "graphics ready" notification promptly.
    if (s_SteamAPI_RunCallbacks)
        s_SteamAPI_RunCallbacks();

    printf(STEAM_OK "Graphics device ready — Steam overlay should now be active (Shift+Tab)\n");
}

// ---------------------------------------------------------------------------
// steamOverlay_Shutdown
// ---------------------------------------------------------------------------
void steamOverlay_Shutdown()
{
    if (s_steamActive && s_SteamAPI_Shutdown)
    {
        s_SteamAPI_Shutdown();
        printf(STEAM_TAG "SteamAPI shut down\n");
    }

    if (s_steamDLL)
    {
        FreeLibrary(s_steamDLL);
        s_steamDLL = nullptr;
    }

    s_steamActive = false;
    s_SteamAPI_Init = nullptr;
    s_SteamAPI_InitEx = nullptr;
    s_SteamAPI_Shutdown = nullptr;
    s_SteamAPI_RunCallbacks = nullptr;
    s_SteamAPI_IsSteamRunning = nullptr;
    s_SteamAPI_RestartAppIfNecessary = nullptr;
}

// ---------------------------------------------------------------------------
// steamOverlay_IsActive
// ---------------------------------------------------------------------------
bool steamOverlay_IsActive()
{
    return s_steamActive;
}

// ---------------------------------------------------------------------------
// steamOverlay_ShouldRelaunch
// ---------------------------------------------------------------------------
bool steamOverlay_ShouldRelaunch()
{
    return s_steamNeedsRelaunch;
}

#else
// ---- Non-Windows stubs (Steam overlay is Windows-only for now) ----

bool steamOverlay_Init()                { return false; }
void steamOverlay_RunCallbacks()        {}
void steamOverlay_NotifyGraphicsReady() {}
void steamOverlay_Shutdown()            {}
bool steamOverlay_IsActive()            { return false; }
bool steamOverlay_ShouldRelaunch()      { return false; }

#endif // _WIN32
