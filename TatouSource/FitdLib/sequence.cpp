///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// FMV cutscene sequence player
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "consoleLog.h"
#include "configRemaster.h"
#include "hdAutoUpscale.h"
#include "hdBackgroundRenderer.h"
#include "sequence.h"

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

// stb_image_write for sequence frame dumping (static to avoid linker conflicts)
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../ThirdParty/bgfx.cmake/bimg/3rdparty/stb/stb_image_write.h"

// stb_image for loading HD replacement sequence frames
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "../ThirdParty/bgfx.cmake/bimg/3rdparty/stb/stb_image.h"

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

const char* sequenceListAITD2[]=
{
    "BATL",
    "GRAP",
    "CLE1",
    "CLE2",
    "COOK",
    "EXPL",
    "FALA",
    "FAL2",
    "GLIS",
    "GREN",
    "JEND",
    "MANI",
    "MER",
    "TORD",
    "PANT",
    "VERE",
    "PL21",
    "PL22",
    "ENDX",
    "SORT",
    "EFER",
    "STAR",
    "MEDU",
    "PROL",
    "GRAS",
    "STRI",
    "ITRO",
    "BILL",
    "PIRA",
    "PIR2",
    "VENT",
    "FIN",
    "LAST"
};

void unapckSequenceFrame(unsigned char* source,unsigned char* dest)
{
    unsigned char byteCode;

    byteCode = *(source++);

    while(byteCode)
    {
        if(!(--byteCode)) // change pixel or skip pixel
        {
            unsigned char changeColor;

            changeColor = *(source++);

            if(changeColor)
            {
                *(dest++) = changeColor;
            }
            else
            {
                dest++;
            }
        }
        else
            if(!(--byteCode)) // change 2 pixels or skip 2 pixels
            {
                unsigned char changeColor;

                changeColor = *(source++);

                if(changeColor)
                {
                    *(dest++) = changeColor;
                    *(dest++) = changeColor;
                }
                else
                {
                    dest+=2;
                }
            }
            else
                if(!(--byteCode)) // fill or skip
                {
                    unsigned char size;
                    unsigned char fillColor;

                    size = *(source++);
                    fillColor = *(source++);

                    if(fillColor)
                    {
                        int i;

                        for(i=0;i<size;i++)
                        {
                            *(dest++) = fillColor;
                        }
                    }
                    else
                    {
                        dest+=size;
                    }
                }
                else // large fill of skip
                {
                    u16 size;
                    unsigned char fillColor;

                    size = READ_LE_U16(source);
                    source+=2;
                    fillColor = *(source++);

                    if(fillColor)
                    {
                        int i;

                        for(i=0;i<size;i++)
                        {
                            *(dest++) = fillColor;
                        }
                    }
                    else
                    {
                        dest+=size;
                    }
                }

                byteCode = *(source++);
    }
}

extern "C" { extern char homePath[512]; }

static const char* getHDSequenceFolder()
{
    if (g_gameId == AITD2) return "sequences_hd_aitd2";
    if (g_gameId == AITD3) return "sequences_hd_aitd3";
    return "sequences_hd";
}

namespace
{
struct FullSequenceDumpState
{
    std::vector<std::string> names;
    size_t sequenceIndex = 0;
    unsigned int frameIndex = 0;
    unsigned int frameCount = 0;
    std::vector<unsigned char> decoded;
    palette_t palette{};
    bool active = false;
    bool haveFirstFrame = false;
};

static bool g_sequencePlaybackActive = false;

FullSequenceDumpState& fullDumpState()
{
    static FullSequenceDumpState state;
    return state;
}

static void queueCompletedSequence(const std::string& name)
{
    char sourceDir[512], outputDir[512];
    snprintf(sourceDir, sizeof(sourceDir), "%ssequences_dump/%s", homePath, name.c_str());
    snprintf(outputDir, sizeof(outputDir), "%s%s/%s", homePath, getHDSequenceFolder(), name.c_str());
    hdAutoUpscale_QueueDirectory(sourceDir, outputDir);
}
}

bool isSequencePlaybackActive()
{
    return g_sequencePlaybackActive;
}

