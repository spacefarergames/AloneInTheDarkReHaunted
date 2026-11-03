#include <soloud.h>
#include "common.h"
#include "processExit.h"
#include "unityHost.h"
#include "unityGpuBridge.h"
#include "filePaths.h"
#include "fontTTF.h"
#include "bgfxGlue.h"
#include "osystemAL.h"
#include "jobSystemInit.h"
#include "configRemaster.h"
#include "remasterOptions.h"
#include "controlsMenu.h"
#include "embedded/embeddedData.h"
#include <SDL.h>
#include <backends/imgui_impl_sdl3.h>
#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <thread>
#include <vector>
#include <array>
#include <algorithm>
extern SoLoud::Soloud* gSoloud;

#define API FITD_UNITY_API
extern "C" { char homePath[512] = ""; int FitdMain(int, char**); }
void detectGame();
void deleteBgfxGlue();
void osystemAL_deinit();

namespace {
std::atomic<int> state{0}; // 0 not started, 1 running, 2 paused, 3 stopped, -1 failed
std::atomic<bool> stopping{false};
std::atomic<bool> toggleOptions{false};
std::mutex stopMutex;
std::mutex controlMutex, frameMutex, inputMutex, errorMutex;
std::mutex audioMutex;
bool audioReady = false;
unsigned int sampleRate = 48000;
std::atomic<uint32_t> renderSize{960u | (600u << 16)};
std::atomic<bool> managedStartup{false};
std::condition_variable resume;
std::thread worker;
bool graphicsReady = false;
std::atomic<bool> frameClockReset{true};
std::atomic<int> consumedJoy{0},consumedKey{0},consumedClick{0},consumedPad{0},inputSequence{0};
std::vector<uint8_t> pixels;
uint64_t sequence = 0;
int width = 0, height = 0;
char error[1024] = "";
struct Input { int joy = 0, key = 0, click = 0; float x = -1, y = -1; int buttons = 0; } input;
std::array<uint8_t,512> incomingKeys{};
std::array<bool,512> keyboard{};
struct Pad {bool connected=false;uint32_t buttons=0;std::array<float,6> axes{};} incomingPad,currentPad;
std::atomic<int> sceneGame{0},sceneFloor{-1},sceneCamera{-1},sceneActive{0};

void fail(const char* message) {
    std::lock_guard<std::mutex> lock(errorMutex);
    snprintf(error, sizeof(error), "%s", message);
    state = -1;
}

struct Callbacks final : bgfx::CallbackI {
    void fatal(const char*, uint16_t, bgfx::Fatal::Enum, const char* message) override {
        // bgfx calls this on its render thread: exceptions cannot cross that thread.
        fail(message);
        { std::lock_guard<std::mutex> lock(controlMutex); stopping = true; }
        resume.notify_all();
    }
    void traceVargs(const char*, uint16_t, const char* format, va_list args) override { vfprintf(stderr, format, args); }
    void profilerBegin(const char*, uint32_t, const char*, uint16_t) override {}
    void profilerBeginLiteral(const char*, uint32_t, const char*, uint16_t) override {}
    void profilerEnd() override {}
    uint32_t cacheReadSize(uint64_t) override { return 0; }
    bool cacheRead(uint64_t, void*, uint32_t) override { return false; }
    void cacheWrite(uint64_t, const void*, uint32_t) override {}
    void screenShot(const char*, uint32_t w, uint32_t h, uint32_t pitch, const void* data, uint32_t size, bool yflip) override {
        if (!data || !w || !h || w > 4096 || h > 4096 || pitch < w * 4 || uint64_t(pitch) * h > size) return;
        std::lock_guard<std::mutex> lock(frameMutex);
        pixels.resize(size_t(w) * h * 4);
        const auto* source = static_cast<const uint8_t*>(data);
        // Unity texture memory starts at bottom-left. bgfx screenshots are BGRA.
        for (uint32_t y = 0; y < h; ++y) {
            const auto* row = source + size_t(yflip ? y : h - 1 - y) * pitch;
            auto* dest = pixels.data() + size_t(y) * w * 4;
            for (uint32_t x = 0; x < w; ++x) {
                dest[x*4] = row[x*4+2]; dest[x*4+1] = row[x*4+1];
                dest[x*4+2] = row[x*4]; dest[x*4+3] = 255;
            }
        }
        width = int(w); height = int(h); ++sequence;
    }
    void captureBegin(uint32_t, uint32_t, uint32_t, bgfx::TextureFormat::Enum, bool) override {}
    void captureEnd() override {}
    void captureFrame(const void*, uint32_t) override {}
} callbacks;

void run(int w, int h) {
    bool audio = false, jobs = false;
    try {
#ifdef FITD_UNITY_COMMANDS
        // Unity owns events. SDL's clock/delay functions need no video/activity
        // initialization, including on platforms without an SDL application.
#else
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) throw std::runtime_error(SDL_GetError());
        gWindowBGFX = SDL_CreateWindow("FITD Unity render surface", w, h, SDL_WINDOW_HIDDEN);
        if (!gWindowBGFX) throw std::runtime_error(SDL_GetError());
#endif
        loadRemasterConfig();
        if (!fileExists("LISTBOD2.PAK") && !fileExists("MER.PAK") && !fileExists("AN1.PAK") && !fileExists("PERE.PAK"))
            throw std::runtime_error("No supported game data found in the configured Unity content cache.");
        // Embedded mode never opens Steam, installer dialogs, or a console window.
        g_remasterConfig.gameData.steamless = true;
        #ifdef FITD_UNITY_COMMANDS
        // Leave CPU capacity for Unity's renderer, jobs, and audio thread.
        JobSystem::instance().init(2,(std::max)(1,(std::min)(4,int(std::thread::hardware_concurrency())-2)));
#else
        initJobSystem();
#endif
        jobs = true;
        { std::lock_guard<std::mutex> lock(audioMutex); osystemAL_init(); audio = true; audioReady = true; }
        detectGame();
        FitdMain(0, nullptr);
    } catch (const FitdHostExit& e) {
        if (e.code) fail("The game stopped after a native engine error; see native log output.");
    } catch (const std::exception& e) { fail(e.what()); }
    catch (...) { fail("Unexpected native gameplay exception."); }
    // getCaps is valid only after bgfx initialization; callback installation is
    // recorded by the host hook below instead of probing uninitialized bgfx.
    if (jobs) shutdownJobSystem();
    if (audio) { std::lock_guard<std::mutex> lock(audioMutex); audioReady = false; osystemAL_deinit(); }
    if (graphicsReady) {
#ifndef FITD_UNITY_COMMANDS
        ImGui_ImplSDL3_Shutdown();
#endif
        deleteBgfxGlue();
        bgfx::shutdown();
        graphicsReady = false;
    }
    if (gWindowBGFX) { SDL_DestroyWindow(gWindowBGFX); gWindowBGFX = nullptr; }
#ifndef FITD_UNITY_COMMANDS
    SDL_Quit();
#endif
    if (state != -1) state = 3;
}
}

