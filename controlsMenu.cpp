///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Controller and keyboard configuration menu
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "controlsMenu.h"
#include "configRemaster.h"
#include "fontTTF.h"
#include "input.h"
#include <cstring>

// Helper function to play menu navigation sounds (defined in systemMenu.cpp)
extern void playMenuSound(const char* soundName);

// Key bindings array
static KeyBinding g_keyBindings[ACTION_COUNT];

void initDefaultKeyBindings()
{
	// Up
	g_keyBindings[ACTION_UP].keyboard = SDL_SCANCODE_UP;
	g_keyBindings[ACTION_UP].gamepadButton = SDL_GAMEPAD_BUTTON_DPAD_UP;
	g_keyBindings[ACTION_UP].gamepadAxis = SDL_GAMEPAD_AXIS_LEFTY;
	g_keyBindings[ACTION_UP].gamepadAxisPositive = false;

	// Down
	g_keyBindings[ACTION_DOWN].keyboard = SDL_SCANCODE_DOWN;
	g_keyBindings[ACTION_DOWN].gamepadButton = SDL_GAMEPAD_BUTTON_DPAD_DOWN;
	g_keyBindings[ACTION_DOWN].gamepadAxis = SDL_GAMEPAD_AXIS_LEFTY;
	g_keyBindings[ACTION_DOWN].gamepadAxisPositive = true;

	// Left
	g_keyBindings[ACTION_LEFT].keyboard = SDL_SCANCODE_LEFT;
	g_keyBindings[ACTION_LEFT].gamepadButton = SDL_GAMEPAD_BUTTON_DPAD_LEFT;
	g_keyBindings[ACTION_LEFT].gamepadAxis = SDL_GAMEPAD_AXIS_LEFTX;
	g_keyBindings[ACTION_LEFT].gamepadAxisPositive = false;

	// Right
	g_keyBindings[ACTION_RIGHT].keyboard = SDL_SCANCODE_RIGHT;
	g_keyBindings[ACTION_RIGHT].gamepadButton = SDL_GAMEPAD_BUTTON_DPAD_RIGHT;
	g_keyBindings[ACTION_RIGHT].gamepadAxis = SDL_GAMEPAD_AXIS_LEFTX;
	g_keyBindings[ACTION_RIGHT].gamepadAxisPositive = true;

	// Action (Space / A)
	g_keyBindings[ACTION_ACTION].keyboard = SDL_SCANCODE_SPACE;
	g_keyBindings[ACTION_ACTION].gamepadButton = SDL_GAMEPAD_BUTTON_SOUTH;
	g_keyBindings[ACTION_ACTION].gamepadAxis = SDL_GAMEPAD_AXIS_COUNT;
	g_keyBindings[ACTION_ACTION].gamepadAxisPositive = false;

	// Confirm (Enter / Start)
	g_keyBindings[ACTION_CONFIRM].keyboard = SDL_SCANCODE_RETURN;
	g_keyBindings[ACTION_CONFIRM].gamepadButton = SDL_GAMEPAD_BUTTON_START;
	g_keyBindings[ACTION_CONFIRM].gamepadAxis = SDL_GAMEPAD_AXIS_COUNT;
	g_keyBindings[ACTION_CONFIRM].gamepadAxisPositive = false;

	// Cancel (Escape / B)
	g_keyBindings[ACTION_CANCEL].keyboard = SDL_SCANCODE_ESCAPE;
	g_keyBindings[ACTION_CANCEL].gamepadButton = SDL_GAMEPAD_BUTTON_EAST;
	g_keyBindings[ACTION_CANCEL].gamepadAxis = SDL_GAMEPAD_AXIS_COUNT;
	g_keyBindings[ACTION_CANCEL].gamepadAxisPositive = false;

	// Quick Turn Left (Q / Left Shoulder)
	g_keyBindings[ACTION_QUICK_TURN_LEFT].keyboard = SDL_SCANCODE_Q;
	g_keyBindings[ACTION_QUICK_TURN_LEFT].gamepadButton = SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;
	g_keyBindings[ACTION_QUICK_TURN_LEFT].gamepadAxis = SDL_GAMEPAD_AXIS_COUNT;
	g_keyBindings[ACTION_QUICK_TURN_LEFT].gamepadAxisPositive = false;

	// Quick Turn Right (E / Right Shoulder)
	g_keyBindings[ACTION_QUICK_TURN_RIGHT].keyboard = SDL_SCANCODE_E;
	g_keyBindings[ACTION_QUICK_TURN_RIGHT].gamepadButton = SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;
	g_keyBindings[ACTION_QUICK_TURN_RIGHT].gamepadAxis = SDL_GAMEPAD_AXIS_COUNT;
	g_keyBindings[ACTION_QUICK_TURN_RIGHT].gamepadAxisPositive = false;
}

