///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Creative Voice File (.VOC) decoder
// Converts VOC audio data into a WAV buffer that SoLoud can load directly.
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <cstddef>

// Decode a Creative Voice File (.VOC) from memory into a standard WAV buffer.
// Input:  vocData / vocSize — raw bytes of the .VOC file
// Output: outWavData — newly allocated buffer (caller must delete[])
//         outWavSize — size of the WAV buffer in bytes
// Returns true on success, false if the data is not a valid VOC file.
bool vocDecodeToWav(const unsigned char* vocData, size_t vocSize,
                    unsigned char** outWavData, size_t* outWavSize);