bgfx::CallbackI* unityBgfxCallback() { return &callbacks; }
void unityBgfxReady() { graphicsReady = true; }
uint32_t unityRenderSize() { return renderSize.load(); }
int unityRenderWidth() { return unityRenderSize() & 65535; }
int unityRenderHeight() { return unityRenderSize() >> 16; }
bool unityManagedStartup() { return managedStartup.load(); }
API void FitdUnity_UseManagedStartup() { managedStartup = true; }
API void FitdUnity_SetRenderSize(int w,int h) {
    if(w >= 320 && w <= 4096 && h >= 200 && h <= 4096) renderSize = uint32_t(w) | (uint32_t(h) << 16);
}
unsigned int unityAudioSampleRate() { return sampleRate; }
const bool* unityKeyboardState(int* count){if(count)*count=int(keyboard.size());return keyboard.data();}
bool unityGamepadButton(SDL_Gamepad*,SDL_GamepadButton button){return button>=0 && button<32 && (currentPad.buttons&(uint32_t(1)<<button))!=0;}
Sint16 unityGamepadAxis(SDL_Gamepad*,SDL_GamepadAxis axis){return axis>=0 && axis<6?Sint16(std::clamp(currentPad.axes[axis],-1.f,1.f)*32767.f):0;}

void unityCheckStop() {
    std::unique_lock<std::mutex> lock(controlMutex);
    const bool paused = state == 2;
    if (paused && gSoloud) gSoloud->setPauseAll(true);
    resume.wait(lock, [] { return state != 2 || stopping.load(); });
    if (paused) {
        frameClockReset = true;
        if (gSoloud) gSoloud->setPauseAll(false);
    }
    if (stopping) throw FitdHostExit{0};
}

