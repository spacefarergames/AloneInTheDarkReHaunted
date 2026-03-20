///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Author: Jake Jackson (jake@spacefarergames.com)
//
// OpenAL audio system for sound effects and music
///////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <assert.h>

//#include "OpenAL/al.h"
//#include "OpenAL/alc.h"

#include "soloud.h"
#include "soloud_file.h"
#include "soloud_wav.h"
#include "soloud_wavstream.h"

#include "common.h"
#include "osystemAL.h"
#include "hdArchive.h"

void osystemAL_mp3_Update();
void osystemAL_adlib_Update();

SoLoud::Soloud* gSoloud = NULL;

// Self-contained audio archive reader (same HDBG format, separate instance from HDArchive)
static FILE* s_audioArchiveFile = nullptr;
static std::vector<HDArchiveEntry> s_audioEntries;
static bool s_audioArchiveOpen = false;
static bool s_audioArchiveTried = false;

static void ensureAudioArchiveOpen()
{
    if (s_audioArchiveTried) return;
    s_audioArchiveTried = true;

    FILE* f = fopen("audio.hda", "rb");
    if (!f) return;

    uint32_t magic = 0, version = 0, entryCount = 0;
    if (fread(&magic, 4, 1, f) != 1 ||
        fread(&version, 4, 1, f) != 1 ||
        fread(&entryCount, 4, 1, f) != 1)
    {
        fclose(f);
        return;
    }

    if (magic != 0x47424448 || version != 1)
    {
        printf("AudioArchive: bad magic/version (magic=0x%08X, ver=%u)\n", magic, version);
        fclose(f);
        return;
    }

    s_audioEntries.resize(entryCount);
    for (uint32_t i = 0; i < entryCount; i++)
    {
        uint16_t pathLen = 0;
        if (fread(&pathLen, 2, 1, f) != 1)
        {
            s_audioEntries.clear();
            fclose(f);
            return;
        }
        std::string path(pathLen, '\0');
        if (fread(&path[0], 1, pathLen, f) != pathLen)
        {
            s_audioEntries.clear();
            fclose(f);
            return;
        }
        uint64_t offset = 0, size = 0;
        if (fread(&offset, 8, 1, f) != 1 || fread(&size, 8, 1, f) != 1)
        {
            s_audioEntries.clear();
            fclose(f);
            return;
        }
        s_audioEntries[i].path = std::move(path);
        s_audioEntries[i].offset = offset;
        s_audioEntries[i].size = size;
    }

    s_audioArchiveFile = f;
    s_audioArchiveOpen = true;
    printf("AudioArchive: opened audio.hda (%u entries)\n", entryCount);
}

static const HDArchiveEntry* findAudioEntry(const char* name)
{
    if (!s_audioArchiveOpen) return nullptr;
    for (const auto& e : s_audioEntries)
    {
        if (_stricmp(e.path.c_str(), name) == 0)
            return &e;
    }
    return nullptr;
}

static unsigned char* readAudioEntry(const HDArchiveEntry* entry, size_t* outSize)
{
    if (!entry || !s_audioArchiveFile) return nullptr;
    size_t sz = (size_t)entry->size;
    unsigned char* buf = (unsigned char*)malloc(sz);
    if (!buf) return nullptr;
#ifdef _WIN32
    if (_fseeki64(s_audioArchiveFile, (long long)entry->offset, SEEK_SET) != 0)
#else
    if (fseeko(s_audioArchiveFile, (off_t)entry->offset, SEEK_SET) != 0)
#endif
    {
        free(buf);
        return nullptr;
    }
    if (fread(buf, 1, sz, s_audioArchiveFile) != sz)
    {
        free(buf);
        return nullptr;
    }
    if (outSize) *outSize = sz;
    return buf;
}

void closeAudioArchive()
{
    if (s_audioArchiveFile)
    {
        fclose(s_audioArchiveFile);
        s_audioArchiveFile = nullptr;
    }
    s_audioEntries.clear();
    s_audioArchiveOpen = false;
}

void osystemAL_init()
{
    gSoloud = new SoLoud::Soloud();
    gSoloud->init();
}

