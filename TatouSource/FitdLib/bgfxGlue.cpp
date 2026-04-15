///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// BGFX graphics library initialization and window management
///////////////////////////////////////////////////////////////////////////////
//Updated
#include <SDL.h>
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <bx/platform.h>
#include <backends/imgui_impl_sdl3.h>
#include "imguiBGFX.h"
#include "SDL3/SDL.h"
#include "fontTTF.h"
#include "configRemaster.h"
#include "consoleLog.h"
#include "postProcessing.h"
#include "lightProbes.h"
#include "dustParticles.h"
#include "debugger.h"
#include "osystem.h"

#if BX_PLATFORM_OSX
extern "C" {
	void* cbSetupMetalLayer(void*);
}
#elif BX_PLATFORM_WINDOWS
#include <windows.h>
#endif


SDL_Window* gWindowBGFX = nullptr;

int gFrameLimit = 60;
bool gCloseApp = false;

float gVolume = 1.f;

bool gIsFullscreen = false;
bool g_pendingFullscreenToggle = false;

// For dust particle system - we need the current floor
extern s16 g_currentFloor;

void toggleFullscreen()
{
    if (gWindowBGFX == nullptr)
        return;

    gIsFullscreen = !gIsFullscreen;

    if (gIsFullscreen)
    {
        SDL_SetWindowFullscreen(gWindowBGFX, true);
    }
    else
    {
        SDL_SetWindowFullscreen(gWindowBGFX, false);
    }
}

// Because SDL is not well defined when using cmake (it's using the old style config.h and is somewhat broken)
extern "C" {
    size_t
        wcslcpy(SDL_OUT_Z_CAP(maxlen) wchar_t* dst, const wchar_t* src, size_t maxlen)
    {
        size_t srclen = SDL_wcslen(src);
        if (maxlen > 0) {
            size_t len = SDL_min(srclen, maxlen - 1);
            SDL_memcpy(dst, src, len * sizeof(wchar_t));
            dst[len] = '\0';
        }
        return srclen;
    }

    size_t
        wcslcat(SDL_INOUT_Z_CAP(maxlen) wchar_t* dst, const wchar_t* src, size_t maxlen)
    {
        size_t dstlen = SDL_wcslen(dst);
        size_t srclen = SDL_wcslen(src);
        if (dstlen < maxlen) {
            SDL_wcslcpy(dst + dstlen, src, maxlen - dstlen);
        }
        return dstlen + srclen;
    }
}

int outputResolution[2] = { -1, -1 };