const KeyBinding& getKeyBinding(KeyAction action)
{
	return g_keyBindings[action];
}

void setKeyboardBinding(KeyAction action, SDL_Scancode scancode)
{
	if (action >= 0 && action < ACTION_COUNT)
		g_keyBindings[action].keyboard = scancode;
}

void setGamepadBinding(KeyAction action, SDL_GamepadButton button)
{
	if (action >= 0 && action < ACTION_COUNT)
		g_keyBindings[action].gamepadButton = button;
}

const char* getActionName(KeyAction action)
{
	switch (action)
	{
	case ACTION_UP:      return "Up";
	case ACTION_DOWN:    return "Down";
	case ACTION_LEFT:    return "Left";
	case ACTION_RIGHT:   return "Right";
	case ACTION_ACTION:  return "Action";
	case ACTION_CONFIRM: return "Confirm";
	case ACTION_CANCEL:  return "Cancel";
	default:             return "Quick Turn";
	}
}

const char* getKeyboardKeyName(SDL_Scancode scancode)
{
	switch (scancode)
	{
	case SDL_SCANCODE_UP:      return "Up Arrow";
	case SDL_SCANCODE_DOWN:    return "Down Arrow";
	case SDL_SCANCODE_LEFT:    return "Left Arrow";
	case SDL_SCANCODE_RIGHT:   return "Right Arrow";
	case SDL_SCANCODE_SPACE:   return "Space";
	case SDL_SCANCODE_RETURN:  return "Enter";
	case SDL_SCANCODE_ESCAPE:  return "Escape";
	case SDL_SCANCODE_A:       return "A";
	case SDL_SCANCODE_B:       return "B";
	case SDL_SCANCODE_C:       return "C";
	case SDL_SCANCODE_D:       return "D";
	case SDL_SCANCODE_E:       return "E";
	case SDL_SCANCODE_F:       return "F";
	case SDL_SCANCODE_G:       return "G";
	case SDL_SCANCODE_H:       return "H";
	case SDL_SCANCODE_I:       return "I";
	case SDL_SCANCODE_J:       return "J";
	case SDL_SCANCODE_K:       return "K";
	case SDL_SCANCODE_L:       return "L";
	case SDL_SCANCODE_M:       return "M";
	case SDL_SCANCODE_N:       return "N";
	case SDL_SCANCODE_O:       return "O";
	case SDL_SCANCODE_P:       return "P";
	case SDL_SCANCODE_Q:       return "Q";
	case SDL_SCANCODE_R:       return "R";
	case SDL_SCANCODE_S:       return "S";
	case SDL_SCANCODE_T:       return "T";
	case SDL_SCANCODE_U:       return "U";
	case SDL_SCANCODE_V:       return "V";
	case SDL_SCANCODE_W:       return "W";
	case SDL_SCANCODE_X:       return "X";
	case SDL_SCANCODE_Y:       return "Y";
	case SDL_SCANCODE_Z:       return "Z";
	case SDL_SCANCODE_0:       return "0";
	case SDL_SCANCODE_1:       return "1";
	case SDL_SCANCODE_2:       return "2";
	case SDL_SCANCODE_3:       return "3";
	case SDL_SCANCODE_4:       return "4";
	case SDL_SCANCODE_5:       return "5";
	case SDL_SCANCODE_6:       return "6";
	case SDL_SCANCODE_7:       return "7";
	case SDL_SCANCODE_8:       return "8";
	case SDL_SCANCODE_9:       return "9";
	case SDL_SCANCODE_TAB:         return "Tab";
	case SDL_SCANCODE_LSHIFT:      return "L Shift";
	case SDL_SCANCODE_RSHIFT:      return "R Shift";
	case SDL_SCANCODE_LCTRL:       return "L Ctrl";
	case SDL_SCANCODE_RCTRL:       return "R Ctrl";
	case SDL_SCANCODE_LALT:        return "L Alt";
	case SDL_SCANCODE_RALT:        return "R Alt";
	default:                       return "???";
	}
}

