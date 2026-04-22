#include "menuFade.h"
#include "imguiBGFX.h"
#include "postProcessing.h"
#include <bgfx/bgfx.h>
#include <imgui.h>
#include <SDL.h>

// Distinct from kViewSnapshot=206 in rendererBGFX.cpp.
static const bgfx::ViewId kViewFadeCapture = 207;

static bgfx::TextureHandle s_fadeCaptureTex = BGFX_INVALID_HANDLE;
static int  s_fadeTexW = 0;
static int  s_fadeTexH = 0;
static bool s_active = false;
static Uint64 s_startMs = 0;
static float  s_durationMs = 350.0f;

static void ensureCaptureTexture(int w, int h)
{
    if (bgfx::isValid(s_fadeCaptureTex) && w == s_fadeTexW && h == s_fadeTexH)
        return;

    if (bgfx::isValid(s_fadeCaptureTex))
        bgfx::destroy(s_fadeCaptureTex);

    s_fadeCaptureTex = bgfx::createTexture2D(
        (uint16_t)w, (uint16_t)h, false, 1,
        bgfx::TextureFormat::RGBA8,
        BGFX_TEXTURE_BLIT_DST | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);

    s_fadeTexW = w;
    s_fadeTexH = h;
}

// Continuously mirror the latest fully-rendered frame into our capture texture
// while the fade is idle. When menuFadeStart() flips s_active to true, the
// mirror stops and the capture texture remains frozen on whatever the last
// rendered frame was -- giving us a clean "previous frame" to fade out from.
static void mirrorScene()
{
    if (!g_postProcessing)
        return;

    bgfx::TextureHandle src = g_postProcessing->getMainColorTexture();
    if (!bgfx::isValid(src))
        return;

    int w = g_postProcessing->getWidth();
    int h = g_postProcessing->getHeight();
    if (w <= 0 || h <= 0)
        return;

    ensureCaptureTexture(w, h);
    if (!bgfx::isValid(s_fadeCaptureTex))
        return;

    bgfx::setViewName(kViewFadeCapture, "MenuFadeCapture");
    bgfx::setViewRect(kViewFadeCapture, 0, 0, (uint16_t)w, (uint16_t)h);
    bgfx::touch(kViewFadeCapture);
    bgfx::blit(kViewFadeCapture, s_fadeCaptureTex, 0, 0, src);
}

void menuFadeStart(float durationSeconds)
{
    // Do NOT blit here: the capture texture is already kept up-to-date by
    // mirrorScene() running each frame from menuFadeUpdateAndRender(). Just
    // freeze it by stopping the mirror (s_active = true) and start the timer.
    if (!bgfx::isValid(s_fadeCaptureTex))
        return;

    s_startMs = SDL_GetTicks();
    s_durationMs = durationSeconds * 1000.0f;
    if (s_durationMs < 1.0f)
        s_durationMs = 1.0f;
    s_active = true;
}

void menuFadeCancel()
{
    s_active = false;
}

bool menuFadeIsActive()
{
    return s_active;
}

void menuFadeUpdateAndRender()
{
    // While idle, keep our capture texture mirroring the latest rendered frame so
    // a future menuFadeStart() has something to fade from.
    if (!s_active)
    {
        mirrorScene();
        return;
    }

    if (!bgfx::isValid(s_fadeCaptureTex))
        return;

    float t = (float)(SDL_GetTicks() - s_startMs) / s_durationMs;
    if (t >= 1.0f)
    {
        s_active = false;
        return;
    }

    // Smoothstep ease-out: alpha = 1 - smoothstep(t).
    float smooth = 1.0f - (t * t * (3.0f - 2.0f * t));
    int alpha = (int)(smooth * 255.0f);
    if (alpha <= 0)
    {
        s_active = false;
        return;
    }

    ImVec2 sz = ImGui::GetIO().DisplaySize;
    ImU32 col = IM_COL32(255, 255, 255, alpha);
    ImGui::GetForegroundDrawList()->AddImage(
        ImGui::toId(s_fadeCaptureTex, IMGUI_FLAGS_ALPHA_BLEND, 0),
        ImVec2(0.0f, 0.0f), sz,
        ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
        col);
}

void menuFadeShutdown()
{
    if (bgfx::isValid(s_fadeCaptureTex))
    {
        bgfx::destroy(s_fadeCaptureTex);
        s_fadeCaptureTex = BGFX_INVALID_HANDLE;
    }
    s_active = false;
}
