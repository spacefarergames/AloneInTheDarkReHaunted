///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Keyboard, mouse, and gamepad input handling via SDL
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include <SDL.h>
#include <backends/imgui_impl_sdl3.h>
#include <cmath>
#include "configRemaster.h"
#include "controlsMenu.h"
#include "bgfxGlue.h"
#include "debugger.h"

extern float nearVal;
extern float farVal;
extern float cameraZoom;
extern float fov;

// Controller configuration - exposed in input.h
ControllerConfig g_controllerConfig = {
    0.15f,  // analogDeadzone
    1.0f,   // analogSensitivity
    false,  // invertYAxis
    true    // analogMovement
};

// Controller state - exposed in input.h
ControllerState g_controllerState = {
    nullptr, // gamepad
    false,   // connected
    0.0f,    // leftStickX
    0.0f,    // leftStickY
    0.0f,    // rightStickX
    0.0f     // rightStickY
};

void handleKeyDown(SDL_Event& event)
{
    switch (event.key.key)
    {
#ifdef FITD_DEBUGGER
    case SDL_SCANCODE_GRAVE:
        debuggerVar_debugMenuDisplayed ^= 1;
        break;
#endif
    case SDL_SCANCODE_RETURN:
        // ALT+ENTER toggles fullscreen
        if (event.key.mod & SDL_KMOD_ALT)
        {
            toggleFullscreen();
        }
        break;
    }
}

void readKeyboard(void)
{
    SDL_Event event;
    int size;
    int j;
    const bool *keyboard;

    JoyD = 0;
    Click = 0;
    key = 0;

    while (SDL_PollEvent(&event)) {

        ImGui_ImplSDL3_ProcessEvent(&event);

        switch (event.type) {
        case SDL_EVENT_KEY_DOWN:
            handleKeyDown(event);
            break;
        case SDL_EVENT_QUIT:
            cleanupAndExit();
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            // Double-click toggles fullscreen
            if (event.button.clicks == 2 && event.button.button == SDL_BUTTON_LEFT)
            {
                toggleFullscreen();
            }
            break;
        case SDL_EVENT_GAMEPAD_ADDED:
        case SDL_EVENT_GAMEPAD_REMOVED:
            handleControllerDeviceEvent(event);
            break;
        }

    }

#ifdef FITD_DEBUGGER
    debuggerVar_fastForward = false;
#endif

    // Update controller state
    updateController();

    keyboard = SDL_GetKeyboardState(&size);

    for (j = 0; j < size; j++)
    {
        if (keyboard[j])
        {
            switch (j)
            {
                /*        case SDLK_z:
                nearVal-=1;
                break;
                case SDLK_x:
                nearVal+=1;
                break;
                case SDLK_a:
                cameraZoom-=100;
                break;
                case SDLK_s:
                cameraZoom+=100;
                break;
                case SDLK_w:
                {
                float factor = (float)cameraY/(float)cameraZ;
                cameraY+=1;
                cameraZ=(float)cameraY/factor;
                break;
                }
                case SDLK_q:
                {
                float factor = (float)cameraY/(float)cameraZ;
                cameraY-=1;
                cameraZ=(float)cameraY/factor;
                break;
                }
                case SDLK_e:
                fov-=1;
                break;
                case SDLK_r:
                fov+=2;
                break; */
            case SDL_SCANCODE_RETURN:
                key = 0x1C;
                break;
            case SDL_SCANCODE_ESCAPE:
                key = 0x1B;
                break;
            default:
                // Check configurable key bindings
                if (j == (int)getKeyBinding(ACTION_UP).keyboard)
                    JoyD |= 1;
                if (j == (int)getKeyBinding(ACTION_DOWN).keyboard)
                    JoyD |= 2;
                if (j == (int)getKeyBinding(ACTION_LEFT).keyboard)
                    JoyD |= 4;
                if (j == (int)getKeyBinding(ACTION_RIGHT).keyboard)
                    JoyD |= 8;
                if (j == (int)getKeyBinding(ACTION_QUICK_TURN_LEFT).keyboard)
                    JoyD |= 0x10;
                if (j == (int)getKeyBinding(ACTION_QUICK_TURN_RIGHT).keyboard)
                    JoyD |= 0x20;
                if (j == (int)getKeyBinding(ACTION_ACTION).keyboard)
                    Click = 1;
                if (j == (int)getKeyBinding(ACTION_CONFIRM).keyboard && j != SDL_SCANCODE_RETURN)
                    key = 0x1C;
                if (j == (int)getKeyBinding(ACTION_CANCEL).keyboard && j != SDL_SCANCODE_ESCAPE)
                    key = 0x1B;
                break;
#ifdef FITD_DEBUGGER
            case SDL_SCANCODE_O:
                debufferVar_topCameraZoom += 100;
                break;
            case SDL_SCANCODE_P:
                debufferVar_topCameraZoom -= 100;
                break;
            case SDL_SCANCODE_T:
                debuggerVar_topCamera = true;
                backgroundMode = backgroundModeEnum_3D;
                break;
            case SDL_SCANCODE_Y:
                debuggerVar_topCamera = false;
                backgroundMode = backgroundModeEnum_2D;
                FlagInitView = 1;
                break;
            case SDL_SCANCODE_C:
                debuggerVar_noHardClip = !debuggerVar_noHardClip;
                break;
            case SDL_SCANCODE_B:
                backgroundMode = backgroundModeEnum_3D;
                break;
            case SDL_SCANCODE_N:
                backgroundMode = backgroundModeEnum_2D;
                break;
            case SDL_SCANCODE_F:
                debuggerVar_fastForward = true;
                break;


#endif
            }
        }
    }
#ifdef FITD_DEBUGGER
    if (debuggerVar_topCamera)
    {
        backgroundMode = backgroundModeEnum_3D;
    }
#endif
}

