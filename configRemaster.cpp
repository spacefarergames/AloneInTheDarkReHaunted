///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Remaster configuration loading and management
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "configRemaster.h"
#include "consoleLog.h"
#include "controlsMenu.h"
#include <stdio.h>
#include <string.h>

RemasterConfig g_remasterConfig;

void initDefaultRemasterConfig()
{
    // Controller defaults
    g_remasterConfig.controller.analogDeadzone = 0.15f;
    g_remasterConfig.controller.analogSensitivity = 1.0f;
    g_remasterConfig.controller.invertYAxis = false;
    g_remasterConfig.controller.analogMovement = true;
    g_remasterConfig.controller.enableController = true;

    // HD Graphics defaults (for future)
    g_remasterConfig.graphics.enableHDBackgrounds = false;
    g_remasterConfig.graphics.backgroundScale = 2;
    g_remasterConfig.graphics.enableFiltering = true;
    g_remasterConfig.graphics.enableBlurredMenu = false;
    g_remasterConfig.graphics.menuBlurAmount = 5.0f;

    // Post-processing defaults
    g_remasterConfig.postProcessing.enableBloom = true;
    g_remasterConfig.postProcessing.bloomThreshold = 0.45f;
    g_remasterConfig.postProcessing.bloomIntensity = 0.55f;
    g_remasterConfig.postProcessing.enableFilmGrain = true;
    g_remasterConfig.postProcessing.filmGrainIntensity = 0.025f;
    g_remasterConfig.postProcessing.enableSSAO = true;
    g_remasterConfig.postProcessing.ssaoRadius = 400.0f;
    g_remasterConfig.postProcessing.ssaoIntensity = 0.8f;
    g_remasterConfig.postProcessing.bloomPasses = 2;

    // External music defaults (for future)
    g_remasterConfig.music.enableExternalMusic = false;
    strcpy(g_remasterConfig.music.musicFolder, "music");

    // TTF font defaults
    g_remasterConfig.font.enableTTF = false;
    strcpy(g_remasterConfig.font.fontPath, "BLKCHCRY.TTF");
    g_remasterConfig.font.fontSize = 16;
    g_remasterConfig.font.hideOriginalText = true;

    // Controls defaults (matching initDefaultKeyBindings)
    g_remasterConfig.controls.keyBindings[0] = SDL_SCANCODE_UP;
    g_remasterConfig.controls.keyBindings[1] = SDL_SCANCODE_DOWN;
    g_remasterConfig.controls.keyBindings[2] = SDL_SCANCODE_LEFT;
    g_remasterConfig.controls.keyBindings[3] = SDL_SCANCODE_RIGHT;
    g_remasterConfig.controls.keyBindings[4] = SDL_SCANCODE_SPACE;
    g_remasterConfig.controls.keyBindings[5] = SDL_SCANCODE_RETURN;
    g_remasterConfig.controls.keyBindings[6] = SDL_SCANCODE_ESCAPE;
    g_remasterConfig.controls.gamepadBindings[0] = SDL_GAMEPAD_BUTTON_DPAD_UP;
    g_remasterConfig.controls.gamepadBindings[1] = SDL_GAMEPAD_BUTTON_DPAD_DOWN;
    g_remasterConfig.controls.gamepadBindings[2] = SDL_GAMEPAD_BUTTON_DPAD_LEFT;
    g_remasterConfig.controls.gamepadBindings[3] = SDL_GAMEPAD_BUTTON_DPAD_RIGHT;
    g_remasterConfig.controls.gamepadBindings[4] = SDL_GAMEPAD_BUTTON_SOUTH;
    g_remasterConfig.controls.gamepadBindings[5] = SDL_GAMEPAD_BUTTON_START;
    g_remasterConfig.controls.gamepadBindings[6] = SDL_GAMEPAD_BUTTON_EAST;
    g_remasterConfig.controls.keyBindings[7] = SDL_SCANCODE_Q;
    g_remasterConfig.controls.keyBindings[8] = SDL_SCANCODE_E;
    g_remasterConfig.controls.gamepadBindings[7] = SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;
    g_remasterConfig.controls.gamepadBindings[8] = SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;
}