void StartFrame()
{
    int oldResolution[2];
    oldResolution[0] = outputResolution[0];
    oldResolution[1] = outputResolution[1];

    SDL_GetWindowSizeInPixels(gWindowBGFX, &outputResolution[0], &outputResolution[1]);

    if ((oldResolution[0] != outputResolution[0]) || (oldResolution[1] != outputResolution[1]))
    {
        // Guard against zero dimensions (e.g. during minimize or window transitions)
        if (outputResolution[0] > 0 && outputResolution[1] > 0)
        {
            bgfx::reset(outputResolution[0], outputResolution[1], BGFX_RESET_VSYNC);

            // Resize post-processing framebuffers (always active for scene capture)
            if (g_postProcessing)
            {
                g_postProcessing->resize(outputResolution[0], outputResolution[1]);
            }
        }
    }

    // Increment TTF frame counter once per actual frame
    incrementTTFFrameCounter();

    // Pull the input from SDL2 instead
    ImGui_ImplSDL3_NewFrame();
    imguiBeginFrame(0, 0, 0, 0, outputResolution[0], outputResolution[1], -1);

    // Menu bar is hidden for cleaner look - uncomment to show debug info
    /*
    if (ImGui::BeginMainMenuBar())
    {
        ImGui::Text(" %.2f FPS (%.2f ms)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);

        ImGui::PushItemWidth(100);
        ImGui::SliderFloat("Volume", &gVolume, 0, 1);
        ImGui::PopItemWidth();

        ImGui::EndMainMenuBar();
    }
    */

    // Always route rendering through the offscreen framebuffer so m_mainColorTex
    // contains the composited scene (needed for pause-menu preview in all modes).
    if (g_postProcessing)
    {
        // Apply effect settings when effects are enabled
        if (g_remasterConfig.postProcessing.enableBloom || g_remasterConfig.postProcessing.enableFilmGrain || g_remasterConfig.postProcessing.enableSSAO || g_remasterConfig.postProcessing.enableSSGI)
        {
            g_postProcessing->setBloomEnabled(g_remasterConfig.postProcessing.enableBloom);
            g_postProcessing->setBloomThreshold(g_remasterConfig.postProcessing.bloomThreshold);
            g_postProcessing->setBloomIntensity(g_remasterConfig.postProcessing.bloomIntensity);
            g_postProcessing->setFilmGrainEnabled(g_remasterConfig.postProcessing.enableFilmGrain);
            g_postProcessing->setFilmGrainIntensity(g_remasterConfig.postProcessing.filmGrainIntensity);
            g_postProcessing->setSSAOEnabled(g_remasterConfig.postProcessing.enableSSAO);
            g_postProcessing->setSSAORadius(g_remasterConfig.postProcessing.ssaoRadius);
            g_postProcessing->setSSAOIntensity(g_remasterConfig.postProcessing.ssaoIntensity);
            g_postProcessing->setBloomPasses(g_remasterConfig.postProcessing.bloomPasses);

            // SSGI settings
            g_postProcessing->setSSGIEnabled(g_remasterConfig.postProcessing.enableSSGI);
            g_postProcessing->setSSGIRadius(g_remasterConfig.postProcessing.ssgiRadius);
            g_postProcessing->setSSGIIntensity(g_remasterConfig.postProcessing.ssgiIntensity);
            g_postProcessing->setSSGINumSamples(g_remasterConfig.postProcessing.ssgiNumSamples);

            // Light Probe settings
            g_postProcessing->setLightProbesEnabled(g_remasterConfig.postProcessing.enableLightProbes);
            g_postProcessing->setLightProbeIntensity(g_remasterConfig.postProcessing.lightProbeIntensity);
        }

        g_postProcessing->beginScene();
    }

    // Update dust particles - enable only on floor 0 (attic)
    if (g_dustParticles)
    {
        static Uint64 lastUpdateTime = SDL_GetPerformanceCounter();
        Uint64 currentTime = SDL_GetPerformanceCounter();
        float deltaTime = (float)(currentTime - lastUpdateTime) / (float)SDL_GetPerformanceFrequency();
        lastUpdateTime = currentTime;

        // Enable dust particles only in the attic (ETAGE00)
        bool isAttic = (g_currentFloor == 0);

        // Debug: print floor change
        static s16 lastFloor = -999;
        if (lastFloor != g_currentFloor)
        {
            printf("[DUST] Floor changed to %d, dust %s\n", g_currentFloor, isAttic ? "ENABLED" : "disabled");
            lastFloor = g_currentFloor;
        }

        g_dustParticles->setEnabled(isAttic);
        g_dustParticles->update(deltaTime);
    }

    bgfx::setViewRect(0, 0, 0, outputResolution[0], outputResolution[1]);
    bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH);
    bgfx::touch(0);
}

void EndFrame()
{
#ifdef FITD_DEBUGGER
    // Debug menu toggle (` key) - only in debug builds
    if (ImGui::IsKeyPressed(ImGuiKey_GraveAccent, false))
    {
        debuggerVar_debugMenuDisplayed = !debuggerVar_debugMenuDisplayed;
    }
#endif

    // Render dust particles (attic atmosphere effect)
    if (g_dustParticles && g_dustParticles->isEnabled())
    {
        g_dustParticles->render(0);
    }

    // Render vignette overlay (darkened screen edges for cinematic look)
    osystem_drawVignette();

    // Update and render cinematic letterbox (black bars for intro scenes)
    osystem_updateLetterbox();
    osystem_drawLetterbox();

    // Render TTF text overlay before ImGui render
    renderTTFText();

    // Always call imguiEndFrame() to render ImGui windows to bgfx
    imguiEndFrame();

    // Composite the offscreen framebuffer to the backbuffer (applies PP effects if enabled,
    // otherwise does a simple pass-through). Always active for scene snapshot capture.
    if (g_postProcessing)
    {
        g_postProcessing->endScene();
    }

    // Snapshot the composited scene for the pause menu preview.
    // Must happen after endScene (so the PP chain has written the final image)
    // but before bgfx::frame() (which presents and advances the frame).
    osystem_updateSceneSnapshot();

    bgfx::frame();

    // Don't clear the text queue here - text should persist until a new batch is queued
    // The game doesn't re-draw text every frame, only when menus change
    // Clearing happens in queueTTFText() when a new batch is detected

    {
        static Uint64 last_time = SDL_GetPerformanceCounter();
        Uint64 now = SDL_GetPerformanceCounter();

        double freq = (double)SDL_GetPerformanceFrequency();
        double secs = (now - last_time) / freq;
        double timeToWait = ((1.f / gFrameLimit) - secs) * 1000;
        //timeToWait = 0;
        if (timeToWait > 0)
        {
            SDL_Delay((unsigned int)timeToWait);
        }

        last_time = SDL_GetPerformanceCounter();
    }
}