static void ensureSequenceDirectory(const char* dir)
{
#ifdef _WIN32
    _mkdir(dir);
#else
    mkdir(dir, 0755);
#endif
}

namespace
{
struct SequencePngJob
{
    std::string sourcePath;
    std::string upscalePath;
    std::vector<unsigned char> indexed;
    palette_t palette{};
};

struct SequencePngWriter
{
    std::mutex mutex;
    std::condition_variable wake;
    std::deque<SequencePngJob> jobs;
    std::set<std::string> knownPaths;
    bool started = false;
};

SequencePngWriter& sequencePngWriter()
{
    static SequencePngWriter* writer = new SequencePngWriter();
    return *writer;
}

void sequencePngWorker()
{
    SequencePngWriter& writer = sequencePngWriter();
    for (;;)
    {
        SequencePngJob job;
        {
            std::unique_lock<std::mutex> lock(writer.mutex);
            writer.wake.wait(lock, [&] { return !writer.jobs.empty(); });
            job = std::move(writer.jobs.front());
            writer.jobs.pop_front();
        }

        std::vector<unsigned char> rgba(320 * 200 * 4);
        for (int i = 0; i < 320 * 200; ++i)
        {
            const unsigned char index = job.indexed[i];
            rgba[i * 4 + 0] = job.palette[index][0];
            rgba[i * 4 + 1] = job.palette[index][1];
            rgba[i * 4 + 2] = job.palette[index][2];
            rgba[i * 4 + 3] = 255;
        }
        if (stbi_write_png(job.sourcePath.c_str(), 320, 200, 4, rgba.data(), 320 * 4))
        {
            printf(SEQ_OK "Dumped sequence frame: %s" CON_RESET "\n", job.sourcePath.c_str());
            hdAutoUpscale_QueueFile(job.sourcePath, job.upscalePath);
        }
        else
        {
            printf(SEQ_ERR "Failed to dump sequence frame: %s" CON_RESET "\n", job.sourcePath.c_str());
            std::lock_guard<std::mutex> lock(writer.mutex);
            writer.knownPaths.erase(job.sourcePath);
        }
    }
}

bool sequencePngQueueHasCapacity()
{
    SequencePngWriter& writer = sequencePngWriter();
    std::lock_guard<std::mutex> lock(writer.mutex);
    return writer.jobs.size() < 96;
}
}

static void dumpSequenceFrame(const char* seqName, int frameId, const char* auxBuffer, const palette_t& palette)
{
    char dirPath[512];
    snprintf(dirPath, sizeof(dirPath), "%ssequences_dump", homePath);
    ensureSequenceDirectory(dirPath);

    snprintf(dirPath, sizeof(dirPath), "%ssequences_dump/%s", homePath, seqName);
    ensureSequenceDirectory(dirPath);

    char filePath[512];
    snprintf(filePath, sizeof(filePath), "%ssequences_dump/%s/%s_%04d.png", homePath, seqName, seqName, frameId);
    char upscalePath[512];
    snprintf(upscalePath, sizeof(upscalePath), "%s%s/%s/%s_%04d.png",
             homePath, getHDSequenceFolder(), seqName, seqName, frameId);

    FILE* existing = nullptr;
#ifdef _WIN32
    fopen_s(&existing, filePath, "rb");
#else
    existing = fopen(filePath, "rb");
#endif
    if (existing)
    {
        fclose(existing);
        hdAutoUpscale_QueueFile(filePath, upscalePath);
        return;
    }

    SequencePngWriter& writer = sequencePngWriter();
    {
        std::lock_guard<std::mutex> lock(writer.mutex);
        if (!writer.knownPaths.insert(filePath).second) return;
        SequencePngJob job;
        job.sourcePath = filePath;
        job.upscalePath = upscalePath;
        job.indexed.assign((const unsigned char*)auxBuffer,
                           (const unsigned char*)auxBuffer + 320 * 200);
        job.palette = palette;
        writer.jobs.push_back(std::move(job));
        if (!writer.started)
        {
            writer.started = true;
            std::thread(sequencePngWorker).detach();
        }
    }
    writer.wake.notify_one();
}

