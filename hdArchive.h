///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// HD Background Archive - single-file container for all HD background images.
//
// Archive format (.hda):
//   Header:
//     4 bytes  - magic "HDBG"
//     4 bytes  - version (uint32, currently 1)
//     4 bytes  - entry count (uint32)
//   Entry table (repeated entry_count times):
//     2 bytes  - path length (uint16, NOT null-terminated in file)
//     N bytes  - relative path (e.g. "CAMERA00_000.png" or
//                "anim_CAMERA03_015/frame_001.png")
//     8 bytes  - data offset from start of file (uint64)
//     8 bytes  - data size in bytes (uint64)
//   Data section:
//     Raw file bytes concatenated (PNG/TGA/BMP as-is, no re-compression)
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>

// Single entry in the archive table of contents
struct HDArchiveEntry
{
    std::string path;       // relative path inside archive (forward slashes)
    uint64_t    offset;     // byte offset of data from start of archive file
    uint64_t    size;       // byte length of data
};

// Opens the archive, reads the TOC, provides lookup / read helpers.
// Only one archive is ever open (singleton-style, matches the single
// backgrounds_hd folder it replaces).
namespace HDArchive
{
    // Open an archive file and cache its TOC.
    // Returns true on success.  Subsequent calls close the previous archive.
    bool open(const char* archivePath);

    // Close the archive and free cached TOC.
    void close();

    // Is an archive currently open?
    bool isOpen();

    // Look up a single entry by exact relative path (case-insensitive).
    // Returns nullptr if not found.
    const HDArchiveEntry* findEntry(const char* relativePath);

    // Collect all entries whose path starts with a given directory prefix
    // (e.g. "anim_CAMERA03_015/").  Results are sorted alphabetically.
    std::vector<const HDArchiveEntry*> listByPrefix(const char* prefix);

    // Read the raw bytes of an entry into a caller-supplied buffer.
    // |buf| must be at least entry->size bytes.  Returns true on success.
    bool readEntry(const HDArchiveEntry* entry, void* buf);

    // Convenience: allocate + read.  Caller must free() the returned pointer.
    // Sets outSize to the number of bytes read.  Returns nullptr on failure.
    unsigned char* readEntryAlloc(const HDArchiveEntry* entry, size_t* outSize);
}

