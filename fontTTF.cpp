///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// TrueType font rendering for remastered text display
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "fontTTF.h"
#include "configRemaster.h"
#include "consoleLog.h"
#include "font.h"
#include <vector>
#include <string>
#include <bgfx/bgfx.h>

#ifdef USE_IMGUI
#include "imgui.h"

static ImFont* g_ttfFont = nullptr;
static std::vector<TTFTextCommand> g_textQueue;
static bool g_ttfInitialized = false;
static float g_fontLoadResolutionY = 0.0f;

extern s16 g_fontInterWordSpace;
extern s16 g_fontInterLetterSpace;
extern s16 fontSm1; // character height
extern char* fontVar1; // font data pointer

// CP850 (DOS Western Europe) to Unicode mapping for bytes 0x80-0xFF
static const unsigned short cp850ToUnicode[128] = {
    0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, 0x00E7, // 80-87
    0x00EA, 0x00EB, 0x00E8, 0x00EF, 0x00EE, 0x00EC, 0x00C4, 0x00C5, // 88-8F
    0x00C9, 0x00E6, 0x00C6, 0x00F4, 0x00F6, 0x00F2, 0x00FB, 0x00F9, // 90-97
    0x00FF, 0x00D6, 0x00DC, 0x00F8, 0x00A3, 0x00D8, 0x00D7, 0x0192, // 98-9F
    0x00E1, 0x00ED, 0x00F3, 0x00FA, 0x00F1, 0x00D1, 0x00AA, 0x00BA, // A0-A7
    0x00BF, 0x00AE, 0x00AC, 0x00BD, 0x00BC, 0x00A1, 0x00AB, 0x00BB, // A8-AF
    0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x00C1, 0x00C2, 0x00C0, // B0-B7
    0x00A9, 0x2563, 0x2551, 0x2557, 0x255D, 0x00A2, 0x00A5, 0x2510, // B8-BF
    0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x00E3, 0x00C3, // C0-C7
    0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x00A4, // C8-CF
    0x00F0, 0x00D0, 0x00CA, 0x00CB, 0x00C8, 0x0131, 0x00CD, 0x00CE, // D0-D7
    0x00CF, 0x2518, 0x250C, 0x2588, 0x2584, 0x00A6, 0x00CC, 0x2580, // D8-DF
    0x00D3, 0x00DF, 0x00D4, 0x00D2, 0x00F5, 0x00D5, 0x00B5, 0x00FE, // E0-E7
    0x00DE, 0x00DA, 0x00DB, 0x00D9, 0x00FD, 0x00DD, 0x00AF, 0x00B4, // E8-EF
    0x00AD, 0x00B1, 0x2017, 0x00BE, 0x00B6, 0x00A7, 0x00F7, 0x00B8, // F0-F7
    0x00B0, 0x00A8, 0x00B7, 0x00B9, 0x00B3, 0x00B2, 0x25A0, 0x00A0, // F8-FF
};

// Convert a CP850-encoded byte string to UTF-8
static std::string cp850ToUtf8(const u8* src)
{
    std::string result;
    while (*src)
    {
        unsigned char ch = *src++;
        if (ch < 0x80)
        {
            result += (char)ch;
        }
        else
        {
            unsigned short uni = cp850ToUnicode[ch - 0x80];
            if (uni < 0x0080)
            {
                result += (char)uni;
            }
            else if (uni < 0x0800)
            {
                result += (char)(0xC0 | (uni >> 6));
                result += (char)(0x80 | (uni & 0x3F));
            }
            else
            {
                result += (char)(0xE0 | (uni >> 12));
                result += (char)(0x80 | ((uni >> 6) & 0x3F));
                result += (char)(0x80 | (uni & 0x3F));
            }
        }
    }
    return result;
}