const char* getGamepadButtonName(SDL_GamepadButton button)
{
	switch (button)
	{
	case SDL_GAMEPAD_BUTTON_SOUTH:          return "A / Cross";
	case SDL_GAMEPAD_BUTTON_EAST:           return "B / Circle";
	case SDL_GAMEPAD_BUTTON_WEST:           return "X / Square";
	case SDL_GAMEPAD_BUTTON_NORTH:          return "Y / Triangle";
	case SDL_GAMEPAD_BUTTON_START:          return "Start";
	case SDL_GAMEPAD_BUTTON_BACK:           return "Back";
	case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:  return "LB / L1";
	case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return "RB / R1";
	case SDL_GAMEPAD_BUTTON_LEFT_STICK:     return "L Stick";
	case SDL_GAMEPAD_BUTTON_RIGHT_STICK:    return "R Stick";
	case SDL_GAMEPAD_BUTTON_DPAD_UP:        return "DPad Up";
	case SDL_GAMEPAD_BUTTON_DPAD_DOWN:      return "DPad Down";
	case SDL_GAMEPAD_BUTTON_DPAD_LEFT:      return "DPad Left";
	case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:     return "DPad Right";
	default:                                return "???";
	}
}

const char* getGamepadAxisName(SDL_GamepadAxis axis, bool positive)
{
	switch (axis)
	{
	case SDL_GAMEPAD_AXIS_LEFTX:  return positive ? "L Stick Right" : "L Stick Left";
	case SDL_GAMEPAD_AXIS_LEFTY:  return positive ? "L Stick Down" : "L Stick Up";
	case SDL_GAMEPAD_AXIS_RIGHTX: return positive ? "R Stick Right" : "R Stick Left";
	case SDL_GAMEPAD_AXIS_RIGHTY: return positive ? "R Stick Down" : "R Stick Up";
	default:                      return "???";
	}
}

// ============================================================================
// Controls menu rendering helpers
// ============================================================================

#define CONTROLS_SELECT_COUL 0xF
#define CONTROLS_MENU_COUL   4
#define CONTROLS_LABEL_COUL  7
#define CONTROLS_VALUE_COUL  6

// Draw a custom text string centered at x,y with given color
static void drawText(int x, int y, const char* text, int color)
{
	SetFont(PtrFont, color);
	int width = ExtGetSizeFont((u8*)text);
	PrintFont(x - width / 2, y + 1, logicalScreen, (u8*)text);
}

// Draw a custom text string left-aligned at x,y with given color
static void drawTextLeft(int x, int y, const char* text, int color)
{
	SetFont(PtrFont, color);
	PrintFont(x, y + 1, logicalScreen, (u8*)text);
}

