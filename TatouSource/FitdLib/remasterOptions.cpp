///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Complete in-game remaster configuration dialog (F1)
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "remasterOptions.h"
#include "configRemaster.h"
#include "controlsMenu.h"
#include "input.h"
#include "bgfxGlue.h"

#include <imgui.h>
#include <atomic>
#include <cstring>

static std::atomic_bool s_optionsOpen(false);
static std::atomic_bool s_startupGate(false);
static double s_statusUntil = 0.0;
static const char* s_statusText = nullptr;

bool remasterOptionsIsOpen()
{
    return s_optionsOpen.load(std::memory_order_acquire);
}

void remasterOptionsToggle()
{
    if (remasterOptionsIsOpen())
    {
        remasterOptionsClose();
    }
    else
    {
        s_startupGate.store(false, std::memory_order_release);
        s_optionsOpen.store(true, std::memory_order_release);
    }
}

void remasterOptionsClose()
{
    s_optionsOpen.store(false, std::memory_order_release);
    s_startupGate.store(false, std::memory_order_release);
}

void remasterOptionsBeginStartupGate()
{
    s_startupGate.store(true, std::memory_order_release);
    s_optionsOpen.store(true, std::memory_order_release);
}

bool remasterOptionsIsStartupGate()
{
    return s_startupGate.load(std::memory_order_acquire);
}

static void syncRuntimeInput()
{
    g_controllerConfig.analogDeadzone = g_remasterConfig.controller.analogDeadzone;
    g_controllerConfig.analogSensitivity = g_remasterConfig.controller.analogSensitivity;
    g_controllerConfig.invertYAxis = g_remasterConfig.controller.invertYAxis;
    g_controllerConfig.analogMovement = g_remasterConfig.controller.analogMovement;

    for (int i = 0; i < ACTION_COUNT; ++i)
    {
        setKeyboardBinding((KeyAction)i, (SDL_Scancode)g_remasterConfig.controls.keyBindings[i]);
        setGamepadBinding((KeyAction)i, (SDL_GamepadButton)g_remasterConfig.controls.gamepadBindings[i]);
    }
}

static void showStatus(const char* text)
{
    s_statusText = text;
    s_statusUntil = ImGui::GetTime() + 2.5;
}

