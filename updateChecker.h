///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Background update checker via GitHub Releases API
///////////////////////////////////////////////////////////////////////////////

#pragma once

// Launches a background thread that queries GitHub for the latest release.
// The local version is read from "version.txt" in the game folder at check time.
// Safe to call early in init; does not block the caller.
void CheckForUpdatesAsync();
