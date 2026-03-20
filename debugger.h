///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Debug visualization tool declarations
///////////////////////////////////////////////////////////////////////////////

#ifndef _DEBUGGER_H_
#define _DEBUGGER_H_

#ifdef FITD_DEBUGGER

////// debug var used in engine
extern bool debuggerVar_debugMenuDisplayed;
extern bool debuggerVar_drawModelZv;
extern bool debuggerVar_drawCameraCoverZone;
extern bool debuggerVar_noHardClip;
extern bool debuggerVar_topCamera;
extern long int debufferVar_topCameraZoom;

extern bool debuggerVar_useBlackBG;
extern bool debuggerVar_fastForward;
///////////////////////////////

void debugger_draw(void);

#else
// Release build stubs - all debug features disabled
namespace {
    constexpr bool debuggerVar_debugMenuDisplayed = false;
    constexpr bool debuggerVar_drawModelZv = false;
    constexpr bool debuggerVar_drawCameraCoverZone = false;
    constexpr bool debuggerVar_noHardClip = false;
    constexpr bool debuggerVar_topCamera = false;
    constexpr long int debufferVar_topCameraZoom = -4000;
    constexpr bool debuggerVar_useBlackBG = false;
    constexpr bool debuggerVar_fastForward = false;
}

inline void debugger_draw(void) {}

#endif // FITD_DEBUGGER

#endif // _DEBUGGER_H_