void loadRemasterConfig()
{
    FILE* file = fopen("aitd_remaster.cfg", "r");
    
    // Initialize defaults first
    initDefaultRemasterConfig();
    
    if (!file)
    {
        printf(CFG_TAG "No config file found, using defaults\n");
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), file))
    {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r')
            continue;

        char key[128];
        char value[128];
        
        if (sscanf(line, "%127s = %127s", key, value) == 2)
        {
            // Controller settings
            if (strcmp(key, "controller.deadzone") == 0)
                g_remasterConfig.controller.analogDeadzone = (float)atof(value);
            else if (strcmp(key, "controller.sensitivity") == 0)
                g_remasterConfig.controller.analogSensitivity = (float)atof(value);
            else if (strcmp(key, "controller.invertY") == 0)
                g_remasterConfig.controller.invertYAxis = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
            else if (strcmp(key, "controller.analogMovement") == 0)
                g_remasterConfig.controller.analogMovement = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
            else if (strcmp(key, "controller.enable") == 0)
                g_remasterConfig.controller.enableController = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
            
            // HD Graphics settings
            else if (strcmp(key, "graphics.hdBackgrounds") == 0)
                g_remasterConfig.graphics.enableHDBackgrounds = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
            else if (strcmp(key, "graphics.backgroundScale") == 0)
                g_remasterConfig.graphics.backgroundScale = atoi(value);
            else if (strcmp(key, "graphics.filtering") == 0)
                g_remasterConfig.graphics.enableFiltering = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
            else if (strcmp(key, "graphics.blurredMenu") == 0)
                g_remasterConfig.graphics.enableBlurredMenu = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
            else if (strcmp(key, "graphics.menuBlurAmount") == 0)
                g_remasterConfig.graphics.menuBlurAmount = (float)atof(value);
            
            // External music settings
            else if (strcmp(key, "music.external") == 0)
                g_remasterConfig.music.enableExternalMusic = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
            else if (strcmp(key, "music.folder") == 0)
            {
                // Remove quotes if present
                char* start = value;
                if (*start == '"') start++;
                char* end = start + strlen(start) - 1;
                if (*end == '"') *end = '\0';
                strncpy(g_remasterConfig.music.musicFolder, start, 255);
                g_remasterConfig.music.musicFolder[255] = '\0';
            }

            // TTF font settings
            else if (strcmp(key, "font.enableTTF") == 0)
                g_remasterConfig.font.enableTTF = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
            else if (strcmp(key, "font.path") == 0)
            {
                // Remove quotes if present
                char* start = value;
                if (*start == '"') start++;
                char* end = start + strlen(start) - 1;
                if (*end == '"') *end = '\0';
                strncpy(g_remasterConfig.font.fontPath, start, 255);
                g_remasterConfig.font.fontPath[255] = '\0';
            }
            else if (strcmp(key, "font.size") == 0)
                g_remasterConfig.font.fontSize = atoi(value);
            else if (strcmp(key, "font.hideOriginal") == 0)
                g_remasterConfig.font.hideOriginalText = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);

            // Post-processing settings
            else if (strcmp(key, "postprocessing.bloom") == 0)
                g_remasterConfig.postProcessing.enableBloom = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
            else if (strcmp(key, "postprocessing.bloomThreshold") == 0)
                g_remasterConfig.postProcessing.bloomThreshold = (float)atof(value);
            else if (strcmp(key, "postprocessing.bloomIntensity") == 0)
                g_remasterConfig.postProcessing.bloomIntensity = (float)atof(value);
            else if (strcmp(key, "postprocessing.filmGrain") == 0)
                g_remasterConfig.postProcessing.enableFilmGrain = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
            else if (strcmp(key, "postprocessing.filmGrainIntensity") == 0)
                g_remasterConfig.postProcessing.filmGrainIntensity = (float)atof(value);
            else if (strcmp(key, "postprocessing.ssao") == 0)
                g_remasterConfig.postProcessing.enableSSAO = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
            else if (strcmp(key, "postprocessing.ssaoRadius") == 0)
                g_remasterConfig.postProcessing.ssaoRadius = (float)atof(value);
            else if (strcmp(key, "postprocessing.ssaoIntensity") == 0)
                g_remasterConfig.postProcessing.ssaoIntensity = (float)atof(value);
            else if (strcmp(key, "postprocessing.bloomPasses") == 0)
                g_remasterConfig.postProcessing.bloomPasses = atoi(value);

            // Controls settings
            else if (strcmp(key, "controls.key.up") == 0)
                g_remasterConfig.controls.keyBindings[0] = atoi(value);
            else if (strcmp(key, "controls.key.down") == 0)
                g_remasterConfig.controls.keyBindings[1] = atoi(value);
            else if (strcmp(key, "controls.key.left") == 0)
                g_remasterConfig.controls.keyBindings[2] = atoi(value);
            else if (strcmp(key, "controls.key.right") == 0)
                g_remasterConfig.controls.keyBindings[3] = atoi(value);
            else if (strcmp(key, "controls.key.action") == 0)
                g_remasterConfig.controls.keyBindings[4] = atoi(value);
            else if (strcmp(key, "controls.key.confirm") == 0)
                g_remasterConfig.controls.keyBindings[5] = atoi(value);
            else if (strcmp(key, "controls.key.cancel") == 0)
                g_remasterConfig.controls.keyBindings[6] = atoi(value);
            else if (strcmp(key, "controls.pad.up") == 0)
                g_remasterConfig.controls.gamepadBindings[0] = atoi(value);
            else if (strcmp(key, "controls.pad.down") == 0)
                g_remasterConfig.controls.gamepadBindings[1] = atoi(value);
            else if (strcmp(key, "controls.pad.left") == 0)
                g_remasterConfig.controls.gamepadBindings[2] = atoi(value);
            else if (strcmp(key, "controls.pad.right") == 0)
                g_remasterConfig.controls.gamepadBindings[3] = atoi(value);
            else if (strcmp(key, "controls.pad.action") == 0)
                g_remasterConfig.controls.gamepadBindings[4] = atoi(value);
            else if (strcmp(key, "controls.pad.confirm") == 0)
                g_remasterConfig.controls.gamepadBindings[5] = atoi(value);
            else if (strcmp(key, "controls.pad.cancel") == 0)
                g_remasterConfig.controls.gamepadBindings[6] = atoi(value);
            else if (strcmp(key, "controls.key.quickturnleft") == 0)
                g_remasterConfig.controls.keyBindings[7] = atoi(value);
            else if (strcmp(key, "controls.key.quickturnright") == 0)
                g_remasterConfig.controls.keyBindings[8] = atoi(value);
            else if (strcmp(key, "controls.pad.quickturnleft") == 0)
                g_remasterConfig.controls.gamepadBindings[7] = atoi(value);
            else if (strcmp(key, "controls.pad.quickturnright") == 0)
                g_remasterConfig.controls.gamepadBindings[8] = atoi(value);
        }
    }

    fclose(file);
    printf(CFG_OK "Remaster config loaded\n");

    // Sync detail level with HD backgrounds setting
    extern int detailLevel;
    detailLevel = g_remasterConfig.graphics.enableHDBackgrounds ? 1 : 0;
}

