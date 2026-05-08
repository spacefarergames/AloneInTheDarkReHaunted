///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Routes getEmbeddedFile() lookups to either the AITD1 or the
// "Jack in the Dark" embedded data registry based on the active remaster
// game-mode flag (g_remasterConfig.gameData.jackMode).
///////////////////////////////////////////////////////////////////////////////

#include "embeddedData.h"
#include "../configRemaster.h"

bool getEmbeddedFile(const char* filename, const unsigned char** outData, size_t* outSize)
{
    if (g_remasterConfig.gameData.jackMode)
    {
        // In JACK mode, prefer JACK assets and fall back to AITD1 only for
        // shared helper assets (e.g. ControllerHint.png) that are not part
        // of the JACK data set.
        if (getEmbeddedJackFile(filename, outData, outSize))
            return true;
        return getEmbeddedAitd1File(filename, outData, outSize);
    }
    return getEmbeddedAitd1File(filename, outData, outSize);
}
