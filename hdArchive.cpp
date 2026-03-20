///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Author: Jake Jackson (jake@spacefarergames.com)
//
// HD Background Archive implementation
///////////////////////////////////////////////////////////////////////////////

#include "hdArchive.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <cctype>

// Archive magic & version
static const uint32_t HDARCHIVE_MAGIC   = 0x47424448; // "HDBG" as little-endian uint32
static const uint32_t HDARCHIVE_VERSION = 1;

// Internal state (singleton)
static FILE*                          s_file    = nullptr;
static std::vector<HDArchiveEntry>    s_entries;
static bool                           s_open    = false;

// Case-insensitive string compare (ASCII only, sufficient for file names)
static bool strEqualCI(const char* a, const char* b)
{
    while (*a && *b)
    {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return false;
        ++a;
        ++b;
    }
    return *a == *b;
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

    if (magic != HDARCHIVE_MAGIC || version != HDARCHIVE_VERSION)
    {
        printf("HDArchive: bad magic/version in %s (magic=0x%08X, ver=%u)\n",
               archivePath, magic, version);
        fclose(f);
        return false;
    }

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

        s_entries[i].path   = std::move(path);
        s_entries[i].offset = offset;
        s_entries[i].size   = size;
    }

    // Sort entries by path for deterministic prefix searches
    std::sort(s_entries.begin(), s_entries.end(),
              [](const HDArchiveEntry& a, const HDArchiveEntry& b)
              {
                  return a.path < b.path;
              });

    s_file = f;
    s_open = true;

    printf("HDArchive: opened %s (%u entries)\n", archivePath, entryCount);
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

    for (const auto& e : s_entries)
    {
        if (strEqualCI(e.path.c_str(), relativePath))
            return &e;
    }
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

    return fread(buf, 1, (size_t)entry->size, s_file) == (size_t)entry->size;
}

unsigned char* readEntryAlloc(const HDArchiveEntry* entry, size_t* outSize)
{
    if (!entry)
        return nullptr;

    size_t sz = (size_t)entry->size;
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
