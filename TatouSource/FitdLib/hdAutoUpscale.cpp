#include "hdAutoUpscale.h"

#include "backgroundDump.h"
#include "common.h"
#include "configRemaster.h"
#include "consoleLog.h"
#include "imgui.h"
#include "modelAtlas.h"
#include "sequence.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <condition_variable>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

extern "C" { extern char homePath[512]; }

namespace fs = std::filesystem;

namespace
{
static bool isSequelMode()
{
    return g_remasterConfig.gameData.mode == REMASTER_GAME_AITD2 ||
           g_remasterConfig.gameData.mode == REMASTER_GAME_AITD3;
}

static bool anyUpscaleRequested()
{
    return isSequelMode() && (g_remasterConfig.assetGeneration.autoUpscaleMissingHD ||
        g_remasterConfig.backgrounds.dumpEnabled || g_remasterConfig.sequences.dumpEnabled);
}

struct UpscaleJob
{
    fs::path source;
    fs::path output;
    bool replaceSource = false;
    int scale = 4;
    bool directory = false;
};

struct UpscaleState
{
    std::mutex mutex;
    std::condition_variable wake;
    std::deque<UpscaleJob> jobs;
    std::set<std::string> knownOutputs;
    std::string executable;
    std::string models;
    std::string modelName = "upscayl-standard-4x";
    std::string current;
    std::string lastError;
    int total = 0;
    int completed = 0;
    int failed = 0;
    bool workerStarted = false;
    bool available = false;
    bool gameplayCritical = false;
#ifdef _WIN32
    HANDLE currentProcess = nullptr;
    bool processSuspended = false;
#endif
};

// Deliberately process-lifetime storage. Several legacy exit paths call
// exit(), and joining an external GPU process during shutdown would stall.
UpscaleState& state()
{
    static UpscaleState* s = new UpscaleState();
    return *s;
}

static std::string quote(const std::string& value)
{
    return std::string("\"") + value + "\"";
}

static bool isImage(const fs::path& path)
{
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)tolower(c); });
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".webp";
}

static bool hasImages(const fs::path& dir)
{
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) return false;
    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(dir, ec))
        if (entry.is_regular_file(ec) && isImage(entry.path())) return true;
    return false;
}

static std::string readTextFile(const fs::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) return {};
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

static bool readPngDimensions(const fs::path& path, unsigned int& width, unsigned int& height)
{
    unsigned char header[24]{};
    std::ifstream input(path, std::ios::binary);
    input.read((char*)header, sizeof(header));
    if (input.gcount() != sizeof(header) ||
        memcmp(header, "\x89PNG\r\n\x1a\n", 8) != 0)
        return false;
    width = ((unsigned int)header[16] << 24) | ((unsigned int)header[17] << 16) |
            ((unsigned int)header[18] << 8) | header[19];
    height = ((unsigned int)header[20] << 24) | ((unsigned int)header[21] << 16) |
             ((unsigned int)header[22] << 8) | header[23];
    return width > 0 && height > 0;
}

static bool directoryIsUpscaled(const fs::path& source, const fs::path& output, int scale)
{
    std::error_code ec;
    if (!fs::is_directory(source, ec) || !fs::is_directory(output, ec)) return false;
    size_t checked = 0;
    for (const fs::directory_entry& entry : fs::directory_iterator(source, ec))
    {
        if (!entry.is_regular_file(ec) || !isImage(entry.path())) continue;
        fs::path target = output / entry.path().filename();
        target.replace_extension(".png");
        unsigned int sourceW, sourceH, outputW, outputH;
        if (!readPngDimensions(entry.path(), sourceW, sourceH) ||
            !readPngDimensions(target, outputW, outputH) ||
            outputW < sourceW * (unsigned int)scale ||
            outputH < sourceH * (unsigned int)scale)
            return false;
        ++checked;
    }
    return checked > 0;
}