class ITD_AudioSource : public SoLoud::AudioSource
{
public:
    char* m_samples;
    int m_size;

    ITD_AudioSource(char* samplePtr, int size);
    virtual ~ITD_AudioSource();
    virtual SoLoud::AudioSourceInstance *createInstance();
};

class ITD_AudioInstance : public SoLoud::AudioSourceInstance
{
    ITD_AudioSource *mParent;
    unsigned int mOffset;
public:
    ITD_AudioInstance(ITD_AudioSource *aParent) : SoLoud::AudioSourceInstance()
    {
        mOffset = 0;
        mParent = aParent;
    }

    virtual unsigned int getAudio(float *aBuffer, unsigned int aSamplesToRead, unsigned int aBufferSize) override
    {
        if (mParent->m_size < 0 || (int)mOffset > mParent->m_size)
        {
            return 0;
        }

        unsigned int samplesWritten = 0;
        for (unsigned int i = 0; i < aSamplesToRead; i++)
        {
            if ((int)mOffset > mParent->m_size)
            {
                break;
            }
            aBuffer[i] = (((float)((unsigned char*)mParent->m_samples)[mOffset]) - 128.f) / 128.f;
            mOffset++;
            samplesWritten++;
        }
        return samplesWritten;
    }

    virtual bool hasEnded()
    {
        if (mParent->m_size < 0 || (int)mOffset > mParent->m_size)
        {
            return true;
        }

        return false;
    }
};

ITD_AudioSource::ITD_AudioSource(char* samplePtr, int size) : SoLoud::AudioSource()
{
    assert(samplePtr[26] == 1); //assert first block is of sound data type
    int sampleSize = (READ_LE_U32(samplePtr + 26) >> 8) - 2;

    int frequencyDiv = *(unsigned char*)(samplePtr + 30);
    //    int codecId = samplePtr[31];

    char* sampleData = samplePtr + 32;

    int sampleRate = 1000000 / (256 - frequencyDiv);

    m_samples = sampleData;
    m_size = sampleSize-1;

    mBaseSamplerate = sampleRate;
}

ITD_AudioSource::~ITD_AudioSource()
{
}

SoLoud::AudioSourceInstance* ITD_AudioSource::createInstance()
{
   return new ITD_AudioInstance(this);
}

// Track the last played SFX handle and source so we can stop/free them
static SoLoud::handle g_lastSfxHandle = 0;
static SoLoud::AudioSource* g_lastSfxSource = nullptr;

void osystem_playSample(char* samplePtr,int size)
{
    // Stop the previous sound effect to prevent overlapping/stuck sounds
    if (gSoloud && g_lastSfxHandle != 0)
    {
        gSoloud->stop(g_lastSfxHandle);
    }
    // Free the previous audio source to prevent memory leaks
    if (g_lastSfxSource)
    {
        delete g_lastSfxSource;
        g_lastSfxSource = nullptr;
    }

    if (g_gameId >= TIMEGATE)
    {
        SoLoud::Wav* pAudioSource = new SoLoud::Wav();
        pAudioSource->loadMem((u8*)samplePtr, size, true);
        g_lastSfxHandle = gSoloud->play(*pAudioSource);
        g_lastSfxSource = pAudioSource;
    }
    else
    {
        ITD_AudioSource* pAudioSource = new ITD_AudioSource(samplePtr, size);
        g_lastSfxHandle = gSoloud->play(*pAudioSource);
        g_lastSfxSource = pAudioSource;
    }
}

void osystem_stopSample()
{
    if (gSoloud && g_lastSfxHandle != 0)
    {
        gSoloud->stop(g_lastSfxHandle);
        g_lastSfxHandle = 0;
    }
    if (g_lastSfxSource)
    {
        delete g_lastSfxSource;
        g_lastSfxSource = nullptr;
    }
}

extern float gVolume;

void osystemAL_udpate()
{
    gSoloud->setGlobalVolume(gVolume);
}

SoLoud::DiskFile* pFile = NULL;
SoLoud::WavStream* pWavStream = NULL;
static unsigned char* s_musicArchiveBuf = nullptr;

