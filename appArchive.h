///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// App archive — single-file container for bundled application assets
// (UI images, fonts, etc.) that ship alongside the executable.
//
// Uses the same HDBG archive format as backgrounds_hd.hda and audio.hda.
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <cstddef>
#include "hdArchive.h"

// AppArchive: separate singleton reader for app.hda.
// Coexists with HDArchive (backgrounds) and the audio archive.
namespace AppArchive
{
    // Open app.hda (searched in the current working directory).
    // Returns true on success.  Safe to call multiple times — only opens once.
    bool open(const char* archivePath = "app.hda");

    // Close the archive and free cached TOC.
    void close();

    // Is the archive currently open?
    bool isOpen();

    // Look up an entry by exact relative path (case-insensitive).
    // Returns nullptr if not found.
    const HDArchiveEntry* findEntry(const char* relativePath);

    // Allocate + read an entry's raw bytes.
    // Sets *outSize to the byte count.  Caller must free() the pointer.
    // Returns nullptr on failure.
    unsigned char* readEntryAlloc(const HDArchiveEntry* entry, size_t* outSize);
}
