///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// OpenAL audio system for sound effects and music
///////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <assert.h>
#include <new>
#include "consoleLog.h"

#ifdef _WIN32
#include <windows.h>
#pragma comment(lib, "winmm.lib")
#endif

//#include "OpenAL/al.h"
//#include "OpenAL/alc.h"

#include "soloud.h"
#include "soloud_file.h"
#include "soloud_wav.h"
#include "soloud_wavstream.h"

#include "common.h"
#include "osystemAL.h"
#include "hdArchive.h"
#include "vocDecoder.h"

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
        printf(AL_ERR "bad magic/version in audio archive (magic=0x%08X, ver=%u)" CON_RESET "\n", magic, version);
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
    printf(AL_OK "Opened audio.hda (%u entries)\n", entryCount);
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

// Allocate with new[] (not malloc) because SoLoud's MemoryFile uses delete[]
// when aTakeOwnership=true. Using malloc here would cause a malloc/delete[] mismatch.
static unsigned char* readAudioEntry(const HDArchiveEntry* entry, size_t* outSize)
{
    if (!entry || !s_audioArchiveFile) return nullptr;
    size_t sz = (size_t)entry->size;
    unsigned char* buf = new (std::nothrow) unsigned char[sz];
    if (!buf) return nullptr;
#ifdef _WIN32
    if (_fseeki64(s_audioArchiveFile, (long long)entry->offset, SEEK_SET) != 0)
#else
    if (fseeko(s_audioArchiveFile, (off_t)entry->offset, SEEK_SET) != 0)
#endif
    {
        delete[] buf;
        return nullptr;
    }
    if (fread(buf, 1, sz, s_audioArchiveFile) != sz)
    {
        delete[] buf;
        return nullptr;
    }
    if (outSize) *outSize = sz;
    return buf;
}

// Music stream state (declared here so closeAudioArchive can clean them up)
SoLoud::DiskFile* pFile = NULL;
SoLoud::WavStream* pWavStream = NULL;
static unsigned char* s_musicArchiveBuf = nullptr;

// Voice Over stream state
static SoLoud::WavStream* s_voStream = nullptr;
static SoLoud::DiskFile*  s_voFile   = nullptr;
static unsigned char*      s_voBuf    = nullptr;
static SoLoud::handle      s_voHandle = 0;

static void stopCDAudio();

void closeAudioArchive()
{
    // Stop CD audio if playing
    stopCDAudio();

    // Stop and clean up any active VO stream
    if (s_voStream)
    {
        if (gSoloud) gSoloud->stop(s_voHandle);
        s_voStream->stop();
        delete s_voStream;
        s_voStream = nullptr;
    }
    if (s_voFile)  { delete s_voFile;  s_voFile  = nullptr; }
    if (s_voBuf)   { delete[] s_voBuf; s_voBuf   = nullptr; }
    s_voHandle = 0;

    // Stop and clean up any active music stream before closing the archive.
    // The WavStream may be reading from s_musicArchiveBuf, so destroy it first.
    if (pWavStream)
    {
        pWavStream->stop();
        delete pWavStream;
        pWavStream = nullptr;
    }
    if (pFile)
    {
        delete pFile;
        pFile = nullptr;
    }
    if (s_musicArchiveBuf)
    {
        delete[] s_musicArchiveBuf;
        s_musicArchiveBuf = nullptr;
    }

    if (s_audioArchiveFile)
    {
        fclose(s_audioArchiveFile);
        s_audioArchiveFile = nullptr;
    }
    s_audioEntries.clear();
    s_audioArchiveOpen = false;
    s_audioArchiveTried = false;
}

// ── CD Audio (ALONECD) support ─────────────────────────────────────────────
#ifdef _WIN32
static bool  s_cdDetected    = false;
static bool  s_cdDetectDone  = false;
static char  s_cdDriveLetter = '\0';
static bool  s_cdPlaying     = false;

// Scan all CD/DVD drives for a volume labelled "ALONECD".
static void detectAloneCD()
{
    if (s_cdDetectDone) return;
    s_cdDetectDone = true;

    DWORD drives = GetLogicalDrives();
    for (int i = 0; i < 26; i++)
    {
        if (!(drives & (1u << i))) continue;

        char root[] = { (char)('A' + i), ':', '\\', '\0' };
        if (GetDriveTypeA(root) != DRIVE_CDROM) continue;

        char volName[MAX_PATH + 1] = {};
        if (GetVolumeInformationA(root, volName, sizeof(volName),
                                   NULL, NULL, NULL, NULL, 0))
        {
            if (_stricmp(volName, "ALONECD") == 0)
            {
                s_cdDriveLetter = (char)('A' + i);
                s_cdDetected = true;
                printf(AL_OK "Detected AITD1 CD (ALONECD) on drive %c:\\\n", s_cdDriveLetter);
                return;
            }
        }
    }
}