static void findUpscayl()
{
    UpscaleState& s = state();
    const char* configured = getenv("UPSCAYL_BIN");
    std::vector<fs::path> candidates;
    if (configured && *configured) candidates.emplace_back(configured);
#ifdef _WIN32
    fs::path runtimeBase = homePath[0] ? fs::path(homePath) : fs::current_path();
    candidates.emplace_back(runtimeBase / "upscayl/bin/upscayl-bin.exe");
    candidates.emplace_back(fs::current_path() / "upscayl/bin/upscayl-bin.exe");
    const char* programFiles = getenv("ProgramFiles");
    const char* localAppData = getenv("LOCALAPPDATA");
    if (programFiles) candidates.emplace_back(fs::path(programFiles) / "Upscayl/resources/bin/upscayl-bin.exe");
    if (localAppData) candidates.emplace_back(fs::path(localAppData) / "Programs/Upscayl/resources/bin/upscayl-bin.exe");
#else
    candidates.emplace_back("/usr/bin/upscayl-bin");
    candidates.emplace_back("/usr/local/bin/upscayl-bin");
#endif
    for (const fs::path& candidate : candidates)
    {
        std::error_code ec;
        if (!fs::is_regular_file(candidate, ec)) continue;
        s.executable = candidate.string();
        fs::path modelDir = candidate.parent_path().parent_path() / "models";
        if (fs::is_directory(modelDir, ec))
        {
            s.models = modelDir.string();
            if (fs::is_regular_file(modelDir / "upscayl-lite-4x.bin", ec) &&
                fs::is_regular_file(modelDir / "upscayl-lite-4x.param", ec))
                s.modelName = "upscayl-lite-4x";
        }
        s.available = true;
        return;
    }
}