void saveRemasterConfig()
{
    FILE* file = fopen("aitd_remaster.cfg", "w");
    
    if (!file)
    {
        printf(CFG_ERR "Failed to save config file" CON_RESET "\n");
        return;
    }

    fprintf(file, "# Alone In The Dark - Remaster Configuration\n");
    fprintf(file, "# This file is automatically generated\n\n");

    fprintf(file, "# Controller Settings\n");
    fprintf(file, "controller.enable = %s\n", g_remasterConfig.controller.enableController ? "true" : "false");
    fprintf(file, "controller.deadzone = %.2f\n", g_remasterConfig.controller.analogDeadzone);
    fprintf(file, "controller.sensitivity = %.2f\n", g_remasterConfig.controller.analogSensitivity);
    fprintf(file, "controller.invertY = %s\n", g_remasterConfig.controller.invertYAxis ? "true" : "false");
    fprintf(file, "controller.analogMovement = %s\n\n", g_remasterConfig.controller.analogMovement ? "true" : "false");

    fprintf(file, "# HD Graphics Settings\n");
    fprintf(file, "graphics.hdBackgrounds = %s\n", g_remasterConfig.graphics.enableHDBackgrounds ? "true" : "false");
    fprintf(file, "graphics.backgroundScale = %d\n", g_remasterConfig.graphics.backgroundScale);
    fprintf(file, "graphics.filtering = %s\n", g_remasterConfig.graphics.enableFiltering ? "true" : "false");
    fprintf(file, "graphics.blurredMenu = %s\n", g_remasterConfig.graphics.enableBlurredMenu ? "true" : "false");
    fprintf(file, "graphics.menuBlurAmount = %.1f\n\n", g_remasterConfig.graphics.menuBlurAmount);

    fprintf(file, "# External Music Settings\n");
    fprintf(file, "music.external = %s\n", g_remasterConfig.music.enableExternalMusic ? "true" : "false");
    fprintf(file, "music.folder = \"%s\"\n\n", g_remasterConfig.music.musicFolder);

    fprintf(file, "# TTF Font Settings\n");
    fprintf(file, "font.enableTTF = %s\n", g_remasterConfig.font.enableTTF ? "true" : "false");
    fprintf(file, "font.path = \"%s\"\n", g_remasterConfig.font.fontPath);
    fprintf(file, "font.size = %d\n", g_remasterConfig.font.fontSize);
    fprintf(file, "font.hideOriginal = %s\n\n", g_remasterConfig.font.hideOriginalText ? "true" : "false");

    fprintf(file, "# Post-Processing Settings\n");
    fprintf(file, "postprocessing.bloom = %s\n", g_remasterConfig.postProcessing.enableBloom ? "true" : "false");
    fprintf(file, "postprocessing.bloomThreshold = %.2f\n", g_remasterConfig.postProcessing.bloomThreshold);
    fprintf(file, "postprocessing.bloomIntensity = %.2f\n", g_remasterConfig.postProcessing.bloomIntensity);
    fprintf(file, "postprocessing.filmGrain = %s\n", g_remasterConfig.postProcessing.enableFilmGrain ? "true" : "false");
    fprintf(file, "postprocessing.filmGrainIntensity = %.3f\n", g_remasterConfig.postProcessing.filmGrainIntensity);
    fprintf(file, "postprocessing.ssao = %s\n", g_remasterConfig.postProcessing.enableSSAO ? "true" : "false");
    fprintf(file, "postprocessing.ssaoRadius = %.1f\n", g_remasterConfig.postProcessing.ssaoRadius);
    fprintf(file, "postprocessing.ssaoIntensity = %.2f\n", g_remasterConfig.postProcessing.ssaoIntensity);
    fprintf(file, "postprocessing.bloomPasses = %d\n", g_remasterConfig.postProcessing.bloomPasses);

    fprintf(file, "\n# Controls Settings (scancode / button IDs)\n");
    fprintf(file, "controls.key.up = %d\n", g_remasterConfig.controls.keyBindings[0]);
    fprintf(file, "controls.key.down = %d\n", g_remasterConfig.controls.keyBindings[1]);
    fprintf(file, "controls.key.left = %d\n", g_remasterConfig.controls.keyBindings[2]);
    fprintf(file, "controls.key.right = %d\n", g_remasterConfig.controls.keyBindings[3]);
    fprintf(file, "controls.key.action = %d\n", g_remasterConfig.controls.keyBindings[4]);
    fprintf(file, "controls.key.confirm = %d\n", g_remasterConfig.controls.keyBindings[5]);
    fprintf(file, "controls.key.cancel = %d\n", g_remasterConfig.controls.keyBindings[6]);
    fprintf(file, "controls.pad.up = %d\n", g_remasterConfig.controls.gamepadBindings[0]);
    fprintf(file, "controls.pad.down = %d\n", g_remasterConfig.controls.gamepadBindings[1]);
    fprintf(file, "controls.pad.left = %d\n", g_remasterConfig.controls.gamepadBindings[2]);
    fprintf(file, "controls.pad.right = %d\n", g_remasterConfig.controls.gamepadBindings[3]);
    fprintf(file, "controls.pad.action = %d\n", g_remasterConfig.controls.gamepadBindings[4]);
    fprintf(file, "controls.pad.confirm = %d\n", g_remasterConfig.controls.gamepadBindings[5]);
    fprintf(file, "controls.pad.cancel = %d\n", g_remasterConfig.controls.gamepadBindings[6]);
    fprintf(file, "controls.key.quickturnleft = %d\n", g_remasterConfig.controls.keyBindings[7]);
    fprintf(file, "controls.key.quickturnright = %d\n", g_remasterConfig.controls.keyBindings[8]);
    fprintf(file, "controls.pad.quickturnleft = %d\n", g_remasterConfig.controls.gamepadBindings[7]);
    fprintf(file, "controls.pad.quickturnright = %d\n", g_remasterConfig.controls.gamepadBindings[8]);

    fclose(file);
    printf(CFG_OK "Remaster config saved\n");
}