// Draw a single controls row: ACTION  KEYBOARD  [GAMEPAD]
static void drawControlRow(int y, KeyAction action, bool selected, bool remapping, bool remapGamepad)
{
	const KeyBinding& binding = getKeyBinding(action);
	int leftCol = WindowX1 + 4;
	int midCol = 140;
	int rightCol = 230;

	// Action name
	const char* actionName = getActionName(action);
	drawTextLeft(leftCol, y, actionName, selected ? CONTROLS_SELECT_COUL : CONTROLS_LABEL_COUL);

	// Keyboard binding
	if (selected && remapping && !remapGamepad)
	{
		drawTextLeft(midCol, y, "Press Key...", CONTROLS_SELECT_COUL);
	}
	else
	{
		drawTextLeft(midCol, y, getKeyboardKeyName(binding.keyboard), selected ? CONTROLS_SELECT_COUL : CONTROLS_VALUE_COUL);
	}

	// Gamepad binding
	if (g_controllerState.connected)
	{
		if (selected && remapping && remapGamepad)
		{
			drawTextLeft(rightCol, y, "Press...", CONTROLS_SELECT_COUL);
		}
		else
		{
			drawTextLeft(rightCol, y, getGamepadButtonName(binding.gamepadButton), selected ? CONTROLS_SELECT_COUL : CONTROLS_VALUE_COUL);
		}
	}
}

