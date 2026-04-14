///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Steam overlay passthrough — dynamically loads steam_api64.dll so the
// Steam Overlay can hook D3D11 Present calls on the main thread.
// The game works without Steam; all calls are no-ops when the DLL is absent.
///////////////////////////////////////////////////////////////////////////////

#ifndef _STEAM_OVERLAY_H_
#define _STEAM_OVERLAY_H_

// Initialize Steam overlay support.
// Dynamically loads steam_api64.dll and calls SteamAPI_Init().
// Must be called BEFORE the SDL window and bgfx device are created so that
// Steam's overlay hooks can intercept D3D11 device creation.
// Returns true if Steam was initialized successfully.
bool steamOverlay_Init();

// Pump Steam callbacks — call once per frame from the main (render) thread.
// This keeps the overlay responsive and processes Steam events.
void steamOverlay_RunCallbacks();

// Notify Steam that the game's graphics window is now active.
// Call after bgfx::init() has created the D3D11 device and the first
// frame has been presented, so the overlay attaches to the correct swap chain.
void steamOverlay_NotifyGraphicsReady();

// Shutdown Steam API and unload the DLL.
void steamOverlay_Shutdown();

// Returns true if Steam API was loaded and initialized successfully.
bool steamOverlay_IsActive();

// Returns true if Steam needs to relaunch the game via the Steam client.
// When true, the process should exit immediately so Steam can restart it.
bool steamOverlay_ShouldRelaunch();

#endif // _STEAM_OVERLAY_H_