static unsigned char* tryLoadHDSequenceFrame(const char* seqName, int frameId, int* outWidth, int* outHeight)
{
    char filePath[512];
    snprintf(filePath, sizeof(filePath), "%s%s/%s/%s_%04d.png", homePath, getHDSequenceFolder(), seqName, seqName, frameId);

    int channels = 0;
    unsigned char* data = stbi_load(filePath, outWidth, outHeight, &channels, 4);
    if (data)
    {
        printf(SEQ_OK "Loaded HD sequence frame: %s (%dx%d)" CON_RESET "\n", filePath, *outWidth, *outHeight);
    }
    return data;
}

static bool isUnexpectedlyBlackHDFrame(const unsigned char* hd, int width, int height,
                                       const char* indexed, const palette_t& palette)
{
    if (!hd || width <= 0 || height <= 0) return false;
    int hdVisible = 0;
    const int hdPixels = width * height;
    const int hdStep = (std::max)(1, hdPixels / 1024);
    int hdSamples = 0;
    for (int i = 0; i < hdPixels; i += hdStep)
    {
        const unsigned char* pixel = hd + i * 4;
        if ((int)pixel[0] + pixel[1] + pixel[2] > 36) ++hdVisible;
        ++hdSamples;
    }

    int sdVisible = 0;
    int sdSamples = 0;
    for (int i = 0; i < 320 * 200; i += 64)
    {
        const unsigned char index = (unsigned char)indexed[i];
        if ((int)palette[index][0] + palette[index][1] + palette[index][2] > 36) ++sdVisible;
        ++sdSamples;
    }
    return hdVisible * 100 < hdSamples && sdVisible * 10 > sdSamples;
}