bgfx::Init initparam;

void createBgfxInitParams()
{
#if BX_PLATFORM_LINUX
    if (SDL_strcmp(SDL_GetCurrentVideoDriver(), "x11") == 0)
    {
        initparam.platformData.ndt = (void*)SDL_GetPointerProperty(SDL_GetWindowProperties(gWindowBGFX), SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
        initparam.platformData.nwh = (void*)SDL_GetNumberProperty(SDL_GetWindowProperties(gWindowBGFX), SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
    }
    else if (SDL_strcmp(SDL_GetCurrentVideoDriver(), "wayland") == 0)
    {
        initparam.platformData.ndt = (void*)SDL_GetPointerProperty(SDL_GetWindowProperties(gWindowBGFX), SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, NULL);
        initparam.platformData.nwh = (void*)SDL_GetPointerProperty(SDL_GetWindowProperties(gWindowBGFX), SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, NULL);
    }
#elif BX_PLATFORM_OSX
    initparam.platformData.ndt = NULL;
    initparam.platformData.nwh = cbSetupMetalLayer((void*)SDL_GetPointerProperty(SDL_GetWindowProperties(gWindowBGFX), SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, NULL));
#elif BX_PLATFORM_WINDOWS
    initparam.platformData.ndt = NULL;
    initparam.platformData.nwh = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(gWindowBGFX), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
#endif // BX_PLATFORM_
}

int initBgfxGlue(int argc, char* argv[])
{
    createBgfxInitParams();

    // Check for renderer command-line argument
    bool rendererSpecified = false;
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-d3d11") == 0 || strcmp(argv[i], "--d3d11") == 0)
        {
            initparam.type = bgfx::RendererType::Direct3D11;
            rendererSpecified = true;
            printf(BGFX_TAG "Forcing DirectX 11 renderer\n");
        }
        else if (strcmp(argv[i], "-d3d12") == 0 || strcmp(argv[i], "--d3d12") == 0)
        {
            initparam.type = bgfx::RendererType::Direct3D12;
            rendererSpecified = true;
            printf(BGFX_TAG "Forcing DirectX 12 renderer\n");
        }
        else if (strcmp(argv[i], "-opengl") == 0 || strcmp(argv[i], "--opengl") == 0)
        {
            initparam.type = bgfx::RendererType::OpenGL;
            rendererSpecified = true;
            printf(BGFX_TAG "Forcing OpenGL renderer\n");
        }
        else if (strcmp(argv[i], "-vulkan") == 0 || strcmp(argv[i], "--vulkan") == 0)
        {
            initparam.type = bgfx::RendererType::Vulkan;
            rendererSpecified = true;
            printf(BGFX_TAG "Forcing Vulkan renderer\n");
        }
    }

    // If no command-line override, check config file setting
    if (!rendererSpecified)
    {
        const char* backend = g_remasterConfig.graphics.rendererBackend;
        if (strcmp(backend, "d3d11") == 0)
            initparam.type = bgfx::RendererType::Direct3D11;
        else if (strcmp(backend, "d3d12") == 0)
            initparam.type = bgfx::RendererType::Direct3D12;
        else if (strcmp(backend, "opengl") == 0)
            initparam.type = bgfx::RendererType::OpenGL;
        else if (strcmp(backend, "vulkan") == 0)
            initparam.type = bgfx::RendererType::Vulkan;
        else if (strcmp(backend, "metal") == 0)
            initparam.type = bgfx::RendererType::Metal;
        else
            initparam.type = bgfx::RendererType::Direct3D11; // default fallback

        printf(BGFX_TAG "Using %s renderer (from config)\n", backend);
    }

    // ReShade compatibility settings
    // Set resolution explicitly to match window size
    int windowWidth, windowHeight;
    SDL_GetWindowSizeInPixels(gWindowBGFX, &windowWidth, &windowHeight);
    initparam.resolution.width = windowWidth;
    initparam.resolution.height = windowHeight;
    initparam.resolution.reset = BGFX_RESET_VSYNC; // Enable VSync for stability

    // Set buffer sizes for stable rendering
    initparam.limits.transientVbSize = 6 << 20; // 6MB
    initparam.limits.transientIbSize = 2 << 20; // 2MB

    // Alternative options (uncomment to test):
    //initparam.type = bgfx::RendererType::OpenGL;
    //initparam.type = bgfx::RendererType::Vulkan;
    //initparam.type = bgfx::RendererType::Direct3D12;
#if BX_CONFIG_DEBUG
    initparam.debug = true;
#endif

    printf(BGFX_TAG "Initializing BGFX with resolution: %dx%d\n", windowWidth, windowHeight);

    bgfx::init(initparam);

    // Print actual renderer type for debugging
    const char* rendererName = bgfx::getRendererName(bgfx::getRendererType());
    printf(BGFX_OK "BGFX initialized successfully with renderer: %s\n", rendererName);

    // Small delay to allow ReShade hooks to stabilize (helps prevent crashes)
    SDL_Delay(100);

    imguiCreate();

    ImGuiIO& io = ImGui::GetIO();
    //io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    if ((bgfx::getRendererType() == bgfx::RendererType::Direct3D11) || (bgfx::getRendererType() == bgfx::RendererType::Direct3D12)) {
        ImGui_ImplSDL3_InitForD3D(gWindowBGFX);
    }
    else
    if (bgfx::getRendererType() == bgfx::RendererType::Metal) {
        ImGui_ImplSDL3_InitForMetal(gWindowBGFX);
    }
    else
    if (bgfx::getRendererType() == bgfx::RendererType::OpenGL) {
        ImGui_ImplSDL3_InitForOpenGL(gWindowBGFX, SDL_GL_GetCurrentContext());
    }
    else
    if (bgfx::getRendererType() == bgfx::RendererType::Vulkan) {
        ImGui_ImplSDL3_InitForVulkan(gWindowBGFX);
    }
    else {
        ImGui_ImplSDL3_InitForOther(gWindowBGFX);
    }

    // Initialize TTF font system
    // Note: This adds fonts after imguiCreate() has already built the font atlas.
    // The atlas will need to be rebuilt, which happens automatically on first ImGui_ImplXXX_NewFrame()
    initTTFFont();

    // Initialize post-processing system (framebuffers will be created, but effects disabled by default)
    // Sync outputResolution with the initial window size so the first
    // StartFrame() does not trigger a redundant bgfx::reset().
    outputResolution[0] = windowWidth;
    outputResolution[1] = windowHeight;

    g_postProcessing = new PostProcessing();
    g_postProcessing->init(windowWidth, windowHeight);

    printf(BGFX_OK "Post-processing system initialized (effects will activate when HD backgrounds are enabled)\n");

    // Initialize light probe system
    g_lightProbeManager = new LightProbeManager();
    g_lightProbeManager->init();

    printf(BGFX_OK "Light Probe system initialized\n");

    // Initialize dust particle system
    g_dustParticles = new DustParticleSystem();
    g_dustParticles->init();

    printf(BGFX_OK "Dust particle system initialized\n");

    return true;
}

void deleteBgfxGlue()
{
    // Shutdown dust particle system
    if (g_dustParticles)
    {
        delete g_dustParticles;
        g_dustParticles = nullptr;
    }

    // Shutdown light probe system
    if (g_lightProbeManager)
    {
        delete g_lightProbeManager;
        g_lightProbeManager = nullptr;
    }

    // Shutdown post-processing system
    if (g_postProcessing)
    {
        delete g_postProcessing;
        g_postProcessing = nullptr;
    }

    shutdownTTFFont();
    imguiDestroy();
}