static void helpMarker(const char* text)
{
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30.0f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

static bool rendererCombo()
{
    static const char* values[] = { "auto", "d3d11", "d3d12", "opengl", "vulkan", "metal" };
    bool changed = false;
    if (ImGui::BeginCombo("Renderer backend", g_remasterConfig.graphics.rendererBackend))
    {
        for (const char* value : values)
        {
            const bool selected = strcmp(g_remasterConfig.graphics.rendererBackend, value) == 0;
            if (ImGui::Selectable(value, selected))
            {
                snprintf(g_remasterConfig.graphics.rendererBackend,
                    sizeof(g_remasterConfig.graphics.rendererBackend), "%s", value);
                changed = true;
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    return changed;
}

static bool intChoice(const char* label, int* value, const int* values, int count, const char* suffix = "")
{
    char preview[32];
    snprintf(preview, sizeof(preview), "%d%s", *value, suffix);
    bool changed = false;
    if (ImGui::BeginCombo(label, preview))
    {
        for (int i = 0; i < count; ++i)
        {
            char item[32];
            snprintf(item, sizeof(item), "%d%s", values[i], suffix);
            const bool selected = *value == values[i];
            if (ImGui::Selectable(item, selected))
            {
                *value = values[i];
                changed = true;
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    return changed;
}

static void drawGraphicsTab()
{
    ImGui::TextDisabled("Backgrounds and renderer");
    ImGui::Separator();
    ImGui::Checkbox("HD backgrounds", &g_remasterConfig.graphics.enableHDBackgrounds);
    const int scales[] = { 1, 2, 3, 4 };
    intChoice("Background scale", &g_remasterConfig.graphics.backgroundScale, scales, 4, "x");
    ImGui::Checkbox("Texture filtering", &g_remasterConfig.graphics.enableFiltering);
    ImGui::Checkbox("Wall depth for ambient occlusion", &g_remasterConfig.graphics.enableWallDepth);
    helpMarker("Adds collision-wall depth to the scene so SSAO can darken architectural edges.");
    ImGui::Spacing();

    ImGui::Checkbox("Blurred menu background", &g_remasterConfig.graphics.enableBlurredMenu);
    ImGui::BeginDisabled(!g_remasterConfig.graphics.enableBlurredMenu);
    ImGui::SliderFloat("Menu blur amount", &g_remasterConfig.graphics.menuBlurAmount, 0.0f, 20.0f, "%.1f");
    ImGui::EndDisabled();
    ImGui::Checkbox("Gameplay hints", &g_remasterConfig.graphics.enableHints);
    ImGui::Checkbox("Menu artwork", &g_remasterConfig.graphics.enableArtwork);

    ImGui::Spacing();
    rendererCombo();
    helpMarker("Renderer changes take effect after restarting the game.");
    const int msaa[] = { 0, 2, 4, 8, 16 };
    intChoice("MSAA", &g_remasterConfig.graphics.msaaLevel, msaa, 5, "x");
    helpMarker("MSAA is applied when the renderer is recreated or the window changes size.");

    bool fullscreen = gIsFullscreen;
    if (ImGui::Checkbox("Fullscreen", &fullscreen))
    {
        g_remasterConfig.graphics.fullscreen = fullscreen;
        g_pendingFullscreenToggle = true;
    }
}

static void drawEffectsTab()
{
    ImGui::TextDisabled("Lighting and post-processing");
    ImGui::Separator();

    ImGui::Checkbox("Bloom", &g_remasterConfig.postProcessing.enableBloom);
    ImGui::BeginDisabled(!g_remasterConfig.postProcessing.enableBloom);
    ImGui::SliderFloat("Bloom threshold", &g_remasterConfig.postProcessing.bloomThreshold, 0.05f, 1.5f, "%.2f");
    ImGui::SliderFloat("Bloom intensity", &g_remasterConfig.postProcessing.bloomIntensity, 0.0f, 2.0f, "%.2f");
    const int passes[] = { 1, 2, 3, 4 };
    intChoice("Bloom passes", &g_remasterConfig.postProcessing.bloomPasses, passes, 4);
    ImGui::EndDisabled();

    ImGui::Checkbox("Film grain", &g_remasterConfig.postProcessing.enableFilmGrain);
    ImGui::BeginDisabled(!g_remasterConfig.postProcessing.enableFilmGrain);
    ImGui::SliderFloat("Grain intensity", &g_remasterConfig.postProcessing.filmGrainIntensity, 0.0f, 0.15f, "%.3f");
    ImGui::EndDisabled();

    ImGui::Checkbox("SSAO", &g_remasterConfig.postProcessing.enableSSAO);
    ImGui::BeginDisabled(!g_remasterConfig.postProcessing.enableSSAO);
    ImGui::SliderFloat("SSAO radius", &g_remasterConfig.postProcessing.ssaoRadius, 10.0f, 1500.0f, "%.0f");
    ImGui::SliderFloat("SSAO intensity", &g_remasterConfig.postProcessing.ssaoIntensity, 0.0f, 3.0f, "%.2f");
    ImGui::EndDisabled();

    ImGui::Checkbox("Vignette", &g_remasterConfig.postProcessing.enableVignette);
    ImGui::BeginDisabled(!g_remasterConfig.postProcessing.enableVignette);
    ImGui::SliderFloat("Vignette intensity", &g_remasterConfig.postProcessing.vignetteIntensity, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Vignette radius", &g_remasterConfig.postProcessing.vignetteRadius, 0.1f, 0.95f, "%.2f");
    ImGui::EndDisabled();

    ImGui::Checkbox("Screen-space global illumination", &g_remasterConfig.postProcessing.enableSSGI);
    ImGui::BeginDisabled(!g_remasterConfig.postProcessing.enableSSGI);
    ImGui::SliderFloat("SSGI radius", &g_remasterConfig.postProcessing.ssgiRadius, 10.0f, 1200.0f, "%.0f");
    ImGui::SliderFloat("SSGI intensity", &g_remasterConfig.postProcessing.ssgiIntensity, 0.0f, 4.0f, "%.2f");
    const int samples[] = { 8, 12, 16, 24, 32 };
    intChoice("SSGI samples", &g_remasterConfig.postProcessing.ssgiNumSamples, samples, 5);
    ImGui::EndDisabled();

    ImGui::Checkbox("Light probes", &g_remasterConfig.postProcessing.enableLightProbes);
    ImGui::BeginDisabled(!g_remasterConfig.postProcessing.enableLightProbes);
    ImGui::SliderFloat("Light probe intensity", &g_remasterConfig.postProcessing.lightProbeIntensity, 0.0f, 4.0f, "%.2f");
    ImGui::EndDisabled();
}

static void drawColorTab()
{
    ImGui::TextDisabled("Cinematic image finishing");
    ImGui::Separator();
    ImGui::Checkbox("Color grading", &g_remasterConfig.postProcessing.enableColorGrading);
    ImGui::BeginDisabled(!g_remasterConfig.postProcessing.enableColorGrading);
    ImGui::SliderFloat("Exposure", &g_remasterConfig.postProcessing.exposure, -2.0f, 2.0f, "%+.2f EV");
    ImGui::SliderFloat("Contrast", &g_remasterConfig.postProcessing.contrast, 0.5f, 1.5f, "%.2f");
    ImGui::SliderFloat("Saturation", &g_remasterConfig.postProcessing.saturation, 0.0f, 2.0f, "%.2f");
    ImGui::SliderFloat("Temperature", &g_remasterConfig.postProcessing.temperature, -1.0f, 1.0f, "%+.2f");
    ImGui::SliderFloat("Shadow lift", &g_remasterConfig.postProcessing.shadowLift, 0.0f, 0.2f, "%.3f");
    ImGui::SliderFloat("Highlight rolloff", &g_remasterConfig.postProcessing.highlightRolloff, 0.0f, 1.0f, "%.2f");
    ImGui::EndDisabled();

    ImGui::Spacing();
    ImGui::TextDisabled("Animation presentation");
    ImGui::Separator();
    ImGui::Checkbox("Smooth skeletal poses", &g_remasterConfig.animation.enablePoseSmoothing);
    ImGui::BeginDisabled(!g_remasterConfig.animation.enablePoseSmoothing);
    ImGui::SliderFloat("Pose smoothing strength", &g_remasterConfig.animation.poseSmoothingStrength, 0.0f, 1.0f, "%.2f");
    ImGui::EndDisabled();
    helpMarker("Only rendered joint poses are eased. Root motion, collision and hit timing remain unchanged.");
}

static void drawControllerTab()
{
    ImGui::TextDisabled("Controller behavior");
    ImGui::Separator();
    ImGui::Checkbox("Enable controller", &g_remasterConfig.controller.enableController);
    ImGui::BeginDisabled(!g_remasterConfig.controller.enableController);
    ImGui::SliderFloat("Analog deadzone", &g_remasterConfig.controller.analogDeadzone, 0.0f, 0.5f, "%.2f");
    ImGui::SliderFloat("Analog sensitivity", &g_remasterConfig.controller.analogSensitivity, 0.1f, 3.0f, "%.2f");
    ImGui::Checkbox("Invert Y axis", &g_remasterConfig.controller.invertYAxis);
    ImGui::Checkbox("Analog movement", &g_remasterConfig.controller.analogMovement);
    ImGui::EndDisabled();
    syncRuntimeInput();

    ImGui::Spacing();
    ImGui::TextDisabled("Bindings");
    ImGui::Separator();
    static const SDL_Scancode commonKeys[] = {
        SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT,
        SDL_SCANCODE_W, SDL_SCANCODE_A, SDL_SCANCODE_S, SDL_SCANCODE_D,
        SDL_SCANCODE_Q, SDL_SCANCODE_E, SDL_SCANCODE_SPACE, SDL_SCANCODE_RETURN,
        SDL_SCANCODE_ESCAPE, SDL_SCANCODE_LSHIFT, SDL_SCANCODE_RSHIFT,
        SDL_SCANCODE_LCTRL, SDL_SCANCODE_RCTRL, SDL_SCANCODE_TAB
    };

    if (ImGui::BeginTable("bindings", 3, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Action");
        ImGui::TableSetupColumn("Keyboard");
        ImGui::TableSetupColumn("Gamepad");
        ImGui::TableHeadersRow();
        for (int action = 0; action < ACTION_COUNT; ++action)
        {
            ImGui::PushID(action);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(getActionName((KeyAction)action));

            ImGui::TableSetColumnIndex(1);
            SDL_Scancode currentKey = (SDL_Scancode)g_remasterConfig.controls.keyBindings[action];
            const char* keyName = SDL_GetScancodeName(currentKey);
            if (!keyName || !*keyName) keyName = "Unbound";
            if (ImGui::BeginCombo("##key", keyName))
            {
                for (SDL_Scancode candidate : commonKeys)
                {
                    const bool selected = currentKey == candidate;
                    if (ImGui::Selectable(SDL_GetScancodeName(candidate), selected))
                        g_remasterConfig.controls.keyBindings[action] = (int)candidate;
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::TableSetColumnIndex(2);
            int currentButton = g_remasterConfig.controls.gamepadBindings[action];
            const char* buttonName = (currentButton >= 0 && currentButton < SDL_GAMEPAD_BUTTON_COUNT)
                ? getGamepadButtonName((SDL_GamepadButton)currentButton) : "Unbound";
            if (ImGui::BeginCombo("##pad", buttonName))
            {
                for (int button = 0; button < SDL_GAMEPAD_BUTTON_COUNT; ++button)
                {
                    const bool selected = currentButton == button;
                    char selectableName[64];
                    snprintf(selectableName, sizeof(selectableName), "%s##padbutton%d",
                        getGamepadButtonName((SDL_GamepadButton)button), button);
                    if (ImGui::Selectable(selectableName, selected))
                        g_remasterConfig.controls.gamepadBindings[action] = button;
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}

static void drawInterfaceAudioTab()
{
    ImGui::TextDisabled("Typography");
    ImGui::Separator();
    ImGui::Checkbox("TTF fonts", &g_remasterConfig.font.enableTTF);
    ImGui::BeginDisabled(!g_remasterConfig.font.enableTTF);
    ImGui::InputText("Font path", g_remasterConfig.font.fontPath, sizeof(g_remasterConfig.font.fontPath));
    ImGui::SliderInt("Font size", &g_remasterConfig.font.fontSize, 8, 48);
    ImGui::Checkbox("Hide original bitmap text", &g_remasterConfig.font.hideOriginalText);
    ImGui::EndDisabled();
    helpMarker("Font changes are fully applied after restarting the game.");

    ImGui::Spacing();
    ImGui::TextDisabled("Music");
    ImGui::Separator();
    ImGui::Checkbox("External music", &g_remasterConfig.music.enableExternalMusic);
    ImGui::BeginDisabled(!g_remasterConfig.music.enableExternalMusic);
    ImGui::InputText("Music folder", g_remasterConfig.music.musicFolder, sizeof(g_remasterConfig.music.musicFolder));
    ImGui::EndDisabled();
    helpMarker("External music source changes take effect when a track is next loaded or after restart.");
}

static void drawContentTab()
{
    ImGui::TextDisabled("Replacement content and development pipelines");
    ImGui::Separator();
    ImGui::Checkbox("Load edited HD masks", &g_remasterConfig.masks.loadEnabled);
    ImGui::Checkbox("Dump generated masks to PNG", &g_remasterConfig.masks.dumpEnabled);
    ImGui::Checkbox("Load HD sequence frames", &g_remasterConfig.sequences.loadEnabled);
    ImGui::Checkbox("Dump decoded sequence frames", &g_remasterConfig.sequences.dumpEnabled);
    ImGui::Checkbox("Dump original backgrounds", &g_remasterConfig.backgrounds.dumpEnabled);

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(1.0f, 0.73f, 0.25f, 1.0f), "Dump options can create many files and are intended for content authors.");
}

static void drawAdvancedTab()
{
    ImGui::TextDisabled("Game data");
    ImGui::Separator();
    ImGui::Checkbox("Steamless mode", &g_remasterConfig.gameData.steamless);
    helpMarker("Disables automatic game-data copying and Steam overlay integration after restart.");
    ImGui::Checkbox("Jack in the Dark mode", &g_remasterConfig.gameData.jackMode);
    helpMarker("Selects the JACK data set. Restart required.");

    ImGui::Spacing();
    ImGui::TextDisabled("Diagnostics");
    ImGui::Separator();
    ImGui::Checkbox("Graphics API validation", &g_remasterConfig.debug.enableGraphicsValidation);
    helpMarker("Enables Direct3D/Vulkan validation after restart. DirectX may raise first-chance 0x87A exceptions in a debugger when it detects an invalid GPU call.");
    ImGui::Checkbox("Log LIFE script dispatch", &g_remasterConfig.debug.logLifeScripts);
    ImGui::Checkbox("Dump LIFE scripts on startup", &g_remasterConfig.debug.dumpLifeScripts);
    ImGui::Checkbox("Generate native LIFE scripts", &g_remasterConfig.debug.generateNativeLifeScripts);
    ImGui::Checkbox("Enable native LIFE scripts", &g_remasterConfig.debug.enableNativeLifeScripts);
    ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.35f, 1.0f), "Diagnostic options may affect performance and require a restart.");
}

void remasterOptionsDraw()
{
    if (!remasterOptionsIsOpen())
        return;

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
        ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2((io.DisplaySize.x < 900.0f) ? io.DisplaySize.x * 0.94f : 880.0f,
        (io.DisplaySize.y < 720.0f) ? io.DisplaySize.y * 0.92f : 680.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(ImVec2(620.0f, 430.0f), ImVec2(io.DisplaySize.x * 0.98f, io.DisplaySize.y * 0.98f));

    bool open = true;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 14.0f));
    ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.12f, 0.055f, 0.045f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.28f, 0.075f, 0.045f, 1.0f));

    if (ImGui::Begin("Re-Haunted Remaster Options###ReHauntedOptions", &open,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings))
    {
        if (remasterOptionsIsStartupGate())
            ImGui::TextDisabled("Configure Re-Haunted before continuing  |  Changes preview live where supported");
        else
            ImGui::TextDisabled("Home or F1 toggles this dialog  |  Changes preview live where supported");
        ImGui::Spacing();

        const bool startupGate = remasterOptionsIsStartupGate();
        const float footerHeight = ImGui::GetFrameHeightWithSpacing() * (startupGate ? 3.2f : 2.2f);
        if (ImGui::BeginChild("##optionPages", ImVec2(0.0f, -footerHeight), false))
        {
            if (ImGui::BeginTabBar("##remasterTabs", ImGuiTabBarFlags_FittingPolicyScroll))
            {
                if (ImGui::BeginTabItem("Graphics")) { drawGraphicsTab(); ImGui::EndTabItem(); }
                if (ImGui::BeginTabItem("Effects")) { drawEffectsTab(); ImGui::EndTabItem(); }
                if (ImGui::BeginTabItem("Color & Motion")) { drawColorTab(); ImGui::EndTabItem(); }
                if (ImGui::BeginTabItem("Controls")) { drawControllerTab(); ImGui::EndTabItem(); }
                if (ImGui::BeginTabItem("UI & Audio")) { drawInterfaceAudioTab(); ImGui::EndTabItem(); }
                if (ImGui::BeginTabItem("Content")) { drawContentTab(); ImGui::EndTabItem(); }
                if (ImGui::BeginTabItem("Advanced")) { drawAdvancedTab(); ImGui::EndTabItem(); }
                ImGui::EndTabBar();
            }
        }
        ImGui::EndChild();

        ImGui::Separator();
        if (startupGate)
        {
            bool doNotShowAgain = !g_remasterConfig.ui.showOptionsAtStartup;
            if (ImGui::Checkbox("Don't show this dialog again at startup", &doNotShowAgain))
                g_remasterConfig.ui.showOptionsAtStartup = !doNotShowAgain;
            ImGui::SameLine();
            ImGui::TextDisabled("(Home or F1 will still open it)");
        }

        const char* saveLabel = startupGate ? "Save & Continue" : "Save settings";
        if (ImGui::Button(saveLabel, ImVec2(startupGate ? 145.0f : 130.0f, 0.0f)))
        {
            g_remasterConfig.graphics.fullscreen = gIsFullscreen;
            syncRuntimeInput();
            saveRemasterConfig();
            showStatus("Settings saved to aitd_remaster.cfg");
            if (startupGate)
                open = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Reload saved", ImVec2(120.0f, 0.0f)))
        {
            loadRemasterConfig();
            syncRuntimeInput();
            showStatus("Saved settings restored");
        }
        ImGui::SameLine();
        if (ImGui::Button("Restore defaults", ImVec2(130.0f, 0.0f)))
        {
            initDefaultRemasterConfig();
            syncRuntimeInput();
            showStatus("Defaults restored (not saved yet)");
        }
        ImGui::SameLine();
        if (ImGui::Button(startupGate ? "Continue" : "Close", ImVec2(90.0f, 0.0f)))
            open = false;

        if (s_statusText && ImGui::GetTime() < s_statusUntil)
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.48f, 0.88f, 0.58f, 1.0f), "%s", s_statusText);
        }
    }
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);

    if (!open)
        remasterOptionsClose();
}
