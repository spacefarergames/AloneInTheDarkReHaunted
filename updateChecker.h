///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Background update checker via GitHub Releases API
///////////////////////////////////////////////////////////////////////////////

#pragma once

// Current application version — bump this each release.
// Format: "major.minor.patch.build" e.g. "1.0.3.2103"
#define REHAUNTED_VERSION "1.0.3.2103"

// Launches a background thread that queries GitHub for the latest release.
// Safe to call early in init; does not block the caller.
void CheckForUpdatesAsync();
