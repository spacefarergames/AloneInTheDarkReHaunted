///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Remaster configuration structures and declarations
///////////////////////////////////////////////////////////////////////////////

#ifndef _CONFIG_REMASTER_H_
#define _CONFIG_REMASTER_H_

// Remaster configuration management
struct RemasterConfig
{
    // Controller settings
    struct {
        float analogDeadzone;
        float analogSensitivity;
        bool invertYAxis;
        bool analogMovement;
        bool enableController;
    } controller;

    // HD Graphics settings (for future implementation)
    struct {
        bool enableHDBackgrounds;
        int backgroundScale;
        bool enableFiltering;
        bool enableBlurredMenu;
        float menuBlurAmount;
    } graphics;

    // Post-processing settings
    struct {
        bool enableBloom;
        float bloomThreshold;
        float bloomIntensity;
        bool enableFilmGrain;
        float filmGrainIntensity;
        bool enableSSAO;
        float ssaoRadius;
        float ssaoIntensity;
        int bloomPasses;
    } postProcessing;

    // External music settings (for future implementation)
    struct {
        bool enableExternalMusic;
        char musicFolder[256];
    } music;

    // TTF font settings
    struct {
        bool enableTTF;
        char fontPath[256];
        int fontSize;
        bool hideOriginalText;
    } font;

    // Controls / key binding settings
    struct {
        int keyBindings[9];     // SDL_Scancode per action (ACTION_COUNT = 9)
        int gamepadBindings[9]; // SDL_GamepadButton per action
    } controls;
};

extern RemasterConfig g_remasterConfig;

// Configuration functions
void loadRemasterConfig();
void saveRemasterConfig();
void initDefaultRemasterConfig();

#endif // _CONFIG_REMASTER_H_
