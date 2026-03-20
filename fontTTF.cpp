///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Author: Jake Jackson (jake@spacefarergames.com)
//
// TrueType font rendering for remastered text display
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "fontTTF.h"
#include "configRemaster.h"
#include "font.h"
#include <vector>
#include <string>
#include <bgfx/bgfx.h>

#ifdef USE_IMGUI
#include "imgui.h"

static ImFont* g_ttfFont = nullptr;
static std::vector<TTFTextCommand> g_textQueue;
static bool g_ttfInitialized = false;

extern s16 g_fontInterWordSpace;
extern s16 g_fontInterLetterSpace;
extern s16 fontSm1; // character height
extern char* fontVar1; // font data pointer

void initTTFFont()
{
    printf("=== TTF INIT START ===\n");
    printf("TTF init: already initialized=%d\n", g_ttfInitialized);

    if (g_ttfInitialized)
        return;

    printf("TTF init: config enabled=%d\n", g_remasterConfig.font.enableTTF);

    if (!g_remasterConfig.font.enableTTF)
        return;

    printf("TTF init: Getting ImGui IO...\n");
    ImGuiIO& io = ImGui::GetIO();

    printf("TTF init: Fonts=%p\n", io.Fonts);

    // Try to load custom font
    ImFontConfig config;
    config.OversampleH = 3;
    config.OversampleV = 3;
    config.PixelSnapH = false;

    const ImWchar* ranges = io.Fonts->GetGlyphRangesDefault();

    // Scale font size based on target resolution
    extern int outputResolution[2];

    // If resolution not yet set, use default scale
    float scaleFactor = (outputResolution[1] > 0) ? (outputResolution[1] / 200.0f) : 1.0f;
    float scaledFontSize = g_remasterConfig.font.fontSize * scaleFactor;

    // Ensure we have a valid font size (ImGui requires > 0)
    if (scaledFontSize <= 0.0f)
    {
        printf("WARNING: Invalid scaled font size %.2f, using base size %d\n", 
            scaledFontSize, g_remasterConfig.font.fontSize);
        scaledFontSize = (float)g_remasterConfig.font.fontSize;
    }

    // Ensure minimum font size
    if (scaledFontSize < 8.0f)
    {
        printf("WARNING: Font size %.2f too small, using 8.0\n", scaledFontSize);
        scaledFontSize = 8.0f;
    }

    printf("TTF init: resolution=%dx%d, base_size=%d, scale=%.2f, scaled_size=%.1f\n", 
        outputResolution[0], outputResolution[1],
        g_remasterConfig.font.fontSize, scaleFactor, scaledFontSize);

    printf("TTF init: Loading font from '%s'...\n", g_remasterConfig.font.fontPath);

    g_ttfFont = io.Fonts->AddFontFromFileTTF(
        g_remasterConfig.font.fontPath, 
        scaledFontSize, 
        &config, 
        ranges
    );

    if (g_ttfFont)
    {
        printf("TTF Font loaded: %s (font ptr=%p)\n", g_remasterConfig.font.fontPath, g_ttfFont);
    }
    else
    {
        printf("Failed to load TTF font: %s\n", g_remasterConfig.font.fontPath);
        printf("Trying fallback system font...\n");

        // Try common Windows system fonts as fallback
        const char* fallbackFonts[] = {
            "C:/Windows/Fonts/arial.ttf",
            "C:/Windows/Fonts/times.ttf",
            "C:/Windows/Fonts/cour.ttf"
        };

        for (const char* fallback : fallbackFonts)
        {
            printf("TTF init: Trying fallback '%s'...\n", fallback);
            g_ttfFont = io.Fonts->AddFontFromFileTTF(fallback, scaledFontSize, &config, ranges);
            if (g_ttfFont)
            {
                printf("Loaded fallback font: %s (font ptr=%p)\n", fallback, g_ttfFont);
                break;
            }
        }

        if (!g_ttfFont)
        {
            printf("ERROR: All fonts failed, TTF rendering disabled\n");
            g_remasterConfig.font.enableTTF = false;
            return;
        }
    }

    g_ttfInitialized = true;
    printf("TTF system initialized successfully! font=%p, initialized=%d\n", g_ttfFont, g_ttfInitialized);

    // Rebuild the font atlas texture since we added a new font after initial creation
    extern void imguiRebuildFontTexture();
    imguiRebuildFontTexture();
    printf("TTF init: Font atlas texture rebuilt\n");

    printf("=== TTF INIT END ===\n");
}

