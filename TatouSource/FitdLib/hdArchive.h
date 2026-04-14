///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// HD Background Archive - single-file container for all HD background images.
//
// Archive format (.hda) — version 2 (backward-compatible with v1):
//
//   Header:
//     4 bytes  - magic "HDBG"
//     4 bytes  - version (uint32: 1 or 2)
//     4 bytes  - entry count (uint32)
//
//   Entry table (repeated entry_count times):
//     2 bytes  - path length (uint16, NOT null-terminated in file)
//     N bytes  - relative path (e.g. "CAMERA00_000.png" or
//                "anim_CAMERA03_015/frame_001.png")
//     8 bytes  - data offset from start of file (uint64)
//     8 bytes  - data size in bytes (uint64)
//     [v2 only]:
//       8 bytes  - uncompressed size in bytes (uint64)
//       4 bytes  - flags (uint32, see HDArchiveEntryFlags)
//
//   Data section:
//     File bytes concatenated.  Each entry is either raw (v1/v2 uncompressed)
//     or zlib-compressed (v2 only, indicated by HDARCHIVE_FLAG_COMPRESSED).
//
//   v1 archives are read as-is: every entry is treated as uncompressed
//   (size == uncompressedSize, flags == 0).
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>

// Per-entry flags (stored as uint32 in v2 TOC)
enum HDArchiveEntryFlags : uint32_t
{
    HDARCHIVE_FLAG_NONE       = 0,
    HDARCHIVE_FLAG_COMPRESSED = 1 << 0,  // data is zlib-compressed
};

// Single entry in the archive table of contents
struct HDArchiveEntry
{
    std::string path;               // relative path inside archive (forward slashes)
    uint64_t    offset;             // byte offset of data from start of archive file
    uint64_t    size;               // stored (on-disk) size of data
    uint64_t    uncompressedSize;   // original size after decompression (== size when uncompressed)
    uint32_t    flags;              // HDArchiveEntryFlags bitmask
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
    // Uses a hash map for O(1) average lookup time.
    // Returns nullptr if not found.
    const HDArchiveEntry* findEntry(const char* relativePath);

    // Collect all entries whose path starts with a given directory prefix
    // (e.g. "anim_CAMERA03_015/").  Results are sorted alphabetically.
    std::vector<const HDArchiveEntry*> listByPrefix(const char* prefix);

    // Read the (uncompressed) bytes of an entry into a caller-supplied buffer.
    // |buf| must be at least entry->uncompressedSize bytes.
    // Transparent decompression is performed for compressed entries.
    // Returns true on success.
    bool readEntry(const HDArchiveEntry* entry, void* buf);

    // Convenience: allocate + read.  Caller must free() the returned pointer.
    // Sets outSize to the uncompressed number of bytes read.
    // Returns nullptr on failure.
    unsigned char* readEntryAlloc(const HDArchiveEntry* entry, size_t* outSize);
}

