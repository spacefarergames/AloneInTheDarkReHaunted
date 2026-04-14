///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Auto-detect and copy original AITD game data (PAK/ITD files) from
// Steam, GOG, or CD-ROM into the game's working directory.
///////////////////////////////////////////////////////////////////////////////

#ifndef _GAME_DATA_COPY_H_
#define _GAME_DATA_COPY_H_

// Check whether the required PAK/ITD game data files are present in the
// working directory.  If not, attempt to locate them from (in order):
//   1. Steam library folders  (app 548090 — Alone in the Dark 1)
//   2. GOG install path       (game ID 1207660923)
//   3. CD/DVD drives          (INDARK subfolder or root)
//
// Returns true if the data files are already present or were successfully
// copied.  Returns false if no source was found (the game can still fall
// back to embedded data).
//
// Call early in startup, before any PAK/ITD access.
bool gameDataCopy_EnsureDataFiles();

#endif // _GAME_DATA_COPY_H_