// ============================================================================
// Controller Support Functions
// ============================================================================

void initController()
{
    // Load remaster configuration
    loadRemasterConfig();

    // Initialize default key bindings, then apply saved config
    initDefaultKeyBindings();
    for (int i = 0; i < ACTION_COUNT; i++)
    {
        if (g_remasterConfig.controls.keyBindings[i] != 0)
            setKeyboardBinding((KeyAction)i, (SDL_Scancode)g_remasterConfig.controls.keyBindings[i]);
        if (g_remasterConfig.controls.gamepadBindings[i] != 0)
            setGamepadBinding((KeyAction)i, (SDL_GamepadButton)g_remasterConfig.controls.gamepadBindings[i]);
    }

    // Sync controller config from loaded settings
    g_controllerConfig.analogDeadzone = g_remasterConfig.controller.analogDeadzone;
    g_controllerConfig.analogSensitivity = g_remasterConfig.controller.analogSensitivity;
    g_controllerConfig.invertYAxis = g_remasterConfig.controller.invertYAxis;
    g_controllerConfig.analogMovement = g_remasterConfig.controller.analogMovement;

    if (!g_remasterConfig.controller.enableController)
    {
        printf("Controller support disabled in config\n");
        return;
    }

    // Initialize SDL gamepad subsystem
    if (SDL_InitSubSystem(SDL_INIT_GAMEPAD) < 0)
    {
        printf("Failed to initialize SDL gamepad subsystem: %s\n", SDL_GetError());
        return;
    }

    // Open the first available gamepad
    int numGamepads = 0;
    SDL_JoystickID* gamepads = SDL_GetGamepads(&numGamepads);

    if (gamepads && numGamepads > 0)
    {
        g_controllerState.gamepad = SDL_OpenGamepad(gamepads[0]);
        if (g_controllerState.gamepad)
        {
            g_controllerState.connected = true;
            printf("Controller connected: %s\n", SDL_GetGamepadName(g_controllerState.gamepad));
        }
    }

    if (gamepads)
    {
        SDL_free(gamepads);
    }
}

void shutdownController()
{
    if (g_controllerState.gamepad)
    {
        SDL_CloseGamepad(g_controllerState.gamepad);
        g_controllerState.gamepad = nullptr;
        g_controllerState.connected = false;
    }
}

void handleControllerDeviceEvent(SDL_Event& event)
{
    switch (event.type)
    {
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            // Double-click toggles fullscreen
            if (event.button.clicks == 2 && event.button.button == SDL_BUTTON_LEFT)
            {
                toggleFullscreen();
            }
            break;
    case SDL_EVENT_GAMEPAD_ADDED:
        if (!g_controllerState.connected)
        {
            g_controllerState.gamepad = SDL_OpenGamepad(event.gdevice.which);
            if (g_controllerState.gamepad)
            {
                g_controllerState.connected = true;
                printf("Controller connected: %s\n", SDL_GetGamepadName(g_controllerState.gamepad));
            }
        }
        break;

    case SDL_EVENT_GAMEPAD_REMOVED:
        if (g_controllerState.gamepad)
        {
            SDL_CloseGamepad(g_controllerState.gamepad);
            g_controllerState.gamepad = nullptr;
            g_controllerState.connected = false;
            printf("Controller disconnected\n");
        }
        break;
    }
}