void playSequence(int sequenceIdx, int fadeStart, int fadeOutVar)
{
    // Keep the incremental all-game dumpers out of the live movie decoder.
    // They share the legacy PAK/decompression path and are resumed as soon as
    // playback ends.
    g_sequencePlaybackActive = true;
    hdAutoUpscale_SetGameplayCritical(true);
    osystem_clearSequenceHDFrame();
    // A room may have left the renderer in HD-background mode.  Sequence
    // frames without an HD replacement still update the indexed 320x200
    // texture, so explicitly select that texture for the SD fallback.
    recreateBackgroundTexture(320, 200);

    int frames=0;                   /* Number of frames displayed */

    int var_4 = 1;
    int quitPlayback = 0;
    int nextFrame = 1;
    palette_t seqPalette;           // Palette at outer scope for dump/load access

    char buffer[256] = {};
    if (g_gameId == AITD2)
    {
        const int sequenceCount = (int)(sizeof(sequenceListAITD2) / sizeof(sequenceListAITD2[0]));
        if (sequenceIdx < 0 || sequenceIdx >= sequenceCount)
        {
            printf(SEQ_ERR "Invalid AITD2 sequence index %d (count=%d)" CON_RESET "\n",
                sequenceIdx, sequenceCount);
            osystem_clearSequenceHDFrame();
            g_sequencePlaybackActive = false;
            hdAutoUpscale_SetGameplayCritical(false);
            FlagInitView = 2;
            return;
        }
        strcpy(buffer, sequenceListAITD2[sequenceIdx]);
    }
    else if (g_gameId == AITD3)
    {
        if (sequenceIdx < 0)
        {
            printf(SEQ_ERR "Invalid AITD3 sequence index %d" CON_RESET "\n", sequenceIdx);
            osystem_clearSequenceHDFrame();
            g_sequencePlaybackActive = false;
            hdAutoUpscale_SetGameplayCritical(false);
            FlagInitView = 2;
            return;
        }
        sprintf_s(buffer, sizeof(buffer), "AN%d", sequenceIdx);
    }
    else
    {
        printf(SEQ_ERR "Sequence playback requested for unsupported game mode %d" CON_RESET "\n",
            g_gameId);
        osystem_clearSequenceHDFrame();
        g_sequencePlaybackActive = false;
        hdAutoUpscale_SetGameplayCritical(false);
        FlagInitView = 2;
        return;
    }

    printf(SEQ_TAG "Playing sequence: %s" CON_RESET "\n", buffer);

    int numMaxFrames = PAK_getNumFiles(buffer);
    if (numMaxFrames <= 0)
    {
        printf(SEQ_ERR "Sequence archive '%s' is missing or empty" CON_RESET "\n", buffer);
        osystem_clearSequenceHDFrame();
        g_sequencePlaybackActive = false;
        hdAutoUpscale_SetGameplayCritical(false);
        FlagInitView = 2;
        return;
    }

    while(!quitPlayback)
    {
        int currentFrameId = 0;
        int sequenceParamIdx;

        while(currentFrameId < nextFrame)
        {
            frames++;

            timer = timeGlobal;

            if (currentFrameId >= numMaxFrames)
            {
                quitPlayback = 1;
                break;
            }

            if(!LoadPak(buffer,currentFrameId,logicalScreen))
            {
                printf(SEQ_ERR "Unable to load frame %d from '%s'; ending sequence safely"
                    CON_RESET "\n", currentFrameId, buffer);
                quitPlayback = 1;
                break;
            }

            if(!currentFrameId) // first frame
            {
                copyPalette(logicalScreen, seqPalette);  // copy palette
                memcpy(aux,logicalScreen+0x300,64000);
                nextFrame = READ_LE_U16(logicalScreen+64768);

                convertPaletteIfRequired(seqPalette);

                if(var_4 != 0)
                {
                    /*      if(fadeStart & 1)
                    {
                    fadeOut(0x10,0);
                    }
                    if(fadeStart & 4)
                    {
                    //memset(palette,0,0); // fade from black
                    fadeInSub1(seqPalette);
                    flipOtherPalette(palette);
                    } */

                    osystem_setPalette(&seqPalette);
                    copyPalette(seqPalette,currentGamePalette);
                }
            }
            else // not first frame
            {
                U32 frameSize;

                frameSize = READ_LE_U32(logicalScreen);

                if(frameSize < 64000) // key frame
                {
                    unapckSequenceFrame((unsigned char*)logicalScreen+4,(unsigned char*)aux);
                }
                else // delta frame
                {
                    FastCopyScreen(logicalScreen,aux);
                }
            }

            for(sequenceParamIdx = 0; sequenceParamIdx < numSequenceParam; sequenceParamIdx++)
            {
                if(sequenceParams[sequenceParamIdx].frame == currentFrameId)
                {
                    playSound(sequenceParams[sequenceParamIdx].sample);
                }
            }

			// TODO: here, timming management
			// TODO: fade management

			// Dump decoded frame to PNG if enabled
			if (g_remasterConfig.sequences.dumpEnabled || hdAutoUpscale_IsEnabled())
			{
				dumpSequenceFrame(buffer, currentFrameId, aux, seqPalette);
			}

			// Try loading HD replacement frame
			bool usedHDFrame = false;
			if (g_remasterConfig.sequences.loadEnabled)
			{
				int hdW = 0, hdH = 0;
				unsigned char* hdFrame = tryLoadHDSequenceFrame(buffer, currentFrameId, &hdW, &hdH);
				if (hdFrame)
				{
					if (!isUnexpectedlyBlackHDFrame(hdFrame, hdW, hdH, aux, seqPalette))
					{
						osystem_setSequenceHDFrame(hdFrame, hdW, hdH);
						usedHDFrame = true;
					}
					else
					{
						printf(SEQ_WARN "Ignoring unexpectedly black HD frame %s:%d; using original frame."
						       CON_RESET "\n", buffer, currentFrameId);
					}
					stbi_image_free(hdFrame);
				}
			}

			if (!usedHDFrame)
			{
				osystem_clearSequenceHDFrame();
				osystem_CopyBlockPhys((unsigned char*)aux,0,0,320,200);
			}

            currentFrameId++;

			for(int i=0;i<5;i++) // display the frame 5 times (original seems to wait 5 sync)
			{
				// process_events() closes and begins a render frame on the normal
				// desktop path. Submit the movie quad on every held frame; otherwise
				// only the first sync contains the sequence and the final four clear
				// to the stale room/black frame, making playback look frozen.
				osystem_drawBackground();
				process_events();
			}

			if(key)
			{
				//stopSample();
				quitPlayback = 1;
				break;
			}

        }

        fadeOutVar--;

        if(fadeOutVar==0)
        {
            quitPlayback=1;
        }
    }

	osystem_clearSequenceHDFrame();
	g_sequencePlaybackActive = false;
	hdAutoUpscale_SetGameplayCritical(false);
	if (g_remasterConfig.sequences.dumpEnabled || hdAutoUpscale_IsEnabled())
	{
		char sourceDir[512], outputDir[512];
		snprintf(sourceDir, sizeof(sourceDir), "%ssequences_dump/%s", homePath, buffer);
		snprintf(outputDir, sizeof(outputDir), "%s%s/%s", homePath, getHDSequenceFolder(), buffer);
		hdAutoUpscale_QueueDirectory(sourceDir, outputDir);
	}
	FlagInitView = 2;
}