// Shared text transformation: CP850 -> UTF-8 + all display substitutions.
// Both getTTFTextWidth() and queueTTFText() MUST use this so width
// measurement matches rendering (required for correct centering).
static std::string transformTTFText(const u8* src)
{
    std::string utf8Str = cp850ToUtf8(src);

    // Prepend copyright symbol to the remaster copyright notice
    if (utf8Str.find("2026 Infogrames") != std::string::npos)
        utf8Str = "\xC2\xA9 " + utf8Str;

    // Replace DOS references with Windows
    {
        size_t pos = 0;
        while ((pos = utf8Str.find("DOS", pos)) != std::string::npos)
        {
            utf8Str.replace(pos, 3, "Windows");
            pos += 7;
        }
    }

    // Update original credit line
    {
        size_t pos = utf8Str.find("Infogrames 1992");
        if (pos != std::string::npos)
            utf8Str.replace(pos, 15, "Infogrames 2026 / Spacefarer Retro Remasters");
    }

    return utf8Str;
}

void initTTFFont()
{
    enableConsoleColors();
    printf(TTF_TAG CON_BOLD "════════════════════════════════════════\n" CON_RESET);
    printf(TTF_TAG CON_BOLD "  TrueType Font System - Initializing\n" CON_RESET);
    printf(TTF_TAG CON_BOLD "════════════════════════════════════════\n" CON_RESET);
    printf(TTF_TAG "Initialized: %s\n", g_ttfInitialized ? CON_GREEN "yes" CON_RESET : CON_DIM "no" CON_RESET);

    if (g_ttfInitialized)
        return;

    printf(TTF_TAG "Config TTF enabled: %s\n", g_remasterConfig.font.enableTTF ? CON_GREEN "yes" CON_RESET : CON_DIM "no" CON_RESET);

    if (!g_remasterConfig.font.enableTTF)
        return;

    printf(TTF_TAG CON_DIM "Acquiring ImGui IO..." CON_RESET "\n");
    ImGuiIO& io = ImGui::GetIO();

    printf(TTF_TAG CON_DIM "Font atlas: %p" CON_RESET "\n", io.Fonts);

    // Try to load custom font
    ImFontConfig config;
    config.OversampleH = 3;
    config.OversampleV = 3;
    config.PixelSnapH = false;

    // Custom glyph ranges: ASCII + Latin-1 Supplement + Latin Extended for CP850 support
    static const ImWchar glyphRanges[] = {
        0x0020, 0x00FF, // Basic Latin + Latin-1 Supplement (covers most CP850 accented chars)
        0x0131, 0x0131, // Latin Small Letter Dotless I (CP850 0xD5)
        0x0192, 0x0192, // Latin Small Letter F with Hook (CP850 0x9F)
        0x2500, 0x25A0, // Box Drawing + Block Elements (CP850 box chars)
        0,
    };
    const ImWchar* ranges = glyphRanges;

    // Rasterize the font at a high reference resolution (4K/2160p) so that
    // ImFont::Scale only ever scales DOWN, which keeps text crisp at every
    // window size.  renderTTFText() sets Scale = currentResY / refResY each frame.
    extern int outputResolution[2];
    const float kReferenceResolutionY = 2160.0f;
    float scaleFactor = kReferenceResolutionY / 200.0f;
    float scaledFontSize = g_remasterConfig.font.fontSize * scaleFactor;

    // Ensure we have a valid font size (ImGui requires > 0)
    if (scaledFontSize <= 0.0f)
    {
        printf(TTF_WARN "Invalid scaled font size %.2f, falling back to base size %d" CON_RESET "\n", 
            scaledFontSize, g_remasterConfig.font.fontSize);
        scaledFontSize = (float)g_remasterConfig.font.fontSize;
    }

    // Ensure minimum font size
    if (scaledFontSize < 8.0f)
    {
        printf(TTF_WARN "Font size %.2f too small, clamping to 8.0" CON_RESET "\n", scaledFontSize);
        scaledFontSize = 8.0f;
    }

    printf(TTF_TAG "Resolution: " CON_WHITE "%dx%d" CON_RESET "  Base size: " CON_WHITE "%d" CON_RESET "  Scale: " CON_WHITE "%.2f" CON_RESET "  Scaled: " CON_WHITE "%.1f" CON_RESET "\n", 
        outputResolution[0], outputResolution[1],
        g_remasterConfig.font.fontSize, scaleFactor, scaledFontSize);

    printf(TTF_TAG "Loading font: " CON_CYAN "%s" CON_RESET "\n", g_remasterConfig.font.fontPath);

    g_ttfFont = io.Fonts->AddFontFromFileTTF(
        g_remasterConfig.font.fontPath, 
        scaledFontSize, 
        &config, 
        ranges
    );

    if (g_ttfFont)
    {
        printf(TTF_OK CON_GREEN "Font loaded: " CON_RESET "%s " CON_DIM "(ptr=%p)" CON_RESET "\n", g_remasterConfig.font.fontPath, g_ttfFont);
    }
    else
    {
        printf(TTF_WARN "Failed to load: %s" CON_RESET "\n", g_remasterConfig.font.fontPath);
        printf(TTF_TAG "Trying system font fallbacks...\n");

        // Try common Windows system fonts as fallback
        const char* fallbackFonts[] = {
            "C:/Windows/Fonts/arial.ttf",
            "C:/Windows/Fonts/times.ttf",
            "C:/Windows/Fonts/cour.ttf"
        };

        for (const char* fallback : fallbackFonts)
        {
            printf(TTF_TAG CON_DIM "  Trying: %s" CON_RESET "\n", fallback);
            g_ttfFont = io.Fonts->AddFontFromFileTTF(fallback, scaledFontSize, &config, ranges);
            if (g_ttfFont)
            {
                printf(TTF_OK CON_GREEN "Fallback loaded: " CON_RESET "%s " CON_DIM "(ptr=%p)" CON_RESET "\n", fallback, g_ttfFont);
                break;
            }
        }

        if (!g_ttfFont)
        {
            printf(TTF_ERR "All fonts failed — TTF rendering disabled" CON_RESET "\n");
            g_remasterConfig.font.enableTTF = false;
            return;
        }
    }

    g_ttfInitialized = true;
    g_fontLoadResolutionY = kReferenceResolutionY;
    printf(TTF_OK CON_BGREEN "System ready" CON_RESET " " CON_DIM "(font=%p, refResY=%.0f)" CON_RESET "\n", g_ttfFont, g_fontLoadResolutionY);

    // Rebuild the font atlas texture since we added a new font after initial creation
    extern void imguiRebuildFontTexture();
    imguiRebuildFontTexture();
    printf(TTF_OK "Font atlas texture rebuilt\n");

    printf(TTF_TAG CON_BOLD "════════════════════════════════════════\n" CON_RESET);
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

    extern int outputResolution[2];

    // Ensure font scale matches the current resolution for correct measurement
    if (g_fontLoadResolutionY > 0.0f && outputResolution[1] > 0)
        g_ttfFont->Scale = (float)outputResolution[1] / g_fontLoadResolutionY;

    std::string utf8Str = transformTTFText(string);

    ImGui::PushFont(g_ttfFont);
    ImVec2 textSize = ImGui::CalcTextSize(utf8Str.c_str());
    ImGui::PopFont();

    // Convert from screen pixels back to game-space width (320-wide coordinates)
    float scaleX = (outputResolution[0] > 0) ? (outputResolution[0] / 320.0f) : 1.0f;
    return (int)(textSize.x / scaleX);
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
        printf(TTF_TAG CON_DIM "Menu selection changed, queue cleared" CON_RESET "\n");
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
        printf(TTF_TAG CON_DIM "State: menu %d->%d  pause %d->%d" CON_RESET "\n",
            g_wasInMenu, inMenu, g_wasPaused, isPaused);
        clearTTFTextQueue();
        g_lastTextQueueFrame = g_ttfFrameCounter;
    }

    // If we just exited menu and unpaused (back to gameplay), clear text
    if (!inMenu && !isPaused && (g_wasInMenu || g_wasPaused))
    {
        printf(TTF_TAG CON_DIM "Returned to gameplay, queue cleared" CON_RESET "\n");
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
        printf(TTF_TAG CON_DIM "Queue skip (enabled=%d init=%d font=%p)" CON_RESET "\n", 
            g_remasterConfig.font.enableTTF, g_ttfInitialized, g_ttfFont);
        return;
    }

    // If it's been more than 10 frames since text was last queued, clear old text
    // This handles menu transitions and state changes
    if (g_ttfFrameCounter > g_lastTextQueueFrame + 10)
    {
        // New batch of text - clear old queue
        clearTTFTextQueue();
        printf(TTF_TAG CON_DIM "New text batch, queue cleared (gap=%d frames)" CON_RESET "\n", 
            g_ttfFrameCounter - g_lastTextQueueFrame);
    }
    g_lastTextQueueFrame = g_ttfFrameCounter;

    // Convert CP850 to UTF-8 and apply all display substitutions
    std::string utf8Str = transformTTFText(string);

    u8* textCopy = new u8[utf8Str.size() + 1];
    memcpy(textCopy, utf8Str.c_str(), utf8Str.size() + 1);

    TTFTextCommand cmd;
    cmd.x = x;
    cmd.y = y;
    cmd.text = textCopy;
    cmd.color = color;
    cmd.shadow = shadow;
    cmd.shadowColor = shadowColor;

    g_textQueue.push_back(cmd);
    printf(TTF_TAG CON_MAGENTA "+" CON_RESET " " CON_WHITE "'%s'" CON_RESET " " CON_DIM "@ (%d,%d) col=%d [%zu]" CON_RESET "\n", 
        string, x, y, color, g_textQueue.size());

    // Detect fade-out effect: when color=31 and queue reaches 55-56 or 76 items,
    // this indicates the text is fading out and should be cleared
    size_t queueSize = g_textQueue.size();
    if (color == 31 && (queueSize == 55 || queueSize == 56 || queueSize == 60 || queueSize == 65 || queueSize == 76))
    {
        printf(TTF_TAG CON_YELLOW "Fade-out detected" CON_RESET CON_DIM " (col=%d, q=%zu), clearing" CON_RESET "\n", color, queueSize);
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


    // Detect resolution or fullscreen changes and adjust font scaling dynamically
    if (outputResolution[0] != g_lastResolutionX || outputResolution[1] != g_lastResolutionY)
    {
        printf(TTF_TAG CON_CYAN "Resolution: " CON_RESET "%dx%d -> %dx%d\n",
            g_lastResolutionX, g_lastResolutionY, outputResolution[0], outputResolution[1]);
        g_lastResolutionX = outputResolution[0];
        g_lastResolutionY = outputResolution[1];
    }

    // Adjust font scale on the fly to match current resolution.
    // The font was rasterized at g_fontLoadResolutionY; scale it so the
    // rendered glyphs match the position mapping (based on current output
    // resolution).  This keeps absolute 320x200 positions correct after
    // resize or fullscreen toggle.
    if (g_fontLoadResolutionY > 0.0f && outputResolution[1] > 0)
    {
        g_ttfFont->Scale = (float)outputResolution[1] / g_fontLoadResolutionY;
    }

    // Calculate the viewport size (full window, no menu bar)
    float viewportWidth = (float)outputResolution[0];
    float viewportHeight = (float)outputResolution[1];

    // Calculate scale factors from game coordinates (320x200) to viewport
    float scaleX = viewportWidth / 320.0f;
    float scaleY = viewportHeight / 200.0f;

    // Use foreground draw list directly - no window, no background
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    ImGui::PushFont(g_ttfFont);

    for (const auto& cmd : g_textQueue)
    {
        // Scale from game coordinates (320x200) to viewport
        float screenX = cmd.x * scaleX;
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