static void stopCDAudio()
{
    if (s_cdPlaying)
    {
        mciSendStringA("stop cdaudio", NULL, 0, NULL);
        mciSendStringA("close cdaudio", NULL, 0, NULL);
        s_cdPlaying = false;
    }
}

// Play a CD audio track via MCI.  Returns true on success.
static bool playCDAudioTrack(int trackId)
{
    detectAloneCD();
    if (!s_cdDetected) return false;

    stopCDAudio();

    // Open the CD-ROM drive
    char cmd[256];
    char ret[128];
    sprintf(cmd, "open %c: type cdaudio alias cdaudio", s_cdDriveLetter);
    if (mciSendStringA(cmd, NULL, 0, NULL) != 0)
        return false;

    // Set time format to track/minute/second/frame
    if (mciSendStringA("set cdaudio time format tmsf", NULL, 0, NULL) != 0)
    {
        mciSendStringA("close cdaudio", NULL, 0, NULL);
        return false;
    }

    // Verify the track exists by querying the number of tracks
    if (mciSendStringA("status cdaudio number of tracks", ret, sizeof(ret), NULL) != 0)
    {
        mciSendStringA("close cdaudio", NULL, 0, NULL);
        return false;
    }
    int numTracks = atoi(ret);
    if (trackId < 1 || trackId > numTracks)
    {
        mciSendStringA("close cdaudio", NULL, 0, NULL);
        return false;
    }

    // Play the requested track (to the start of the next track)
    if (trackId < numTracks)
        sprintf(cmd, "play cdaudio from %d to %d", trackId, trackId + 1);
    else
        sprintf(cmd, "play cdaudio from %d", trackId);

    if (mciSendStringA(cmd, NULL, 0, NULL) != 0)
    {
        mciSendStringA("close cdaudio", NULL, 0, NULL);
        return false;
    }

    s_cdPlaying = true;
    printf(AL_TAG "Playing CD audio track %d from drive %c:\\\n", trackId, s_cdDriveLetter);
    return true;
}
#else
static void stopCDAudio() {}
static bool playCDAudioTrack(int) { return false; }
#endif

void osystemAL_init()
{
    gSoloud = new SoLoud::Soloud();
    gSoloud->init();
}

