///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Input handling declarations
///////////////////////////////////////////////////////////////////////////////

#ifndef _INPUT_H_
#define _INPUT_H_

#include <SDL.h>

// Controller configuration
struct ControllerConfig
{
    float analogDeadzone;
    float analogSensitivity;
    bool invertYAxis;
    bool analogMovement;
};

extern ControllerConfig g_controllerConfig;

// Controller state
struct ControllerState
{
    SDL_Gamepad* gamepad;
    bool connected;
    float leftStickX;
    float leftStickY;
    float rightStickX;
    float rightStickY;
};

extern ControllerState g_controllerState;

// Window resize flag - set when window is resized/maximized/fullscreened
extern bool g_windowWasResized;

extern "C" {
void readKeyboard(void);
}

// Reset the window resize flag (call after handling resize)
void resetWindowResizeFlag();

// Controller functions
void initController();
void shutdownController();
void updateController();
void handleControllerDeviceEvent(SDL_Event& event);

#endif
