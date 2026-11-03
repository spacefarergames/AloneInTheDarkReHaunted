#pragma once
#include <bgfx/bgfx.h>
#include <SDL.h>
bgfx::CallbackI* unityBgfxCallback();
void unityBgfxReady();
void unityFrameInput();
void unitySettingsUpdate();
void unityCheckStop();
#include <cstdint>
uint32_t unityRenderSize();
bool unityManagedStartup();
int unityRenderWidth();
int unityRenderHeight();
unsigned int unityAudioSampleRate();
const bool* unityKeyboardState(int* count);
bool unityGamepadButton(SDL_Gamepad*,SDL_GamepadButton button);
Sint16 unityGamepadAxis(SDL_Gamepad*,SDL_GamepadAxis axis);