void shutdownTTFFont()
{
    g_textQueue.clear();
    g_ttfFont = nullptr;
    g_ttfInitialized = false;
}

int getTTFTextWidth(u8* string)
{
    if (!g_ttfFont || !g_ttfInitialized)
        return 0;

    ImGui::PushFont(g_ttfFont);

    // Convert to string for ImGui
    ImVec2 textSize = ImGui::CalcTextSize((const char*)string);

    ImGui::PopFont();

    return (int)textSize.x;
}

// Global frame counter - incremented once per actual render frame
static int g_ttfFrameCounter = 0;
static int g_lastTextQueueFrame = 0;

// Track menu state to detect transitions
static int g_lastMenuSelection = -1;
static bool g_wasInMenu = false;
static bool g_wasPaused = false;

// Track resolution for resize/fullscreen detection
static int g_lastResolutionX = 0;
static int g_lastResolutionY = 0;
static bool g_lastFullscreenState = false;

void incrementTTFFrameCounter()
{
    g_ttfFrameCounter++;
}

// Call this when menu selection changes (up/down navigation)
void notifyTTFMenuSelectionChanged()
{
    if (g_remasterConfig.font.enableTTF)
    {
        printf("TTF: Menu selection changed, clearing text queue\n");
        clearTTFTextQueue();
        g_lastTextQueueFrame = g_ttfFrameCounter; // Reset frame counter to prevent immediate re-clear
    }
}

// Call this when entering/exiting menus or pause state
void notifyTTFMenuStateChanged(bool inMenu, bool isPaused)
{
    if (!g_remasterConfig.font.enableTTF)
        return;

    // Detect transitions
    bool menuStateChanged = (inMenu != g_wasInMenu);
    bool pauseStateChanged = (isPaused != g_wasPaused);

    if (menuStateChanged || pauseStateChanged)
    {
        printf("TTF: Menu/pause state changed (inMenu: %d->%d, paused: %d->%d), clearing text\n",
            g_wasInMenu, inMenu, g_wasPaused, isPaused);
        clearTTFTextQueue();
        g_lastTextQueueFrame = g_ttfFrameCounter;
    }

    // If we just exited menu and unpaused (back to gameplay), clear text
    if (!inMenu && !isPaused && (g_wasInMenu || g_wasPaused))
    {
        printf("TTF: Returned to gameplay, clearing text\n");
        clearTTFTextQueue();
        g_lastTextQueueFrame = g_ttfFrameCounter;
    }

    g_wasInMenu = inMenu;
    g_wasPaused = isPaused;
}

void queueTTFText(int x, int y, u8* string, int color, bool shadow, int shadowColor)
{
    if (!g_remasterConfig.font.enableTTF || !g_ttfInitialized || !g_ttfFont)
    {
        printf("TTF queue skip: enabled=%d, initialized=%d, font=%p\n", 
            g_remasterConfig.font.enableTTF, g_ttfInitialized, g_ttfFont);
        return;
    }

    // If it's been more than 10 frames since text was last queued, clear old text
    // This handles menu transitions and state changes
    if (g_ttfFrameCounter > g_lastTextQueueFrame + 10)
    {
        // New batch of text - clear old queue
        clearTTFTextQueue();
        printf("TTF: New text batch detected, cleared old queue (frame gap: %d)\n", 
            g_ttfFrameCounter - g_lastTextQueueFrame);
    }
    g_lastTextQueueFrame = g_ttfFrameCounter;

    // Make a copy of the string since it might be temporary
    int len = strlen((const char*)string);
    u8* textCopy = new u8[len + 1];
    strcpy((char*)textCopy, (const char*)string);

    TTFTextCommand cmd;
    cmd.x = x;
    cmd.y = y;
    cmd.text = textCopy;
    cmd.color = color;
    cmd.shadow = shadow;
    cmd.shadowColor = shadowColor;

    g_textQueue.push_back(cmd);
    printf("TTF queued: '%s' at (%d, %d) color=%d queue_size=%zu\n", 
        string, x, y, color, g_textQueue.size());

    // Detect fade-out effect: when color=31 and queue reaches 55-56 or 76 items,
    // this indicates the text is fading out and should be cleared
    size_t queueSize = g_textQueue.size();
    if (color == 31 && (queueSize == 55 || queueSize == 56 || queueSize == 60 || queueSize == 65 || queueSize == 76))
    {
        printf("TTF: Fade-out detected (color=%d, queue_size=%zu), clearing text\n", color, queueSize);
        clearTTFTextQueue();
    }
}