void osystemAL_deinit()
{
    // Stop VO playback
    if (s_voStream)
    {
        if (gSoloud) gSoloud->stop(s_voHandle);
        s_voStream->stop();
        delete s_voStream;
        s_voStream = nullptr;
    }
    if (s_voFile)  { delete s_voFile;  s_voFile  = nullptr; }
    if (s_voBuf)   { delete[] s_voBuf; s_voBuf   = nullptr; }
    s_voHandle = 0;

    stopCDAudio();

    if (gSoloud)
    {
        gSoloud->deinit();
        delete gSoloud;
        gSoloud = nullptr;
    }
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
    if (!gSoloud)
        return;

    // Stop the previous sound effect to prevent overlapping/stuck sounds
    if (g_lastSfxHandle != 0)
    {
        gSoloud->stop(g_lastSfxHandle);
        g_lastSfxHandle = 0;
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
        SoLoud::result res = pAudioSource->loadMem((u8*)samplePtr, size, true);
        if (res != SoLoud::SO_NO_ERROR)
        {
            delete pAudioSource;
            return;
        }
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
    if (gSoloud)
        gSoloud->setGlobalVolume(gVolume);
}


int osystem_playTrack(int trackId)
{
    if (!gSoloud)
        return 0;

    // Delete the previous WavStream BEFORE freeing the archive buffer.
    // WavStream::loadMem with aCopy=false means SoLoud's audio mixing thread
    // reads directly from s_musicArchiveBuf. We must fully destroy the stream
    // (which stops the audio thread's access) before freeing the buffer,
    // otherwise we get a use-after-free access violation.
    if (pWavStream)
    {
        pWavStream->stop();
        delete pWavStream;
        pWavStream = nullptr;
    }

    // Clean up previous DiskFile (used by filesystem fallback path)
    if (pFile)
    {
        delete pFile;
        pFile = nullptr;
    }

    // Now safe to free the archive buffer - no audio thread references remain
    if (s_musicArchiveBuf)
    {
        delete[] s_musicArchiveBuf;
        s_musicArchiveBuf = nullptr;
    }

    // Stop any active CD audio before switching tracks
    stopCDAudio();

    // Try CD audio first (ALONECD in drive)
    if (playCDAudioTrack(trackId))
        return 1;

    // MP3/audio files are numbered from 01 for the first audio track, but trackId
    // uses CD track numbers where track 1 is the data track and audio starts at 2.
    // Subtract 1 so CD track 2 (first audio) maps to "01.mp3", etc.
    int fileTrackId = trackId - 1;
    if (fileTrackId < 1)
        return 0;

    // Try audio archive next
    ensureAudioArchiveOpen();
    if (s_audioArchiveOpen)
    {
        const char* extensions[] = { ".ogg", ".wav", ".mp3" };
        for (int i = 0; i < 3; i++)
        {
            char entryName[64];
            sprintf(entryName, "%02d%s", fileTrackId, extensions[i]);
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
                        return 1;
                    }
                    else
                    {
                        delete[] data;
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
    sprintf(filename, "%02d.ogg", fileTrackId);
    fHandle = fopen(filename, "rb");

    // Fall back to WAV if OGG not found
    if (fHandle == NULL)
    {
        sprintf(filename, "%02d.wav", fileTrackId);
        fHandle = fopen(filename, "rb");
    }

    // Fall back to MP3 if WAV not found
    if (fHandle == NULL)
    {
        sprintf(filename, "%02d.mp3", fileTrackId);
        fHandle = fopen(filename, "rb");
    }
    
    if (fHandle == NULL)
        return 0;

    pFile = new SoLoud::DiskFile(fHandle);
    pWavStream = new SoLoud::WavStream();
    SoLoud::result res = pWavStream->loadFile(pFile);
    if (res == SoLoud::SO_NO_ERROR)
    {
        gSoloud->play(*pWavStream);
        return 1;
    }
    else
    {
        delete pWavStream;
        pWavStream = nullptr;
        delete pFile;
        pFile = nullptr;
    }

    return 0;
}

// ── Voice Over (VO) playback ───────────────────────────────────────────────

void osystem_stopVO()
{
    if (!gSoloud) return;

    if (s_voHandle != 0)
    {
        gSoloud->stop(s_voHandle);
        s_voHandle = 0;
    }
    if (s_voStream) { s_voStream->stop(); delete s_voStream; s_voStream = nullptr; }
    if (s_voFile)   { delete s_voFile;   s_voFile   = nullptr; }
    if (s_voBuf)    { delete[] s_voBuf;  s_voBuf    = nullptr; }
}

// Try to decode a VOC buffer and play it.  Returns true on success.
static bool tryPlayVocFromMemory(const unsigned char* data, size_t dataSize, const char* label)
{
    unsigned char* wavBuf = nullptr;
    size_t wavSize = 0;
    if (!vocDecodeToWav(data, dataSize, &wavBuf, &wavSize))
        return false;

    s_voStream = new SoLoud::WavStream();
    SoLoud::result res = s_voStream->loadMem(wavBuf, (unsigned int)wavSize, false, false);
    if (res == SoLoud::SO_NO_ERROR)
    {
        s_voBuf = wavBuf;
        s_voHandle = gSoloud->play(*s_voStream);
        printf(VO_OK "Playing VO: %s (VOC decoded)" CON_RESET "\n", label);
        return true;
    }

    delete[] wavBuf;
    delete s_voStream;
    s_voStream = nullptr;
    return false;
}

// Check whether a path ends with a given extension (case-insensitive).
static bool hasExtCI(const char* path, const char* ext)
{
    size_t pl = strlen(path);
    size_t el = strlen(ext);
    if (el == 0 || pl < el) return false;
    return _stricmp(path + pl - el, ext) == 0;
}

// Try to open a file by path, decode VOC if needed, and play it.
// Returns true on success.
static bool tryPlayVoFile(const char* path, const char* label)
{
    FILE* fh = fopen(path, "rb");
    if (!fh) return false;

    // VOC files need special decoding — read fully into memory first
    if (hasExtCI(path, ".voc"))
    {
        fseek(fh, 0, SEEK_END);
        long fileLen = ftell(fh);
        fseek(fh, 0, SEEK_SET);
        if (fileLen <= 0) { fclose(fh); return false; }

        unsigned char* raw = new (std::nothrow) unsigned char[(size_t)fileLen];
        if (!raw) { fclose(fh); return false; }
        if (fread(raw, 1, (size_t)fileLen, fh) != (size_t)fileLen)
        {
            delete[] raw;
            fclose(fh);
            return false;
        }
        fclose(fh);

        bool ok = tryPlayVocFromMemory(raw, (size_t)fileLen, label);
        delete[] raw;
        return ok;
    }

    // Standard audio formats — let SoLoud handle directly
    s_voFile   = new SoLoud::DiskFile(fh);
    s_voStream = new SoLoud::WavStream();
    SoLoud::result res = s_voStream->loadFile(s_voFile);
    if (res == SoLoud::SO_NO_ERROR)
    {
        s_voHandle = gSoloud->play(*s_voStream);
        printf(VO_OK "Playing VO: %s" CON_RESET "\n", label);
        return true;
    }

    delete s_voStream; s_voStream = nullptr;
    delete s_voFile;   s_voFile   = nullptr;
    return false;
}

// Build a VOC filename from a numeric index and the current language.
// AITD1 CD naming convention: <LANG><index>.VOC  (e.g. ENG001.VOC)
static const char* getVocLanguagePrefix()
{
    if (languageNameString == "FRANCAIS") return "FRE";
    if (languageNameString == "ITALIANO") return "ITA";
    if (languageNameString == "ESPAGNOL") return "ESP";
    if (languageNameString == "DEUTSCH")  return "DEU";
    return "ENG"; // ENGLISH, USA, TEXTES all map to English
}

void osystem_playVocByIndex(int vocIndex)
{
    if (!gSoloud || vocIndex < 0)
        return;

    char vocName[64];
    sprintf(vocName, "%s%03d.VOC", getVocLanguagePrefix(), vocIndex);
    printf(VO_TAG "Playing VOC by index: %d -> %s\n", vocIndex, vocName);
    osystem_playVO(vocName);
}

void osystem_playVO(const char* voFileName)
{
    if (!gSoloud || !voFileName || !voFileName[0])
        return;

    // Stop any currently playing VO
    osystem_stopVO();

    // Try audio archive first
    ensureAudioArchiveOpen();
    if (s_audioArchiveOpen)
    {
        // Try the name as-is and with common extensions (including .voc)
        const char* arcExts[] = { "", ".voc", ".ogg", ".wav", ".mp3" };
        for (int i = 0; i < 5; i++)
        {
            char entryName[512];
            sprintf(entryName, "%s%s", voFileName, arcExts[i]);
            const HDArchiveEntry* entry = findAudioEntry(entryName);
            if (!entry) continue;

            size_t dataSize = 0;
            unsigned char* data = readAudioEntry(entry, &dataSize);
            if (!data) continue;

            // Check if this is a VOC file (by extension or magic)
            if (hasExtCI(entryName, ".voc") ||
                (dataSize >= 20 && memcmp(data, "Creative Voice File\x1a", 20) == 0))
            {
                bool ok = tryPlayVocFromMemory(data, dataSize, entryName);
                delete[] data;
                if (ok)
                {
                    printf(VO_OK "Playing VO: %s (from archive, VOC decoded)" CON_RESET "\n", entryName);
                    return;
                }
                continue;
            }

            // Standard format — load directly
            s_voStream = new SoLoud::WavStream();
            SoLoud::result res = s_voStream->loadMem(data, (unsigned int)dataSize, false, false);
            if (res == SoLoud::SO_NO_ERROR)
            {
                s_voBuf = data;
                s_voHandle = gSoloud->play(*s_voStream);
                printf(VO_OK "Playing VO: %s (from archive)" CON_RESET "\n", entryName);
                return;
            }

            delete[] data;
            delete s_voStream;
            s_voStream = nullptr;
        }
    }

    // Extension list for filesystem searches (includes .voc)
    const char* exts[] = { "", ".voc", ".ogg", ".wav", ".mp3" };

    // Try ALONECD INDARK subfolder (e.g. D:\INDARK\voFileName)
#ifdef _WIN32
    detectAloneCD();
    if (s_cdDetected)
    {
        for (int i = 0; i < 5; i++)
        {
            char cdPath[512];
            sprintf(cdPath, "%c:\\INDARK\\%s%s", s_cdDriveLetter, voFileName, exts[i]);
            if (tryPlayVoFile(cdPath, cdPath))
                return;
        }
    }
#endif

    // Filesystem fallback
    for (int i = 0; i < 5; i++)
    {
        char path[512];
        sprintf(path, "%s%s", voFileName, exts[i]);
        if (tryPlayVoFile(path, path))
            return;
    }

    printf(VO_WARN "VO file not found: %s" CON_RESET "\n", voFileName);
}

bool osystem_isVOPlaying()
{
    if (!gSoloud || s_voHandle == 0)
        return false;

    return gSoloud->isValidVoiceHandle(s_voHandle);
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
                    // Don't delete[] data -- Wav::loadMem's stack-local MemoryFile
                    // already freed it (aTakeOwnership=true)
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