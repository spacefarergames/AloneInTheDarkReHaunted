///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Routes getEmbeddedFile() lookups to either the AITD1 or the
// "Jack in the Dark" embedded data registry based on the active remaster
// four-way game-mode setting (g_remasterConfig.gameData.mode).
///////////////////////////////////////////////////////////////////////////////

#include "embeddedData.h"
#include "../configRemaster.h"
#include <cstring>

bool getEmbeddedFile(const char* filename, const unsigned char** outData, size_t* outSize)
{
    switch (g_remasterConfig.gameData.mode)
    {
    case REMASTER_GAME_JACK:
        // In JACK mode, prefer JACK assets and fall back to AITD1 only for
        // shared helper assets (e.g. ControllerHint.png) that are not part
        // of the JACK data set.
        if (getEmbeddedJackFile(filename, outData, outSize))
            return true;
        return getEmbeddedAitd1File(filename, outData, outSize);
    case REMASTER_GAME_AITD2:
        if (getEmbeddedAitd2File(filename, outData, outSize)) return true;
        return strcmp(filename, "ControllerHint.png") == 0 && getEmbeddedAitd1File(filename, outData, outSize);
    case REMASTER_GAME_AITD3:
        if (getEmbeddedAitd3File(filename, outData, outSize)) return true;
        return strcmp(filename, "ControllerHint.png") == 0 && getEmbeddedAitd1File(filename, outData, outSize);
    default:
        return getEmbeddedAitd1File(filename, outData, outSize);
    }
}

bool preferEmbeddedGameData()
{
    return g_remasterConfig.gameData.mode != REMASTER_GAME_AITD1;
}
