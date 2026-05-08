///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark 2 - Jack Is Back Again
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Embedded data registry header
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstddef>

// Look up an embedded file by filename (case-insensitive, path-stripped).
// Returns true if found, with outData pointing to the embedded byte array
// and outSize set to its length. The data pointer is valid for the lifetime
// of the program (static storage).
//
// getEmbeddedFile() dispatches to either the AITD1 or the JACK (Jack in the
// Dark) embedded data set based on g_remasterConfig.gameData.jackMode, so
// callers don't need to care which game is active.
bool getEmbeddedFile(const char* filename, const unsigned char** outData, size_t* outSize);

// Game-specific lookups (auto-generated registries). Use these directly only
// when you need to bypass the active-mode dispatch above.
bool getEmbeddedAitd1File(const char* filename, const unsigned char** outData, size_t* outSize);
bool getEmbeddedJackFile (const char* filename, const unsigned char** outData, size_t* outSize);
