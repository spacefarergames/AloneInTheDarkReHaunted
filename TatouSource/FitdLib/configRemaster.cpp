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
    g_remasterConfig.graphics.enableHints = true;
    g_remasterConfig.graphics.enableArtwork = true;
    g_remasterConfig.graphics.fullscreen = false;
    strcpy(g_remasterConfig.graphics.rendererBackend, "auto");

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
    g_remasterConfig.postProcessing.enableVignette = false;
    g_remasterConfig.postProcessing.vignetteIntensity = 0.35f;
    g_remasterConfig.postProcessing.vignetteRadius = 0.75f;
    // SSGI defaults
    g_remasterConfig.postProcessing.enableSSGI = false;
    g_remasterConfig.postProcessing.ssgiRadius = 300.0f;
    g_remasterConfig.postProcessing.ssgiIntensity = 0.6f;
    g_remasterConfig.postProcessing.ssgiNumSamples = 16;
    // Light Probe defaults
    g_remasterConfig.postProcessing.enableLightProbes = false;
    g_remasterConfig.postProcessing.lightProbeIntensity = 0.5f;

    // External music defaults
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
    g_remasterConfig.controls.keyBindings[9] = SDL_SCANCODE_LSHIFT;
    g_remasterConfig.controls.gamepadBindings[9] = SDL_GAMEPAD_BUTTON_LEFT_STICK;

    // Mask dumping/loading defaults
    g_remasterConfig.masks.dumpEnabled = false;    // Don't auto-dump masks during normal gameplay
    g_remasterConfig.masks.loadEnabled = true;     // Always load hand-edited HD masks if available

    // Sequence dumping/loading defaults
    g_remasterConfig.sequences.dumpEnabled = false;  // Don't auto-dump sequence frames during normal gameplay
    g_remasterConfig.sequences.loadEnabled = true;   // Load HD replacement sequence frames if available

    // Game data defaults
    g_remasterConfig.gameData.steamless = false;   // Allow automatic file copying/installation by default

    // Debug / diagnostics defaults
    g_remasterConfig.debug.logLifeScripts = false;  // Don't log life scripts by default (very verbose)
    g_remasterConfig.debug.dumpLifeScripts = false;   // Don't dump life scripts by default
    g_remasterConfig.debug.generateNativeLifeScripts = false; // Don't generate native C code by default
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
            else if (strcmp(key, "gameplay.hints") == 0)
                g_remasterConfig.graphics.enableHints = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
            else if (strcmp(key, "graphics.useArtwork") == 0)
                g_remasterConfig.graphics.enableArtwork = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
            else if (strcmp(key, "graphics.fullscreen") == 0)
                g_remasterConfig.graphics.fullscreen = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
            else if (strcmp(key, "graphics.renderer") == 0)
            {
                strncpy(g_remasterConfig.graphics.rendererBackend, value, 31);
                g_remasterConfig.graphics.rendererBackend[31] = '\0';
            }

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
            else if (strcmp(key, "postprocessing.vignette") == 0)
                g_remasterConfig.postProcessing.enableVignette = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
            else if (strcmp(key, "postprocessing.vignetteIntensity") == 0)
                g_remasterConfig.postProcessing.vignetteIntensity = (float)atof(value);
            else if (strcmp(key, "postprocessing.vignetteRadius") == 0)
                g_remasterConfig.postProcessing.vignetteRadius = (float)atof(value);
            // SSGI settings
            else if (strcmp(key, "postprocessing.ssgi") == 0)
                g_remasterConfig.postProcessing.enableSSGI = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
            else if (strcmp(key, "postprocessing.ssgiRadius") == 0)
                g_remasterConfig.postProcessing.ssgiRadius = (float)atof(value);
            else if (strcmp(key, "postprocessing.ssgiIntensity") == 0)
                g_remasterConfig.postProcessing.ssgiIntensity = (float)atof(value);
            else if (strcmp(key, "postprocessing.ssgiNumSamples") == 0)
                g_remasterConfig.postProcessing.ssgiNumSamples = atoi(value);
            // Light Probe settings
            else if (strcmp(key, "postprocessing.lightProbes") == 0)
                g_remasterConfig.postProcessing.enableLightProbes = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
            else if (strcmp(key, "postprocessing.lightProbeIntensity") == 0)
                g_remasterConfig.postProcessing.lightProbeIntensity = (float)atof(value);

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
            else if (strcmp(key, "controls.key.run") == 0)
                g_remasterConfig.controls.keyBindings[9] = atoi(value);
            else if (strcmp(key, "controls.pad.run") == 0)
                g_remasterConfig.controls.gamepadBindings[9] = atoi(value);

            // Mask dumping/loading settings
            else if (strcmp(key, "masks.dump") == 0)
                g_remasterConfig.masks.dumpEnabled = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
            else if (strcmp(key, "masks.load") == 0)
                g_remasterConfig.masks.loadEnabled = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);

            // Sequence dumping/loading settings
            else if (strcmp(key, "sequences.dump") == 0)
                g_remasterConfig.sequences.dumpEnabled = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
            else if (strcmp(key, "sequences.load") == 0)
                g_remasterConfig.sequences.loadEnabled = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);

            // Game data settings
            else if (strcmp(key, "gamedata.steamless") == 0)
                g_remasterConfig.gameData.steamless = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);

            // Debug / diagnostics settings
            else if (strcmp(key, "debug.logLifeScripts") == 0)
                g_remasterConfig.debug.logLifeScripts = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
            else if (strcmp(key, "debug.dumpLifeScripts") == 0)
                g_remasterConfig.debug.dumpLifeScripts = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
            else if (strcmp(key, "debug.generateNativeLifeScripts") == 0)
                g_remasterConfig.debug.generateNativeLifeScripts = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
        }
    }

    fclose(file);

    // Validate and clamp numeric config values to safe ranges
    if (g_remasterConfig.controller.analogDeadzone < 0.0f) g_remasterConfig.controller.analogDeadzone = 0.0f;
    if (g_remasterConfig.controller.analogDeadzone > 0.9f) g_remasterConfig.controller.analogDeadzone = 0.9f;
    if (g_remasterConfig.controller.analogSensitivity < 0.1f) g_remasterConfig.controller.analogSensitivity = 0.1f;
    if (g_remasterConfig.controller.analogSensitivity > 5.0f) g_remasterConfig.controller.analogSensitivity = 5.0f;

    if (g_remasterConfig.graphics.backgroundScale < 1) g_remasterConfig.graphics.backgroundScale = 1;
    if (g_remasterConfig.graphics.backgroundScale > 8) g_remasterConfig.graphics.backgroundScale = 8;
    if (g_remasterConfig.graphics.menuBlurAmount < 0.0f) g_remasterConfig.graphics.menuBlurAmount = 0.0f;
    if (g_remasterConfig.graphics.menuBlurAmount > 10.0f) g_remasterConfig.graphics.menuBlurAmount = 10.0f;

    if (g_remasterConfig.postProcessing.bloomThreshold < 0.0f) g_remasterConfig.postProcessing.bloomThreshold = 0.0f;
    if (g_remasterConfig.postProcessing.bloomThreshold > 1.0f) g_remasterConfig.postProcessing.bloomThreshold = 1.0f;
    if (g_remasterConfig.postProcessing.bloomIntensity < 0.0f) g_remasterConfig.postProcessing.bloomIntensity = 0.0f;
    if (g_remasterConfig.postProcessing.bloomIntensity > 5.0f) g_remasterConfig.postProcessing.bloomIntensity = 5.0f;
    if (g_remasterConfig.postProcessing.bloomPasses < 1) g_remasterConfig.postProcessing.bloomPasses = 1;
    if (g_remasterConfig.postProcessing.bloomPasses > 8) g_remasterConfig.postProcessing.bloomPasses = 8;
    if (g_remasterConfig.postProcessing.filmGrainIntensity < 0.0f) g_remasterConfig.postProcessing.filmGrainIntensity = 0.0f;
    if (g_remasterConfig.postProcessing.filmGrainIntensity > 1.0f) g_remasterConfig.postProcessing.filmGrainIntensity = 1.0f;
    if (g_remasterConfig.postProcessing.ssaoRadius < 0.0f) g_remasterConfig.postProcessing.ssaoRadius = 0.0f;
    if (g_remasterConfig.postProcessing.ssaoIntensity < 0.0f) g_remasterConfig.postProcessing.ssaoIntensity = 0.0f;
    if (g_remasterConfig.postProcessing.ssaoIntensity > 5.0f) g_remasterConfig.postProcessing.ssaoIntensity = 5.0f;
    if (g_remasterConfig.postProcessing.ssgiNumSamples < 1) g_remasterConfig.postProcessing.ssgiNumSamples = 1;
    if (g_remasterConfig.postProcessing.ssgiNumSamples > 64) g_remasterConfig.postProcessing.ssgiNumSamples = 64;

    if (g_remasterConfig.font.fontSize < 8) g_remasterConfig.font.fontSize = 8;
    if (g_remasterConfig.font.fontSize > 72) g_remasterConfig.font.fontSize = 72;

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
    fprintf(file, "graphics.menuBlurAmount = %.1f\n", g_remasterConfig.graphics.menuBlurAmount);
    fprintf(file, "gameplay.hints = %s\n", g_remasterConfig.graphics.enableHints ? "true" : "false");
    fprintf(file, "graphics.useArtwork = %s\n", g_remasterConfig.graphics.enableArtwork ? "true" : "false");
    fprintf(file, "graphics.fullscreen = %s\n", g_remasterConfig.graphics.fullscreen ? "true" : "false");
    fprintf(file, "# Renderer backend: auto, d3d11, d3d12, opengl, vulkan, metal\n");
    fprintf(file, "graphics.renderer = %s\n\n", g_remasterConfig.graphics.rendererBackend);

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
    fprintf(file, "postprocessing.vignette = %s\n", g_remasterConfig.postProcessing.enableVignette ? "true" : "false");
    fprintf(file, "postprocessing.vignetteIntensity = %.2f\n", g_remasterConfig.postProcessing.vignetteIntensity);
    fprintf(file, "postprocessing.vignetteRadius = %.2f\n", g_remasterConfig.postProcessing.vignetteRadius);
    // SSGI settings
    fprintf(file, "postprocessing.ssgi = %s\n", g_remasterConfig.postProcessing.enableSSGI ? "true" : "false");
    fprintf(file, "postprocessing.ssgiRadius = %.1f\n", g_remasterConfig.postProcessing.ssgiRadius);
    fprintf(file, "postprocessing.ssgiIntensity = %.2f\n", g_remasterConfig.postProcessing.ssgiIntensity);
    fprintf(file, "postprocessing.ssgiNumSamples = %d\n", g_remasterConfig.postProcessing.ssgiNumSamples);
    // Light Probe settings
    fprintf(file, "postprocessing.lightProbes = %s\n", g_remasterConfig.postProcessing.enableLightProbes ? "true" : "false");
    fprintf(file, "postprocessing.lightProbeIntensity = %.2f\n", g_remasterConfig.postProcessing.lightProbeIntensity);

    fprintf(file, "\n# Controls Settings\n");
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
    fprintf(file, "controls.key.run = %d\n", g_remasterConfig.controls.keyBindings[9]);
    fprintf(file, "controls.pad.run = %d\n", g_remasterConfig.controls.gamepadBindings[9]);

    fprintf(file, "\n# Mask Dumping/Loading Settings\n");
    fprintf(file, "masks.dump = %s\n", g_remasterConfig.masks.dumpEnabled ? "true" : "false");
    fprintf(file, "masks.load = %s\n", g_remasterConfig.masks.loadEnabled ? "true" : "false");

    fprintf(file, "\n# Sequence Dumping/Loading Settings\n");
    fprintf(file, "sequences.dump = %s\n", g_remasterConfig.sequences.dumpEnabled ? "true" : "false");
    fprintf(file, "sequences.load = %s\n", g_remasterConfig.sequences.loadEnabled ? "true" : "false");

    fprintf(file, "\n# Game Data Settings\n");
    fprintf(file, "gamedata.steamless = %s\n", g_remasterConfig.gameData.steamless ? "true" : "false");

    fprintf(file, "\n# Debug / Diagnostics Settings\n");
    fprintf(file, "debug.logLifeScripts = %s\n", g_remasterConfig.debug.logLifeScripts ? "true" : "false");
    fprintf(file, "debug.dumpLifeScripts = %s\n", g_remasterConfig.debug.dumpLifeScripts ? "true" : "false");
    fprintf(file, "debug.generateNativeLifeScripts = %s\n", g_remasterConfig.debug.generateNativeLifeScripts ? "true" : "false");

    fclose(file);
    printf(CFG_OK "Remaster config saved\n");
}
