///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Automatic original-PAK background dumping (debug/asset extraction)
///////////////////////////////////////////////////////////////////////////////

#pragma once

// Iterate every floor / camera entry from the original PAK files and dump
// each 320x200 paletted background as a PNG into <homePath>backgrounds_dump/.
// Safe to call once at startup; no-op if no backgrounds are found.
void dumpAllBackgrounds();