void unityFrameInput() {
    if (toggleOptions.exchange(false)) remasterOptionsToggle();
    Input snapshot;
    { std::lock_guard<std::mutex> lock(inputMutex); snapshot = input;currentPad=incomingPad;
      for(size_t i=0;i<keyboard.size();++i)keyboard[i]=incomingKeys[i]!=0; }
    JoyD = snapshot.joy; key = snapshot.key; Click = snapshot.click;
    auto held=[](KeyAction action){auto code=getKeyBinding(action).keyboard;return code>=0 && size_t(code)<keyboard.size() && keyboard[code];};
    if(held(ACTION_UP))JoyD|=1;if(held(ACTION_DOWN))JoyD|=2;if(held(ACTION_LEFT))JoyD|=4;if(held(ACTION_RIGHT))JoyD|=8;
    if(held(ACTION_QUICK_TURN_LEFT))JoyD|=16;if(held(ACTION_QUICK_TURN_RIGHT))JoyD|=32;if(held(ACTION_RUN))JoyD|=64;
    if(held(ACTION_ACTION))Click=1;if(held(ACTION_CONFIRM))key=0x1c;if(held(ACTION_CANCEL))key=0x1b;
    g_controllerState.connected=currentPad.connected && g_remasterConfig.controller.enableController;
    if(!g_controllerState.connected){g_controllerState.leftStickX=g_controllerState.leftStickY=g_controllerState.rightStickX=g_controllerState.rightStickY=0;}
#ifdef FITD_UNITY_COMMANDS
    updateController();
#endif
    sceneGame=g_gameId;sceneFloor=g_currentFloor;sceneCamera=NumCamera;sceneActive=g_playWorldActive?1:0;
    auto& io = ImGui::GetIO();
    io.AddMousePosEvent(snapshot.x, snapshot.y);
    io.AddMouseButtonEvent(0, (snapshot.buttons & 1) != 0);
    io.AddMouseButtonEvent(1, (snapshot.buttons & 2) != 0);
    io.AddKeyEvent(ImGuiKey_Enter, key == 0x1c);
    io.AddKeyEvent(ImGuiKey_Escape, key == 0x1b);
    io.AddKeyEvent(ImGuiKey_UpArrow, (JoyD & 1) != 0);
    io.AddKeyEvent(ImGuiKey_DownArrow, (JoyD & 2) != 0);
    io.AddKeyEvent(ImGuiKey_LeftArrow, (JoyD & 4) != 0);
    io.AddKeyEvent(ImGuiKey_RightArrow, (JoyD & 8) != 0);
    if (remasterOptionsIsOpen()) JoyD = key = Click = 0;
    consumedJoy=JoyD;consumedKey=key;consumedClick=Click;consumedPad=currentPad.connected?1:0;++inputSequence;
}