void renderTTFText()
{
    if (!g_remasterConfig.font.enableTTF || !g_ttfInitialized || !g_ttfFont)
        return;

    if (g_textQueue.empty())
        return;

    // Get palette colors (RGB format: 3 bytes per color)
    extern char RGB_Pal[256 * 3];

    // Get output resolution for coordinate scaling
    extern int outputResolution[2];

    // Get fullscreen state to apply centering offset
    extern bool gIsFullscreen;

    // Detect resolution or fullscreen changes and reload font at correct size
    bool needFontReload = false;
    if (outputResolution[0] != g_lastResolutionX || outputResolution[1] != g_lastResolutionY)
    {
        printf("TTF: Resolution changed from %dx%d to %dx%d\n",
            g_lastResolutionX, g_lastResolutionY, outputResolution[0], outputResolution[1]);
        g_lastResolutionX = outputResolution[0];
        g_lastResolutionY = outputResolution[1];
        // Don't clear queue - text coordinates are in game space (320x200)
        // and will automatically scale to the new resolution below
    }

    // Calculate the viewport size (full window, no menu bar)
    float viewportWidth = (float)outputResolution[0];
    float viewportHeight = (float)outputResolution[1];

    // Calculate scale factors from game coordinates (320x200) to viewport
    float scaleX = viewportWidth / 320.0f;
    float scaleY = viewportHeight / 200.0f;

    // Apply fullscreen centering offset (shift text right by 85 pixels when fullscreen)
    float fullscreenOffsetX = gIsFullscreen ? 70.0f : 0.0f;

    // Use foreground draw list directly - no window, no background
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    ImGui::PushFont(g_ttfFont);

    for (const auto& cmd : g_textQueue)
    {
        // Scale from game coordinates to viewport and apply fullscreen offset
        float screenX = cmd.x * scaleX + fullscreenOffsetX;
        float screenY = cmd.y * scaleY;

        // Convert palette index to RGB
        int colorIndex = cmd.color;
        if (colorIndex < 0 || colorIndex >= 256)
            colorIndex = 255;

        unsigned char r = (unsigned char)RGB_Pal[colorIndex * 3 + 0];
        unsigned char g = (unsigned char)RGB_Pal[colorIndex * 3 + 1];
        unsigned char b = (unsigned char)RGB_Pal[colorIndex * 3 + 2];

        // Fallback to white if palette entry is black
        if (r == 0 && g == 0 && b == 0 && colorIndex != 0)
        {
            r = g = b = 255;
        }

        ImU32 color = IM_COL32(r, g, b, 255);

        // Render shadow if requested
        if (cmd.shadow)
        {
            int shadowColorIndex = cmd.shadowColor;
            if (shadowColorIndex < 0 || shadowColorIndex >= 256)
                shadowColorIndex = 0;

            unsigned char sr = (unsigned char)RGB_Pal[shadowColorIndex * 3 + 0];
            unsigned char sg = (unsigned char)RGB_Pal[shadowColorIndex * 3 + 1];
            unsigned char sb = (unsigned char)RGB_Pal[shadowColorIndex * 3 + 2];

            ImU32 shadowColor = IM_COL32(sr, sg, sb, 255);
            drawList->AddText(ImVec2(screenX, screenY + scaleY), shadowColor, (const char*)cmd.text);
        }

        // Render main text
        drawList->AddText(ImVec2(screenX, screenY), color, (const char*)cmd.text);
    }

    ImGui::PopFont();
}

void clearTTFTextQueue()
{
    // Clean up any remaining strings
    for (auto& cmd : g_textQueue)
    {
        delete[] cmd.text;
    }
    g_textQueue.clear();
}

#else // !USE_IMGUI

// Stub implementations when ImGui is disabled
void initTTFFont() {}
void shutdownTTFFont() {}
int getTTFTextWidth(u8* string) { return 0; }
void queueTTFText(int x, int y, u8* string, int color, bool shadow, int shadowColor) {}
void incrementTTFFrameCounter() {}
void notifyTTFMenuSelectionChanged() {}
void notifyTTFMenuStateChanged(bool inMenu, bool isPaused) {}
void renderTTFText() {}
void clearTTFTextQueue() {}

#endif // USE_IMGUI
