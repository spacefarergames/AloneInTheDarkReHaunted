///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// HD Background Archive implementation
///////////////////////////////////////////////////////////////////////////////

#include "hdArchive.h"
#include "consoleLog.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <cctype>
#include <unordered_map>

#include "zlib.h"

// Archive magic & supported versions
static const uint32_t HDARCHIVE_MAGIC   = 0x47424448; // "HDBG" as little-endian uint32
static const uint32_t HDARCHIVE_VERSION_1 = 1;
static const uint32_t HDARCHIVE_VERSION_2 = 2;

// Internal state (singleton)
static FILE*                          s_file    = nullptr;
static std::vector<HDArchiveEntry>    s_entries;
static bool                           s_open    = false;

// Hash map for O(1) case-insensitive lookup by path
static std::unordered_map<std::string, size_t> s_pathIndex; // lowered path -> index in s_entries

// Convert a string to lowercase (ASCII) in-place and return it
static std::string toLowerASCII(const char* str)
{
    std::string result;
    result.reserve(strlen(str));
    while (*str)
    {
        result += (char)tolower((unsigned char)*str);
        ++str;
    }
    return result;
}

// Case-insensitive prefix check
static bool strStartsWithCI(const char* str, const char* prefix)
{
    while (*prefix)
    {
        if (tolower((unsigned char)*str) != tolower((unsigned char)*prefix))
            return false;
        ++str;
        ++prefix;
    }
    return true;
}