static bool runJob(const UpscaleJob& job)
{
    UpscaleState& s = state();
    std::error_code ec;
    fs::create_directories(job.directory ? job.output : job.output.parent_path(), ec);

    // Upscayl writes progressively.  Never expose that file at the runtime HD
    // path: stb_image could otherwise observe a valid PNG header followed by
    // incomplete image data while a sequence is playing.  Publish only the
    // completed file with an atomic rename.
    fs::path actualOutput = job.output;
    if (!job.directory)
    {
        actualOutput = job.output.parent_path() /
            (job.output.filename().string() + ".upscayl.part.png");
        fs::remove(actualOutput, ec);
        ec.clear();
    }

    int scale = job.scale;
    std::string command = quote(s.executable) + " -i " + quote(job.source.string()) +
        " -o " + quote(actualOutput.string()) + " -z 4 -s " + std::to_string(scale) +
        " -f png -n " + s.modelName + " -t 0 -j 2:2:2";
    if (!s.models.empty()) command += " -m " + quote(s.models);

    bool ok = false;
#ifdef _WIN32
    STARTUPINFOA startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    std::vector<char> mutableCommand(command.begin(), command.end());
    mutableCommand.push_back('\0');
    if (CreateProcessA(nullptr, mutableCommand.data(), nullptr, nullptr, FALSE,
                       CREATE_NO_WINDOW | CREATE_SUSPENDED | IDLE_PRIORITY_CLASS,
                       nullptr, nullptr, &startup, &process))
    {
        HANDLE jobObject = CreateJobObjectA(nullptr, nullptr);
        if (jobObject)
        {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
            limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            SetInformationJobObject(jobObject, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
            AssignProcessToJobObject(jobObject, process.hProcess);
        }
        {
            std::lock_guard<std::mutex> lock(s.mutex);
            s.currentProcess = process.hProcess;
            s.processSuspended = s.gameplayCritical;
            if (!s.gameplayCritical) ResumeThread(process.hThread);
        }
        WaitForSingleObject(process.hProcess, INFINITE);
        DWORD exitCode = 1;
        GetExitCodeProcess(process.hProcess, &exitCode);
        {
            std::lock_guard<std::mutex> lock(s.mutex);
            s.currentProcess = nullptr;
            s.processSuspended = false;
        }
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        if (jobObject) CloseHandle(jobObject);
        ok = exitCode == 0 && (job.directory ? hasImages(actualOutput) : fs::is_regular_file(actualOutput, ec));
    }
#else
    ok = std::system(command.c_str()) == 0 && (job.directory ? hasImages(actualOutput) : fs::is_regular_file(actualOutput, ec));
#endif

    if (ok && !job.directory)
    {
#ifdef _WIN32
        ok = MoveFileExA(actualOutput.string().c_str(), job.output.string().c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
#else
        fs::rename(actualOutput, job.output, ec);
        ok = !ec;
#endif
        if (ok && job.replaceSource)
        {
            std::ofstream marker(job.output.string() + ".upscayl.done", std::ios::trunc);
            marker << "Upscayl " << job.scale << "x\n";
        }
    }
    if (!ok && !job.directory)
    {
        ec.clear();
        fs::remove(actualOutput, ec);
    }
    return ok;
}

static void workerMain()
{
    UpscaleState& s = state();
    for (;;)
    {
        UpscaleJob job;
        {
            std::unique_lock<std::mutex> lock(s.mutex);
            s.wake.wait(lock, [&] { return !s.jobs.empty() && !s.gameplayCritical; });
            job = s.jobs.front();
            s.jobs.pop_front();
            s.current = job.source.filename().string();
        }

        bool ok = runJob(job);
        {
            std::lock_guard<std::mutex> lock(s.mutex);
            if (ok) ++s.completed;
            else
            {
                ++s.failed;
                s.lastError = "Upscayl failed for " + job.source.filename().string();
            }
            s.current.clear();
        }
    }
}

static void ensureWorker()
{
    UpscaleState& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    if (s.workerStarted || !s.available) return;
    s.workerStarted = true;
    std::thread(workerMain).detach();
}
}

bool hdAutoUpscale_IsEnabled()
{
    return isSequelMode() && g_remasterConfig.assetGeneration.autoUpscaleMissingHD;
}

void hdAutoUpscale_SetGameplayCritical(bool critical)
{
    UpscaleState& s = state();
    {
        std::lock_guard<std::mutex> lock(s.mutex);
        if (s.gameplayCritical == critical) return;
        s.gameplayCritical = critical;
#ifdef _WIN32
        if (s.currentProcess)
        {
            using NtProcessControl = LONG (NTAPI*)(HANDLE);
            HMODULE ntdll = GetModuleHandleA("ntdll.dll");
            if (ntdll)
            {
                if (critical && !s.processSuspended)
                {
                    NtProcessControl suspendProcess =
                        (NtProcessControl)GetProcAddress(ntdll, "NtSuspendProcess");
                    if (suspendProcess && suspendProcess(s.currentProcess) >= 0)
                        s.processSuspended = true;
                }
                else if (!critical && s.processSuspended)
                {
                    NtProcessControl resumeProcess =
                        (NtProcessControl)GetProcAddress(ntdll, "NtResumeProcess");
                    if (resumeProcess && resumeProcess(s.currentProcess) >= 0)
                        s.processSuspended = false;
                }
            }
        }
#endif
    }
    if (!critical) s.wake.notify_all();
}

void hdAutoUpscale_QueueFile(const std::string& sourcePath, const std::string& outputPath)
{
    if (!anyUpscaleRequested()) return;
    UpscaleState& s = state();
    if (!s.available) return;

    std::error_code ec;
    fs::path source(sourcePath), output(outputPath);
    if (!fs::is_regular_file(source, ec) || fs::is_regular_file(output, ec)) return;

    std::string key = fs::absolute(output, ec).lexically_normal().string();
    {
        std::lock_guard<std::mutex> lock(s.mutex);
        if (!s.knownOutputs.insert(key).second) return;
        int scale = (std::max)(2, (std::min)(4, g_remasterConfig.assetGeneration.upscaleScale));
        s.jobs.push_back({ source, output, false, scale, false });
        ++s.total;
    }
    s.wake.notify_one();
}

void hdAutoUpscale_QueueDirectory(const std::string& sourceDirectory, const std::string& outputDirectory)
{
    if (!anyUpscaleRequested()) return;
    UpscaleState& s = state();
    if (!s.available) return;

    std::error_code ec;
    fs::path source(sourceDirectory), output(outputDirectory);
    if (!hasImages(source)) return;

    int scale = (std::max)(2, (std::min)(4, g_remasterConfig.assetGeneration.upscaleScale));
    fs::create_directories(output, ec);
    int added = 0;
    {
        std::lock_guard<std::mutex> lock(s.mutex);
        for (const fs::directory_entry& entry : fs::directory_iterator(source, ec))
        {
            if (!entry.is_regular_file(ec) || !isImage(entry.path())) continue;
            const std::string sourceName = entry.path().filename().string();
            if (sourceName.find(".upscayl.") != std::string::npos) continue;

            fs::path target = output / entry.path().filename();
            target.replace_extension(".png");
            unsigned int sourceW = 0, sourceH = 0, outputW = 0, outputH = 0;
            if (readPngDimensions(entry.path(), sourceW, sourceH) &&
                readPngDimensions(target, outputW, outputH) &&
                outputW >= sourceW * (unsigned int)scale &&
                outputH >= sourceH * (unsigned int)scale)
                continue;

            std::string key = fs::absolute(target, ec).lexically_normal().string();
            ec.clear();
            if (!s.knownOutputs.insert(key).second) continue;
            s.jobs.push_back({ entry.path(), target, false, scale, false });
            ++s.total;
            ++added;
        }
    }
    if (added > 0) s.wake.notify_all();
}

void hdAutoUpscale_QueueInPlace(const std::string& path)
{
    if (!hdAutoUpscale_IsEnabled()) return;
    UpscaleState& s = state();
    const char* atlasPack = g_remasterConfig.gameData.mode == REMASTER_GAME_AITD2
        ? "atlases_aitd2.hda" : "atlases_aitd3.hda";
    fs::path base = homePath[0] ? fs::path(homePath) : fs::current_path();
    if (!s.available || fs::is_regular_file(base / atlasPack)) return;
    std::error_code ec;
    fs::path source(path);
    if (!fs::is_regular_file(source, ec)) return;
    if (fs::is_regular_file(source.string() + ".upscayl.done", ec)) return;
    std::string key = fs::absolute(source, ec).lexically_normal().string();
    {
        std::lock_guard<std::mutex> lock(s.mutex);
        if (!s.knownOutputs.insert(key).second) return;
        int scale = (std::max)(2, (std::min)(4, g_remasterConfig.assetGeneration.upscaleScale));
        s.jobs.push_back({ source, source, true, scale, false });
        ++s.total;
    }
    s.wake.notify_one();
}

void hdAutoUpscale_Initialize()
{
    if (!anyUpscaleRequested()) return;
    findUpscayl();
    UpscaleState& s = state();
    if (!s.available)
    {
        std::lock_guard<std::mutex> lock(s.mutex);
        s.lastError = "Upscayl was not found. Install it or set UPSCAYL_BIN.";
        return;
    }
    ensureWorker();

    fs::path base = homePath[0] ? fs::path(homePath) : fs::current_path();
    const bool aitd2 = g_remasterConfig.gameData.mode == REMASTER_GAME_AITD2;
    fs::path pack = base / (aitd2 ? "backgrounds_hd_aitd2.hda" : "backgrounds_hd_aitd3.hda");
    fs::path sourceDir = base / (aitd2 ? "backgrounds_upscale_source_aitd2" : "backgrounds_upscale_source_aitd3");
    fs::path outputDir = base / (aitd2 ? "backgrounds_hd_aitd2" : "backgrounds_hd_aitd3");
    std::error_code ec;

    if (hdAutoUpscale_IsEnabled() && !fs::is_regular_file(pack, ec))
    {
        // A pre-existing artist pack wins. A retained source folder denotes
        // our own interrupted/partial queue and is safe to resume.
        if (!hasImages(outputDir) || hasImages(sourceDir))
        {
            const bool paletteRecipeChanged =
                readTextFile(sourceDir / ".fitd_dump_recipe") != "camera-palette-v2\n";
            if (paletteRecipeChanged || !hasImages(sourceDir))
            {
                beginIncrementalBackgroundDump(sourceDir.string().c_str(),
                                               outputDir.string().c_str(),
                                               paletteRecipeChanged);
            }
            // When the palette recipe changed, do not upscale stale source
            // PNGs. The incremental dumper queues each corrected file as it
            // is published.
            if (!paletteRecipeChanged)
                hdAutoUpscale_QueueDirectory(sourceDir.string(), outputDir.string());
        }
    }

    // Generate every embedded model and movie source incrementally from the
    // main loop. This no longer depends on a particular playthrough reaching
    // the asset, while PAK access remains safely single-threaded.
    if (hdAutoUpscale_IsEnabled())
        beginFullModelAtlasDump();
    if (hdAutoUpscale_IsEnabled() || g_remasterConfig.sequences.dumpEnabled)
        beginFullSequenceDump();

    // Resume atlases produced just before a previous shutdown. Scoped names
    // ensure we never reprocess AITD1/JACK or artist-supplied sequel packs.
    fs::path atlasDir = base / "atlases";
    std::string atlasPrefix = aitd2 ? "AITD2_" : "AITD3_";
    if (fs::is_directory(atlasDir, ec))
    {
        for (const fs::directory_entry& entry : fs::directory_iterator(atlasDir, ec))
        {
            std::string name = entry.path().filename().string();
            if (entry.is_regular_file(ec) && isImage(entry.path()) &&
                name.find(atlasPrefix) != std::string::npos &&
                name.find(".upscayl.") == std::string::npos)
                hdAutoUpscale_QueueInPlace(entry.path().string());
        }
    }
}

void hdAutoUpscale_DrawOverlay()
{
    // Run exactly one source-generation slice per rendered frame. This keeps
    // PNG encoding/model rendering from stacking into a visible gameplay
    // pause while the expensive neural upscale remains on its worker.
    if (!isSequencePlaybackActive())
    {
        if (hdAutoUpscale_IsEnabled()) beginFullModelAtlasDump();
        static unsigned int schedulerSlot = 0;
        for (unsigned int attempt = 0; attempt < 3; ++attempt)
        {
            const unsigned int slot = (schedulerSlot + attempt) % 3;
            bool didWork = false;
            if (slot == 0 && isIncrementalBackgroundDumpActive())
                didWork = pumpIncrementalBackgroundDump();
            else if (slot == 1 && isFullSequenceDumpActive())
                didWork = pumpFullSequenceDumpFrame();
            else if (slot == 2 && isFullModelAtlasDumpActive())
                didWork = pumpFullModelAtlasDumpBody();
            if (didWork)
            {
                schedulerSlot = (slot + 1) % 3;
                break;
            }
        }
    }

    UpscaleState& s = state();
    int total, completed, failed, queued;
    std::string current, error;
    {
        std::lock_guard<std::mutex> lock(s.mutex);
        total = s.total;
        completed = s.completed;
        failed = s.failed;
        queued = (int)s.jobs.size();
        current = s.current;
        error = s.lastError;
    }
    if (!anyUpscaleRequested() || (total == 0 && error.empty())) return;
    if (total > 0 && completed + failed >= total && error.empty()) return;

    ImGui::SetNextWindowPos(ImVec2(24.0f, 80.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(430.0f, 0.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("HD asset generation###HDAutoUpscaleProgress", nullptr,
                     ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (!current.empty()) ImGui::Text("Upscaling: %s", current.c_str());
        else if (queued > 0) ImGui::TextUnformatted("Preparing next HD asset...");
        float progress = total > 0 ? (float)(completed + failed) / (float)total : 0.0f;
        char label[96];
        snprintf(label, sizeof(label), "%d / %d  (%d queued)", completed + failed, total, queued);
        ImGui::ProgressBar(progress, ImVec2(400.0f, 0.0f), label);
        ImGui::TextDisabled("This window can be dragged while the game continues.");
        if (!error.empty()) ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.3f, 1.0f), "%s", error.c_str());
    }
    ImGui::End();
}
