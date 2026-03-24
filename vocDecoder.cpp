///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Creative Voice File (.VOC) decoder implementation
//
// VOC format (v1.10+):
//   Header (26 bytes):
//     20 bytes  "Creative Voice File\x1a"
//      2 bytes  offset to first data block (little-endian)
//      2 bytes  version  (e.g. 0x010A = 1.10)
//      2 bytes  checksum (~version + 0x1234)
//
//   Data blocks (repeated until type 0):
//     1 byte   block type
//     3 bytes  block payload size (little-endian, NOT including the 4-byte header)
//
//     Type 0 — Terminator (no size bytes)
//     Type 1 — Sound Data:  1 byte freq divisor, 1 byte codec, then PCM
//              sample_rate = 1000000 / (256 - freq_divisor)
//              codec 0 = 8-bit unsigned PCM
//     Type 2 — Sound Data Continuation: raw PCM (same format as previous type 1)
//     Type 9 — Sound Data (new): 4-byte sample rate, 1 byte bits, 1 byte channels,
//              2 bytes codec, 4 bytes reserved, then PCM
//
//   This decoder handles types 0, 1, 2, and 9 which covers all AITD-era VOC files.
///////////////////////////////////////////////////////////////////////////////

#include "vocDecoder.h"
#include "consoleLog.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>

static const char VOC_MAGIC[] = "Creative Voice File\x1a";
static const size_t VOC_MAGIC_LEN = 20;
static const size_t VOC_HEADER_SIZE = 26;

static uint16_t readU16LE(const unsigned char* p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t readU24LE(const unsigned char* p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
}

static uint32_t readU32LE(const unsigned char* p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void writeU16LE(unsigned char* p, uint16_t v)
{
    p[0] = (unsigned char)(v & 0xFF);
    p[1] = (unsigned char)((v >> 8) & 0xFF);
}

static void writeU32LE(unsigned char* p, uint32_t v)
{
    p[0] = (unsigned char)(v & 0xFF);
    p[1] = (unsigned char)((v >> 8) & 0xFF);
    p[2] = (unsigned char)((v >> 16) & 0xFF);
    p[3] = (unsigned char)((v >> 24) & 0xFF);
}

bool vocDecodeToWav(const unsigned char* vocData, size_t vocSize,
                    unsigned char** outWavData, size_t* outWavSize)
{
    if (!vocData || !outWavData || !outWavSize)
        return false;

    *outWavData = nullptr;
    *outWavSize = 0;

    // Validate header
    if (vocSize < VOC_HEADER_SIZE)
        return false;

    if (memcmp(vocData, VOC_MAGIC, VOC_MAGIC_LEN) != 0)
        return false;

    uint16_t dataOffset = readU16LE(vocData + 20);
    if (dataOffset < VOC_HEADER_SIZE || dataOffset > vocSize)
        return false;

    // Walk the block chain and collect PCM samples
    std::vector<unsigned char> pcmData;
    pcmData.reserve(vocSize); // rough estimate

    uint32_t sampleRate = 0;
    uint16_t bitsPerSample = 8;
    uint16_t numChannels = 1;
    bool gotSoundInfo = false;

    size_t pos = dataOffset;

    while (pos < vocSize)
    {
        uint8_t blockType = vocData[pos];

        // Type 0 — terminator (no size bytes)
        if (blockType == 0)
            break;

        // All other block types have a 3-byte size after the type byte
        if (pos + 4 > vocSize)
            break;

        uint32_t blockSize = readU24LE(vocData + pos + 1);
        size_t blockDataStart = pos + 4;

        if (blockDataStart + blockSize > vocSize)
        {
            printf(VO_WARN "VOC block type %d truncated at offset %zu" CON_RESET "\n",
                   blockType, pos);
            break;
        }

        switch (blockType)
        {
        case 1: // Sound Data
        {
            if (blockSize < 2) break;

            uint8_t freqDiv = vocData[blockDataStart];
            uint8_t codec   = vocData[blockDataStart + 1];

            if (codec != 0)
            {
                printf(VO_WARN "VOC unsupported codec %d in type 1 block" CON_RESET "\n", codec);
                break;
            }

            if (!gotSoundInfo)
            {
                sampleRate = 1000000 / (256 - (uint32_t)freqDiv);
                bitsPerSample = 8;
                numChannels = 1;
                gotSoundInfo = true;
            }

            const unsigned char* pcm = vocData + blockDataStart + 2;
            size_t pcmLen = blockSize - 2;
            pcmData.insert(pcmData.end(), pcm, pcm + pcmLen);
            break;
        }
        case 2: // Sound Data Continuation (same format as previous type 1)
        {
            const unsigned char* pcm = vocData + blockDataStart;
            pcmData.insert(pcmData.end(), pcm, pcm + blockSize);
            break;
        }
        case 9: // Sound Data (new format)
        {
            if (blockSize < 12) break;

            uint32_t sr     = readU32LE(vocData + blockDataStart);
            uint8_t  bits   = vocData[blockDataStart + 4];
            uint8_t  chans  = vocData[blockDataStart + 5];
            uint16_t codec  = readU16LE(vocData + blockDataStart + 6);
            // 4 bytes reserved at +8

            if (codec != 0)
            {
                printf(VO_WARN "VOC unsupported codec %d in type 9 block" CON_RESET "\n", codec);
                break;
            }

            if (!gotSoundInfo)
            {
                sampleRate = sr;
                bitsPerSample = bits;
                numChannels = chans;
                gotSoundInfo = true;
            }

            const unsigned char* pcm = vocData + blockDataStart + 12;
            size_t pcmLen = blockSize - 12;
            pcmData.insert(pcmData.end(), pcm, pcm + pcmLen);
            break;
        }
        default:
            // Skip unknown block types (type 3=silence, 4=marker, 5=text, 6/7=repeat, 8=extended)
            break;
        }

        pos = blockDataStart + blockSize;
    }

    if (!gotSoundInfo || pcmData.empty())
    {
        printf(VO_WARN "VOC file contained no playable audio data" CON_RESET "\n");
        return false;
    }

    // Build a WAV file in memory
    // WAV header: 44 bytes for standard PCM format
    uint32_t dataSize = (uint32_t)pcmData.size();
    uint32_t wavSize  = 44 + dataSize;
    uint16_t blockAlign = numChannels * (bitsPerSample / 8);
    uint32_t byteRate   = sampleRate * blockAlign;

    unsigned char* wav = new (std::nothrow) unsigned char[wavSize];
    if (!wav)
        return false;

    // RIFF header
    memcpy(wav, "RIFF", 4);
    writeU32LE(wav + 4, wavSize - 8);
    memcpy(wav + 8, "WAVE", 4);

    // fmt chunk
    memcpy(wav + 12, "fmt ", 4);
    writeU32LE(wav + 16, 16);               // chunk size
    writeU16LE(wav + 20, 1);                // PCM format
    writeU16LE(wav + 22, numChannels);
    writeU32LE(wav + 24, sampleRate);
    writeU32LE(wav + 28, byteRate);
    writeU16LE(wav + 32, blockAlign);
    writeU16LE(wav + 34, bitsPerSample);

    // data chunk
    memcpy(wav + 36, "data", 4);
    writeU32LE(wav + 40, dataSize);
    memcpy(wav + 44, pcmData.data(), dataSize);

    *outWavData = wav;
    *outWavSize = wavSize;
    return true;
}