float applyDeadzone(float value, float deadzone)
{
    if (fabs(value) < deadzone)
        return 0.0f;

    // Rescale the value after deadzone
    float sign = (value > 0.0f) ? 1.0f : -1.0f;
    return sign * ((fabs(value) - deadzone) / (1.0f - deadzone));
}

void updateController()
{
    if (!g_controllerState.connected || !g_controllerState.gamepad)
        return;

    // Read analog sticks (SDL3 returns -32768 to 32767)
    float leftX = SDL_GetGamepadAxis(g_controllerState.gamepad, SDL_GAMEPAD_AXIS_LEFTX) / 32768.0f;
    float leftY = SDL_GetGamepadAxis(g_controllerState.gamepad, SDL_GAMEPAD_AXIS_LEFTY) / 32768.0f;
    float rightX = SDL_GetGamepadAxis(g_controllerState.gamepad, SDL_GAMEPAD_AXIS_RIGHTX) / 32768.0f;
    float rightY = SDL_GetGamepadAxis(g_controllerState.gamepad, SDL_GAMEPAD_AXIS_RIGHTY) / 32768.0f;

    // Apply deadzone
    leftX = applyDeadzone(leftX, g_controllerConfig.analogDeadzone);
    leftY = applyDeadzone(leftY, g_controllerConfig.analogDeadzone);
    rightX = applyDeadzone(rightX, g_controllerConfig.analogDeadzone);
    rightY = applyDeadzone(rightY, g_controllerConfig.analogDeadzone);

    // Apply sensitivity
    leftX *= g_controllerConfig.analogSensitivity;
    leftY *= g_controllerConfig.analogSensitivity;
    rightX *= g_controllerConfig.analogSensitivity;
    rightY *= g_controllerConfig.analogSensitivity;

    // Invert Y axis if configured
    if (g_controllerConfig.invertYAxis)
    {
        leftY = -leftY;
        rightY = -rightY;
    }

    // Store in state
    g_controllerState.leftStickX = leftX;
    g_controllerState.leftStickY = leftY;
    g_controllerState.rightStickX = rightX;
    g_controllerState.rightStickY = rightY;

    // Map analog stick to directional input (JoyD)
    // JoyD bit flags: 1=up, 2=down, 4=left, 8=right
    if (g_controllerConfig.analogMovement)
    {
        const float threshold = 0.3f; // Threshold for digital direction

        if (leftY < -threshold)
            JoyD |= 1; // Up
        if (leftY > threshold)
            JoyD |= 2; // Down
        if (leftX < -threshold)
            JoyD |= 4; // Left
        if (leftX > threshold)
            JoyD |= 8; // Right
    }

    // D-Pad support (always active)
    if (SDL_GetGamepadButton(g_controllerState.gamepad, SDL_GAMEPAD_BUTTON_DPAD_UP))
        JoyD |= 1;
    if (SDL_GetGamepadButton(g_controllerState.gamepad, SDL_GAMEPAD_BUTTON_DPAD_DOWN))
        JoyD |= 2;
    if (SDL_GetGamepadButton(g_controllerState.gamepad, SDL_GAMEPAD_BUTTON_DPAD_LEFT))
        JoyD |= 4;
    if (SDL_GetGamepadButton(g_controllerState.gamepad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT))
        JoyD |= 8;

    // Configurable button mapping
    if (SDL_GetGamepadButton(g_controllerState.gamepad, getKeyBinding(ACTION_ACTION).gamepadButton))
        Click = 1;

    if (SDL_GetGamepadButton(g_controllerState.gamepad, getKeyBinding(ACTION_CANCEL).gamepadButton))
        key = 0x1B; // ESC

    if (SDL_GetGamepadButton(g_controllerState.gamepad, getKeyBinding(ACTION_CONFIRM).gamepadButton))
        key = 0x1C; // ENTER

#ifdef FITD_DEBUGGER
    // Back/Select button = Toggle debug menu (Debug builds only)
    if (SDL_GetGamepadButton(g_controllerState.gamepad, SDL_GAMEPAD_BUTTON_BACK))
        debuggerVar_debugMenuDisplayed ^= 1;
#endif

    // Quick turn shoulder buttons
    if (SDL_GetGamepadButton(g_controllerState.gamepad, getKeyBinding(ACTION_QUICK_TURN_LEFT).gamepadButton))
        JoyD |= 0x10;
    if (SDL_GetGamepadButton(g_controllerState.gamepad, getKeyBinding(ACTION_QUICK_TURN_RIGHT).gamepadButton))
        JoyD |= 0x20;
}