namespace HDArchive
{

bool open(const char* archivePath)
{
    close(); // close any previous archive

    FILE* f = fopen(archivePath, "rb");
    if (!f)
    {
        return false;
    }

    // Read header
    uint32_t magic = 0, version = 0, entryCount = 0;
    if (fread(&magic, 4, 1, f) != 1 ||
        fread(&version, 4, 1, f) != 1 ||
        fread(&entryCount, 4, 1, f) != 1)
    {
        fclose(f);
        return false;
    }

    if (magic != HDARCHIVE_MAGIC)
    {
        printf(HDAR_ERR "bad magic in %s (magic=0x%08X)" CON_RESET "\n",
               archivePath, magic);
        fclose(f);
        return false;
    }

    if (version != HDARCHIVE_VERSION_1 && version != HDARCHIVE_VERSION_2)
    {
        printf(HDAR_ERR "unsupported version %u in %s" CON_RESET "\n",
               version, archivePath);
        fclose(f);
        return false;
    }

    const bool isV2 = (version >= HDARCHIVE_VERSION_2);

    // Read TOC
    s_entries.resize(entryCount);
    for (uint32_t i = 0; i < entryCount; i++)
    {
        uint16_t pathLen = 0;
        if (fread(&pathLen, 2, 1, f) != 1)
        {
            s_entries.clear();
            fclose(f);
            return false;
        }

        std::string path(pathLen, '\0');
        if (fread(&path[0], 1, pathLen, f) != pathLen)
        {
            s_entries.clear();
            fclose(f);
            return false;
        }

        uint64_t offset = 0, size = 0;
        if (fread(&offset, 8, 1, f) != 1 || fread(&size, 8, 1, f) != 1)
        {
            s_entries.clear();
            fclose(f);
            return false;
        }

        uint64_t uncompressedSize = size;
        uint32_t flags = HDARCHIVE_FLAG_NONE;

        if (isV2)
        {
            if (fread(&uncompressedSize, 8, 1, f) != 1 ||
                fread(&flags, 4, 1, f) != 1)
            {
                s_entries.clear();
                fclose(f);
                return false;
            }
        }

        s_entries[i].path             = std::move(path);
        s_entries[i].offset           = offset;
        s_entries[i].size             = size;
        s_entries[i].uncompressedSize = uncompressedSize;
        s_entries[i].flags            = flags;
    }

    // Sort entries by path for deterministic prefix searches
    std::sort(s_entries.begin(), s_entries.end(),
              [](const HDArchiveEntry& a, const HDArchiveEntry& b)
              {
                  return a.path < b.path;
              });

    // Build case-insensitive hash map for O(1) lookup
    s_pathIndex.clear();
    s_pathIndex.reserve(entryCount);
    for (size_t i = 0; i < s_entries.size(); i++)
    {
        s_pathIndex[toLowerASCII(s_entries[i].path.c_str())] = i;
    }

    s_file = f;
    s_open = true;

    if (isV2)
    {
        // Count compressed entries for the log message
        uint32_t compressedCount = 0;
        for (const auto& e : s_entries)
        {
            if (e.flags & HDARCHIVE_FLAG_COMPRESSED)
                compressedCount++;
        }
        printf(HDAR_OK "Opened %s v%u (%u entries, %u compressed)\n",
               archivePath, version, entryCount, compressedCount);
    }
    else
    {
        printf(HDAR_OK "Opened %s v%u (%u entries)\n",
               archivePath, version, entryCount);
    }

    return true;
}

void close()
{
    if (s_file)
    {
        fclose(s_file);
        s_file = nullptr;
    }
    s_entries.clear();
    s_pathIndex.clear();
    s_open = false;
}

bool isOpen()
{
    return s_open;
}

const HDArchiveEntry* findEntry(const char* relativePath)
{
    if (!s_open)
        return nullptr;

    std::string key = toLowerASCII(relativePath);
    auto it = s_pathIndex.find(key);
    if (it != s_pathIndex.end())
        return &s_entries[it->second];

    return nullptr;
}

std::vector<const HDArchiveEntry*> listByPrefix(const char* prefix)
{
    std::vector<const HDArchiveEntry*> result;
    if (!s_open)
        return result;

    for (const auto& e : s_entries)
    {
        if (strStartsWithCI(e.path.c_str(), prefix))
        {
            result.push_back(&e);
        }
    }

    // Sort alphabetically by path (entries are already sorted, but be safe)
    std::sort(result.begin(), result.end(),
              [](const HDArchiveEntry* a, const HDArchiveEntry* b)
              {
                  return a->path < b->path;
              });

    return result;
}

bool readEntry(const HDArchiveEntry* entry, void* buf)
{
    if (!s_open || !entry || !buf || !s_file)
        return false;

#ifdef _WIN32
    if (_fseeki64(s_file, (long long)entry->offset, SEEK_SET) != 0)
        return false;
#else
    if (fseeko(s_file, (off_t)entry->offset, SEEK_SET) != 0)
        return false;
#endif

    if (entry->flags & HDARCHIVE_FLAG_COMPRESSED)
    {
        // Read compressed data into a temporary buffer, then decompress
        unsigned char* compBuf = (unsigned char*)malloc((size_t)entry->size);
        if (!compBuf)
            return false;

        if (fread(compBuf, 1, (size_t)entry->size, s_file) != (size_t)entry->size)
        {
            free(compBuf);
            return false;
        }

        uLongf destLen = (uLongf)entry->uncompressedSize;
        int zrc = uncompress((Bytef*)buf, &destLen,
                             (const Bytef*)compBuf, (uLong)entry->size);
        free(compBuf);

        if (zrc != Z_OK)
        {
            printf(HDAR_ERR "zlib uncompress failed for '%s' (rc=%d)" CON_RESET "\n",
                   entry->path.c_str(), zrc);
            return false;
        }

        return true;
    }

    // Uncompressed: read directly into caller buffer
    return fread(buf, 1, (size_t)entry->size, s_file) == (size_t)entry->size;
}

unsigned char* readEntryAlloc(const HDArchiveEntry* entry, size_t* outSize)
{
    if (!entry)
        return nullptr;

    size_t sz = (size_t)entry->uncompressedSize;
    unsigned char* buf = (unsigned char*)malloc(sz);
    if (!buf)
        return nullptr;

    if (!readEntry(entry, buf))
    {
        free(buf);
        return nullptr;
    }

    if (outSize)
        *outSize = sz;

    return buf;
}

} // namespace HDArchive
