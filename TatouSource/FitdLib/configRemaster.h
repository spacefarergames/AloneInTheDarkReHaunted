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
    // Remaster options dialog behavior
    struct {
        bool showOptionsAtStartup;
    } ui;

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
        bool enableHints;
        bool enableArtwork;
        bool fullscreen;
        // Renderer backend selection. Valid values: "auto", "d3d11", "d3d12",
        // "opengl", "vulkan", "metal". "auto" lets bgfx choose the best
        // backend for the current platform.
        char rendererBackend[32];
        int msaaLevel;          // MSAA anti-aliasing: 0=off, 2=2x, 4=4x, 8=8x, 16=16x
        bool enableWallDepth;   // Render wall collision geometry to depth buffer for SSAO edge darkening
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
        bool enableVignette;
        float vignetteIntensity;
        float vignetteRadius;
        // SSGI (Screen-Space Global Illumination)
        bool enableSSGI;
        float ssgiRadius;
        float ssgiIntensity;
        int ssgiNumSamples;
        // Light Probes
        bool enableLightProbes;
        float lightProbeIntensity;
        // Global cinematic color pipeline. Values are deliberately subtle so
        // original artwork keeps its authored palette while gaining depth.
        bool enableColorGrading;
        float exposure;
        float contrast;
        float saturation;
        float temperature;
        float shadowLift;
        float highlightRolloff;
    } postProcessing;

    // Animation presentation. Root motion remains linear/gameplay-authored;
    // smoothing is applied only to the rendered skeletal pose.
    struct {
        bool enablePoseSmoothing;
        float poseSmoothingStrength;
    } animation;

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
        int keyBindings[10];     // SDL_Scancode per action (ACTION_COUNT = 10)
        int gamepadBindings[10]; // SDL_GamepadButton per action
    } controls;

    // Mask dumping/loading settings
    struct {
        bool dumpEnabled;       // Dump generated masks to PNG for artist editing (default: false)
        bool loadEnabled;       // Load hand-edited masks from masks_hd/ directory (default: true)
    } masks;

    // Sequence (FMV cutscene) dumping/loading settings
    struct {
        bool dumpEnabled;       // Dump decoded sequence frames to PNG in sequences_dump/ (default: false)
        bool loadEnabled;       // Load HD replacement frames from sequences_hd/ (default: true)
    } sequences;

    // Background (original PAK) dumping settings
    struct {
        bool dumpEnabled;       // Dump every original PAK background to PNG in backgrounds_dump/ on launch (default: false)
    } backgrounds;

    // Game data settings
    struct {
        bool steamless;         // Disable automatic file copying/installation when true (default: false)
        bool jackMode;          // Run "Jack in the Dark" instead of AITD1; uses JACK\ folder rather than INDARK\ (default: false)
    } gameData;

    // Debug / diagnostics settings
    struct {
        bool enableGraphicsValidation; // Opt-in graphics API validation layers (developer use)
        bool logLifeScripts;    // Log every LIFE macro dispatch to console (default: false)
        bool dumpLifeScripts;   // Dump all LISTLIFE scripts to file on startup (default: false)
        bool generateNativeLifeScripts; // Generate native C code for all life scripts (default: false)
        bool enableNativeLifeScripts;   // Use compiled native C replacements for safe life scripts (default: false)
    } debug;
};

extern RemasterConfig g_remasterConfig;

// Configuration functions
void loadRemasterConfig();
void saveRemasterConfig();
void initDefaultRemasterConfig();

#endif // _CONFIG_REMASTER_H_
