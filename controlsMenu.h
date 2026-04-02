///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Controls configuration menu declarations
///////////////////////////////////////////////////////////////////////////////

#ifndef _CONTROLS_MENU_H_
#define _CONTROLS_MENU_H_

#include <SDL.h>

// Key binding actions
enum KeyAction
{
	ACTION_UP = 0,
	ACTION_DOWN,
	ACTION_LEFT,
	ACTION_RIGHT,
	ACTION_ACTION,    // Space / A button
	ACTION_CONFIRM,   // Enter / Start button
	ACTION_CANCEL,    // Escape / B button
	ACTION_QUICK_TURN_LEFT,  // Q / Left Shoulder
	ACTION_QUICK_TURN_RIGHT, // E / Right Shoulder
	ACTION_COUNT
};

// Key binding entry
struct KeyBinding
{
	SDL_Scancode keyboard;           // Keyboard scancode
	SDL_GamepadButton gamepadButton; // Gamepad button (or SDL_GAMEPAD_BUTTON_COUNT if axis)
	SDL_GamepadAxis gamepadAxis;     // Gamepad axis (or SDL_GAMEPAD_AXIS_COUNT if button)
	bool gamepadAxisPositive;        // true = positive direction, false = negative
};

// Default key bindings
void initDefaultKeyBindings();

// Get current binding for an action
const KeyBinding& getKeyBinding(KeyAction action);

// Set keyboard binding
void setKeyboardBinding(KeyAction action, SDL_Scancode scancode);

// Set gamepad button binding
void setGamepadBinding(KeyAction action, SDL_GamepadButton button);

// Get human-readable name for a key binding
const char* getKeyboardKeyName(SDL_Scancode scancode);
const char* getGamepadButtonName(SDL_GamepadButton button);
const char* getGamepadAxisName(SDL_GamepadAxis axis, bool positive);
const char* getActionName(KeyAction action);

// Process the controls submenu
void processControlsMenu(bool hdMode = false);

#endif