void beginFullSequenceDump()
{
    FullSequenceDumpState& state = fullDumpState();
    if (state.active) return;

    state = FullSequenceDumpState{};
    state.decoded.resize(320 * 200);
    if (g_gameId == AITD2)
    {
        const size_t count = sizeof(sequenceListAITD2) / sizeof(sequenceListAITD2[0]);
        for (size_t i = 0; i < count; ++i)
            if (PAK_getNumFiles(sequenceListAITD2[i]) > 0)
                state.names.emplace_back(sequenceListAITD2[i]);
    }
    else if (g_gameId == AITD3)
    {
        // AITD3 sequence archives are sparse (AN1, AN14, AN17, ...), so
        // probe the complete bytecode index range instead of guessing which
        // movies a particular playthrough will reach.
        char name[32];
        for (int i = 0; i < 256; ++i)
        {
            snprintf(name, sizeof(name), "AN%d", i);
            if (PAK_getNumFiles(name) > 0) state.names.emplace_back(name);
        }
    }
    state.active = !state.names.empty();
    printf(SEQ_TAG "Queued full-game movie dump: %zu sequence archive(s)\n", state.names.size());
}

bool isFullSequenceDumpActive()
{
    return fullDumpState().active;
}

bool pumpFullSequenceDumpFrame()
{
    FullSequenceDumpState& state = fullDumpState();
    if (!state.active) return false;
    if (!sequencePngQueueHasCapacity()) return false;

    while (state.sequenceIndex < state.names.size())
    {
        const std::string& name = state.names[state.sequenceIndex];
        if (state.frameIndex == 0)
        {
            state.frameCount = PAK_getNumFiles(name.c_str());
            state.haveFirstFrame = false;
            std::fill(state.decoded.begin(), state.decoded.end(), 0);
        }
        if (state.frameIndex >= state.frameCount)
        {
            if (state.haveFirstFrame) queueCompletedSequence(name);
            ++state.sequenceIndex;
            state.frameIndex = 0;
            continue;
        }

        const int size = getPakSize(name.c_str(), state.frameIndex);
        char* packed = size > 0 ? loadPak(name.c_str(), state.frameIndex) : nullptr;
        if (packed)
        {
            if (state.frameIndex == 0 && size >= 0x300 + 320 * 200)
            {
                copyPalette(packed, state.palette);
                convertPaletteIfRequired(state.palette);
                memcpy(state.decoded.data(), packed + 0x300, 320 * 200);
                state.haveFirstFrame = true;
            }
            else if (state.haveFirstFrame && size >= 4)
            {
                const U32 frameSize = READ_LE_U32(packed);
                if (frameSize < 320 * 200)
                    unapckSequenceFrame((unsigned char*)packed + 4, state.decoded.data());
                else if (size >= 320 * 200)
                    memcpy(state.decoded.data(), packed, 320 * 200);
            }

            if (state.haveFirstFrame)
                dumpSequenceFrame(name.c_str(), (int)state.frameIndex,
                                  (const char*)state.decoded.data(), state.palette);
            free(packed);
        }
        ++state.frameIndex;
        return true;
    }

    state.active = false;
    printf(SEQ_OK "Full-game movie dump complete." CON_RESET "\n");
    return false;
}