// ============================================================================
// Controls submenu
// ============================================================================
void processControlsMenu()
{
	int exitMenu = 0;
	int currentEntry = 0;
	bool remapping = false;
	bool remapGamepad = false;
	bool waitingForRelease = false; // true = waiting for all keys/buttons to be released before capturing

	// Total entries: ACTION_COUNT actions + 1 "BACK" + 1 "DEFAULTS"
	int totalEntries = ACTION_COUNT + 2;
	int backEntry = ACTION_COUNT;
	int defaultsEntry = ACTION_COUNT + 1;

	// Drain stale input from system menu (Enter key that selected Controls)
	AntiRebond = 1;

	// Clear stale system menu text from screen
	flushScreen();
	clearTTFTextQueue();

	while (!exitMenu)
	{
		// Clear TTF text queue each frame to prevent unbounded accumulation
		clearTTFTextQueue();

		// Draw frame - use full 320x200 to clear stale system menu text
		AffBigCadre(160, 100, 320, 200);

		int topY = WindowY1 + 4;

		// Title (translated)
		{
			const char* titleText = "Controls";
			if (languageNameString == "FRANCAIS")
				titleText = "Commandes";
			else if (languageNameString == "ITALIANO")
				titleText = "Comandi";
			else if (languageNameString == "ESPAGNOL")
				titleText = "Controles";
			else if (languageNameString == "DEUTSCH")
				titleText = "Steuerung";
			drawText(160, topY, titleText, CONTROLS_SELECT_COUL);
		}
		topY += 12;

		// Show controller name if connected (translated)
		if (g_controllerState.connected && g_controllerState.gamepad)
		{
			const char* name = SDL_GetGamepadName(g_controllerState.gamepad);
			if (name && name[0])
			{
				char nameBuf[32];
				strncpy(nameBuf, name, 31);
				nameBuf[31] = '\0';
				drawText(160, topY, nameBuf, CONTROLS_VALUE_COUL);
			}
			else
			{
				const char* connText = "Controller Connected";
				if (languageNameString == "FRANCAIS") connText = "Manette Connect" "\xE9" "e";
				else if (languageNameString == "ITALIANO") connText = "Controller Connesso";
				else if (languageNameString == "ESPAGNOL") connText = "Mando Conectado";
				else if (languageNameString == "DEUTSCH") connText = "Controller Verbunden";
				drawText(160, topY, connText, CONTROLS_VALUE_COUL);
			}
		}
		else
		{
			const char* noCtrl = "No Controller";
			if (languageNameString == "FRANCAIS") noCtrl = "Pas de Manette";
			else if (languageNameString == "ITALIANO") noCtrl = "Nessun Controller";
			else if (languageNameString == "ESPAGNOL") noCtrl = "Sin Mando";
			else if (languageNameString == "DEUTSCH") noCtrl = "Kein Controller";
			drawText(160, topY, noCtrl, CONTROLS_MENU_COUL);
		}
		topY += 11;

		// Column headers (translated)
		{
			const char* hdrAction = "Action";
			const char* hdrKey = "Key";
			const char* hdrButton = "Button";
			if (languageNameString == "FRANCAIS") { hdrAction = "Action"; hdrKey = "Touche"; hdrButton = "Bouton"; }
			else if (languageNameString == "ITALIANO") { hdrAction = "Azione"; hdrKey = "Tasto"; hdrButton = "Pulsante"; }
			else if (languageNameString == "ESPAGNOL") { hdrAction = "Acci" "\xF3" "n"; hdrKey = "Tecla"; hdrButton = "Bot" "\xF3" "n"; }
			else if (languageNameString == "DEUTSCH") { hdrAction = "Aktion"; hdrKey = "Taste"; hdrButton = "Knopf"; }
			drawTextLeft(WindowX1 + 4, topY, hdrAction, CONTROLS_LABEL_COUL);
			drawTextLeft(140, topY, hdrKey, CONTROLS_LABEL_COUL);
			if (g_controllerState.connected)
				drawTextLeft(230, topY, hdrButton, CONTROLS_LABEL_COUL);
		}
		topY += 10;

		// Draw each control binding
		for (int i = 0; i < ACTION_COUNT; i++)
		{
			int y = topY + i * 11;
			bool selected = (currentEntry == i);
			drawControlRow(y, (KeyAction)i, selected, remapping && selected, remapGamepad);
		}

		// Back option (translated)
		int backY = topY + ACTION_COUNT * 11 + 2;
		{
			bool selected = (currentEntry == backEntry);
			const char* backText = "Back";
			if (languageNameString == "FRANCAIS") backText = "Retour";
			else if (languageNameString == "ITALIANO") backText = "Indietro";
			else if (languageNameString == "ESPAGNOL") backText = "Volver";
			else if (languageNameString == "DEUTSCH") backText = "Zur" "\xFC" "ck";
			drawText(160, backY, backText, selected ? CONTROLS_SELECT_COUL : CONTROLS_MENU_COUL);
		}

		// Defaults option (translated)
		int defaultsY = backY + 11;
		{
			bool selected = (currentEntry == defaultsEntry);
			const char* defText = "Defaults";
			if (languageNameString == "FRANCAIS") defText = "Par D\xE9aut";
			else if (languageNameString == "ITALIANO") defText = "Predefiniti";
			else if (languageNameString == "ESPAGNOL") defText = "Predeterminado";
			else if (languageNameString == "DEUTSCH") defText = "Standard";
			drawText(160, defaultsY, defText, selected ? CONTROLS_SELECT_COUL : CONTROLS_MENU_COUL);
		}

		menuWaitVSync();
		osystem_CopyBlockPhys((unsigned char*)logicalScreen, 0, 0, 320, 200);
		osystem_startFrame();
		process_events();
		flushScreen();
		osystem_drawBackground();

		// Non-blocking remapping: check for key/button capture each frame
		if (remapping)
		{
			if (waitingForRelease)
			{
				// Wait for all keys/buttons to be released
				bool allReleased = true;

				if (remapGamepad)
				{
					if (g_controllerState.connected && g_controllerState.gamepad)
					{
						for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; i++)
						{
							if (SDL_GetGamepadButton(g_controllerState.gamepad, (SDL_GamepadButton)i))
							{
								allReleased = false;
								break;
							}
						}
					}
					// Also check keyboard (ESC to cancel)
					int numKeys = 0;
					const bool* keyboard = SDL_GetKeyboardState(&numKeys);
					for (int i = 0; i < numKeys; i++)
					{
						if (keyboard[i])
						{
							allReleased = false;
							break;
						}
					}
				}
				else
				{
					int numKeys = 0;
					const bool* keyboard = SDL_GetKeyboardState(&numKeys);
					for (int i = 0; i < numKeys; i++)
					{
						if (keyboard[i])
						{
							allReleased = false;
							break;
						}
					}
				}

				if (allReleased)
					waitingForRelease = false;
			}
			else
				{
					// Capture the next key/button press using keyboard/gamepad state
					// (SDL events are already consumed by process_events above)
					bool captured = false;

					if (!remapGamepad)
					{
						int numKeys = 0;
						const bool* keyboard = SDL_GetKeyboardState(&numKeys);
						for (int i = 0; i < numKeys; i++)
						{
							if (keyboard[i])
							{
								setKeyboardBinding((KeyAction)currentEntry, (SDL_Scancode)i);
								captured = true;
								break;
							}
						}
					}
					else
					{
						// Check ESC to cancel gamepad remap
						int numKeys = 0;
						const bool* keyboard = SDL_GetKeyboardState(&numKeys);
						if (numKeys > SDL_SCANCODE_ESCAPE && keyboard[SDL_SCANCODE_ESCAPE])
						{
							captured = true;
						}
						else if (g_controllerState.connected && g_controllerState.gamepad)
						{
							for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; i++)
							{
								if (SDL_GetGamepadButton(g_controllerState.gamepad, (SDL_GamepadButton)i))
								{
									setGamepadBinding((KeyAction)currentEntry, (SDL_GamepadButton)i);
									captured = true;
									break;
								}
							}
						}
					}

					if (captured)
					{
						remapping = false;
						remapGamepad = false;
						waitingForRelease = false;
						notifyTTFMenuSelectionChanged();
						AntiRebond = 1;
					}
				}

			osystem_flip(NULL);
			continue;
		}

		// Input handling
		char lKey = key;
		char lJoyD = JoyD;
		char lClick = Click;

		if (!AntiRebond)
		{
			if (lKey == 0x1B) // ESC - back
			{
				// Play back sound
				playMenuSound("Back.wav");
				exitMenu = 1;
			}
			else if (lKey == 0x1C || lClick) // Enter or Action
			{
				// Play select sound
				playMenuSound("Select.wav");

				if (currentEntry == backEntry)
				{
					exitMenu = 1;
				}
				else if (currentEntry == defaultsEntry)
				{
					initDefaultKeyBindings();
					notifyTTFMenuSelectionChanged();
					AntiRebond = 1;
				}
				else if (currentEntry < ACTION_COUNT)
				{
					// Start remapping keyboard (non-blocking)
					remapping = true;
					remapGamepad = false;
					waitingForRelease = true;
					AntiRebond = 1;
				}
			}
			else
			{
				if (lJoyD == 1) // up
				{
					currentEntry--;
					if (currentEntry < 0)
						currentEntry = totalEntries - 1;
					// Play navigation sound
					playMenuSound("Navigation.wav");
					notifyTTFMenuSelectionChanged();
					AntiRebond = 1;
				}
				if (lJoyD == 2) // down
				{
					currentEntry++;
					if (currentEntry >= totalEntries)
						currentEntry = 0;
					// Play navigation sound
					playMenuSound("Navigation.wav");
					notifyTTFMenuSelectionChanged();
					AntiRebond = 1;
				}
				// Right arrow on a binding entry: switch to gamepad remap
				if (lJoyD == 8 && currentEntry < ACTION_COUNT && g_controllerState.connected)
				{
					// Play navigation sound
					playMenuSound("Navigation.wav");
					remapping = true;
					remapGamepad = true;
					waitingForRelease = true;
					AntiRebond = 1;
				}
				// Left arrow on a binding entry: remap keyboard
				if (lJoyD == 4 && currentEntry < ACTION_COUNT)
				{
					// Play navigation sound
					playMenuSound("Navigation.wav");
					remapping = true;
					remapGamepad = false;
					waitingForRelease = true;
					AntiRebond = 1;
				}
			}
		}
		else
		{
			if (!lKey && !lJoyD && !lClick)
			{
				AntiRebond = 0;
			}
		}

		osystem_flip(NULL);
	}

	// Save bindings to config
	for (int i = 0; i < ACTION_COUNT; i++)
	{
		g_remasterConfig.controls.keyBindings[i] = (int)g_keyBindings[i].keyboard;
		g_remasterConfig.controls.gamepadBindings[i] = (int)g_keyBindings[i].gamepadButton;
	}
	saveRemasterConfig();

	// Clear controls menu text before returning to system menu
	flushScreen();
	clearTTFTextQueue();

	// Wait for input release
	while (key || JoyD || Click)
	{
		process_events();
	}
}