API int FitdUnity_GetState() { return state; }
API int FitdUnity_OptionsOpen() { return remasterOptionsIsOpen()?1:0; }
API int FitdUnity_GetAbiVersion() {
#ifdef FITD_UNITY_COMMANDS
    return 2;
#else
    return 1;
#endif
}
API void FitdUnity_ConfigureAudio(unsigned int rate) {
    std::lock_guard<std::mutex> lock(audioMutex);
    if (!audioReady && rate >= 8000 && rate <= 192000) sampleRate = rate;
}
API void FitdUnity_MixAudio(float* buffer, unsigned int frames) {
    if (!buffer || frames > 65536) return;
    std::unique_lock<std::mutex> lock(audioMutex,std::try_to_lock);
    if (!lock.owns_lock() || !audioReady || state != 1) { memset(buffer, 0, size_t(frames)*2*sizeof(float)); return; }
    gSoloud->mix(buffer, frames);
}
API void FitdUnity_ToggleOptions() { toggleOptions = true; }
API void FitdUnity_GetError(char* buffer, int capacity) {
    if (!buffer || capacity <= 0) return;
    std::lock_guard<std::mutex> lock(errorMutex);
    snprintf(buffer, size_t(capacity), "%s", error);
}
API int FitdUnity_Start(int w, int h) {
    if (state == 1 || state == 2) {
        { std::lock_guard<std::mutex> lock(controlMutex); state = 1; }
        resume.notify_all(); return 1;
    }
    if (state != 0) return 0; // Legacy globals require reloading the host to start a fresh game.
    if (w < 320 || h < 200 || w > 4096 || h > 4096) { fail("Invalid render dimensions."); return 0; }
    try {
#ifdef FITD_UNITY_COMMANDS
        const auto path = fitdReadRoot() + "/";
#else
        const auto path = std::filesystem::current_path().string() + "/";
#endif
        if (path.size() >= sizeof(homePath)) { fail("Game data path exceeds the engine path limit."); return 0; }
        snprintf(homePath, sizeof(homePath), "%s", path.c_str());
        FitdUnity_SetRenderSize(w,h);
        state = 1;
        worker = std::thread(run, w, h);
        return 1;
    } catch (const std::exception& e) { fail(e.what()); return 0; }
}
API void FitdUnity_SetPaused(int paused) {
    if (paused) frameClockReset = true;
    { std::lock_guard<std::mutex> lock(controlMutex);
      if (state == 1 || state == 2) state = paused ? 2 : 1; }
    resume.notify_all();
}
API void FitdUnity_SetInput(int joy, int keyCode, int click, float x, float y, int buttons) {
    std::lock_guard<std::mutex> lock(inputMutex);
    input = {joy, keyCode, click, x, y, buttons};
}
API void FitdUnity_SetKeyboardState(const uint8_t* keys,int count) {
    if(!keys || count!=int(incomingKeys.size()))return;
    std::lock_guard<std::mutex> lock(inputMutex);std::copy(keys,keys+count,incomingKeys.begin());
}
API void FitdUnity_SetGamepad(int connected,uint32_t buttons,float lx,float ly,float rx,float ry,float lt,float rt) {
    std::lock_guard<std::mutex> lock(inputMutex);incomingPad.connected=connected!=0;incomingPad.buttons=buttons;incomingPad.axes={lx,ly,rx,ry,lt,rt};
}
API void FitdUnity_GetInputStatus(int* values,int count){
    if(!values || count!=5)return;
    values[0]=consumedJoy;values[1]=consumedKey;values[2]=consumedClick;values[3]=consumedPad;values[4]=inputSequence;
}
API void FitdUnity_GetSceneInfo(int* game,int* floor,int* camera,int* active) {
    if(game)*game=sceneGame;if(floor)*floor=sceneFloor;if(camera)*camera=sceneCamera;if(active)*active=sceneActive;
}
API int FitdUnity_CopyFrame(void* destination, int capacity, int* w, int* h, uint64_t* serial) {
    if (!w || !h || !serial) return -1;
    std::lock_guard<std::mutex> lock(frameMutex);
    *w = width; *h = height;
    if (!sequence || *serial == sequence) return 0;
    if (!destination || capacity < int(pixels.size())) return -2;
    memcpy(destination, pixels.data(), pixels.size()); *serial = sequence;
    return int(pixels.size());
}
API void FitdUnity_RequestQuit() {
    {std::lock_guard<std::mutex> lock(controlMutex);stopping=true;}
    resume.notify_all();
#ifdef FITD_UNITY_COMMANDS
    FitdUnity_CancelGpuWait();
    fitdCancelMemoryReads();
#endif
}
API void FitdUnity_Stop() {
    std::lock_guard<std::mutex> stopLock(stopMutex);
    FitdUnity_RequestQuit();
    if (worker.joinable()) worker.join();
}
// Unity invokes this before unloading a native plugin.
API int FitdUnity_GetFontStatus(){return getTTFFontStatus();}
API void UnityPluginLoad(void*) {}
API void UnityPluginUnload() { FitdUnity_Stop(); }

int osystem_mouseRight = 0, osystem_mouseLeft = 0;
extern "C" void Sound_Quit() { osystemAL_deinit(); }
void osystem_init() {}
void osystem_updateImage() {}
void osystem_delay(int ms) {
    // Long menu/sequence waits also remain interruptible.
    while (ms > 0) { unityCheckStop(); int slice = (ms > 10 ? 10 : ms); SDL_Delay(slice); ms -= slice; }
}
u32 osystem_startOfFrame() {
    unityCheckStop();
#ifndef FITD_UNITY_COMMANDS
    SDL_PumpEvents();
    SDL_Event event;
    while (SDL_PollEvent(&event)) {} // Unity is the sole input source.
#endif
    osystemAL_udpate();
    StartFrame(); osystem_startFrame();
    static u32 last = 0;
    u32 now = static_cast<u32>(SDL_GetTicks());
    u32 elapsed = frameClockReset ? 40 : now - last;
    last = now; frameClockReset = false;
    return elapsed / 40 > 0 ? elapsed / 40 : 1;
}
void osystem_endOfFrame() {
    osystem_flushPendingPrimitives(); osystem_drawUILayer(); EndFrame();
    unityCheckStop();
}
bool fileExists(const char* name) {
#ifdef FITD_UNITY_COMMANDS
    if (fitdHasMemoryFile(name)) return true;
#endif
    if (getEmbeddedFile(name, nullptr, nullptr)) return true;
    FILE* file = nullptr; fitdFopen_s(&file, name, "rb");
    if (!file) return false;
    fclose(file); return true;
}