int osystem_playTrack(int trackId)
{
    if (pWavStream)
    {
        pWavStream->stop();
    }

    // Free previous archive music buffer if any
    if (s_musicArchiveBuf)
    {
        free(s_musicArchiveBuf);
        s_musicArchiveBuf = nullptr;
    }

    // Try audio archive first
    ensureAudioArchiveOpen();
    if (s_audioArchiveOpen)
    {
        const char* extensions[] = { ".ogg", ".wav", ".mp3" };
        for (int i = 0; i < 3; i++)
        {
            char entryName[64];
            sprintf(entryName, "%02d%s", trackId, extensions[i]);
            const HDArchiveEntry* entry = findAudioEntry(entryName);
            if (entry)
            {
                size_t dataSize = 0;
                unsigned char* data = readAudioEntry(entry, &dataSize);
                if (data)
                {
                    pWavStream = new SoLoud::WavStream();
                    // aCopy=false, aTakeOwnership=false: we manage the buffer
                    SoLoud::result res = pWavStream->loadMem(data, (unsigned int)dataSize, false, false);
                    if (res == SoLoud::SO_NO_ERROR)
                    {
                        s_musicArchiveBuf = data;
                        gSoloud->play(*pWavStream);
                        return 0;
                    }
                    else
                    {
                        free(data);
                        delete pWavStream;
                        pWavStream = nullptr;
                    }
                }
            }
        }
    }

    // Filesystem fallback
    char filename[256];
    FILE* fHandle = NULL;
    
    // Try OGG first  
    sprintf(filename, "%02d.ogg", trackId);
    fHandle = fopen(filename, "rb");
    
    // Fall back to WAV if OGG not found
    if (fHandle == NULL)
    {
        sprintf(filename, "%02d.wav", trackId);
        fHandle = fopen(filename, "rb");
    }
    
    // Fall back to MP3 if WAV not found
    if (fHandle == NULL)
    {
        sprintf(filename, "%02d.mp3", trackId);
        fHandle = fopen(filename, "rb");
    }
    
    if (fHandle == NULL)
        return 0;

    pFile = new SoLoud::DiskFile(fHandle);
    pWavStream = new SoLoud::WavStream();
    pWavStream->loadFile(pFile);
    gSoloud->play(*pWavStream);

    return 0;
}

static SoLoud::Wav* g_lastNamedSfx = nullptr;
static SoLoud::handle g_lastNamedSfxHandle = 0;

void osystem_playSampleFromName(char* sampleName)
{
    if (!gSoloud || !soundEnabled)
        return;

    // Stop and free previous named sample
    if (g_lastNamedSfxHandle != 0)
    {
        gSoloud->stop(g_lastNamedSfxHandle);
        g_lastNamedSfxHandle = 0;
    }
    if (g_lastNamedSfx)
    {
        delete g_lastNamedSfx;
        g_lastNamedSfx = nullptr;
    }

    // Try audio archive first
    ensureAudioArchiveOpen();
    if (s_audioArchiveOpen)
    {
        const HDArchiveEntry* entry = findAudioEntry(sampleName);
        if (entry)
        {
            size_t dataSize = 0;
            unsigned char* data = readAudioEntry(entry, &dataSize);
            if (data)
            {
                SoLoud::Wav* pWav = new SoLoud::Wav();
                // aCopy=false, aTakeOwnership=true: SoLoud takes ownership of buffer
                SoLoud::result res = pWav->loadMem(data, (unsigned int)dataSize, false, true);
                if (res == SoLoud::SO_NO_ERROR)
                {
                    g_lastNamedSfxHandle = gSoloud->play(*pWav);
                    g_lastNamedSfx = pWav;
                    return;
                }
                else
                {
                    free(data);
                    delete pWav;
                }
            }
        }
    }

    // Filesystem fallback
    SoLoud::Wav* pWav = new SoLoud::Wav();
    SoLoud::result result = pWav->load(sampleName);

    if (result == SoLoud::SO_NO_ERROR)
    {
        g_lastNamedSfxHandle = gSoloud->play(*pWav);
        g_lastNamedSfx = pWav;
    }
    else
    {
        delete pWav;
    }
}