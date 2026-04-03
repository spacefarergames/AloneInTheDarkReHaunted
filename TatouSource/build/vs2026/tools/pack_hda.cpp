///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// pack_hda - Pack a folder of HD background images into a .hda archive.
//
// Usage:  pack_hda <source_folder> [output_file]
//         source_folder : directory containing PNG/TGA/BMP images and
//                         animation sub-directories (e.g. "backgrounds_hd")
//         output_file   : archive output path (default: "backgrounds_hd.hda")
//
// Archive format (.hda) - version 1:
//   Header:
//     4 bytes  - magic "HDBG"
//     4 bytes  - version (uint32, currently 1)
//     4 bytes  - entry count (uint32)
//   Entry table (repeated entry_count times):
//     2 bytes  - path length (uint16, NOT null-terminated)
//     N bytes  - relative path (forward slashes)
//     8 bytes  - data offset from start of file (uint64)
//     8 bytes  - data size in bytes (uint64)
//   Data section:
//     Raw file bytes concatenated (PNG/TGA/BMP as-is)
//
///////////////////////////////////////////////////////////////////////////////

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

// Archive constants (must match hdArchive.h)
static const uint32_t HDARCHIVE_MAGIC   = 0x47424448; // "HDBG" little-endian
static const uint32_t HDARCHIVE_VERSION = 1;

struct FileEntry
{
    std::string relativePath; // forward-slash separated, no leading slash
    std::string fullPath;     // filesystem path for reading
    uint64_t    dataOffset;   // filled during write pass
    uint64_t    dataSize;     // file size in bytes
};

// Check if a filename ends with a supported image extension (case-insensitive)
static bool isImageFile(const char* name)
{
    size_t len = strlen(name);
    if (len < 5) return false; // at least "x.png"
    const char* ext = name + len - 4;
#ifdef _WIN32
    return (_stricmp(ext, ".png") == 0 ||
            _stricmp(ext, ".tga") == 0 ||
            _stricmp(ext, ".bmp") == 0);
#else
    return (strcasecmp(ext, ".png") == 0 ||
            strcasecmp(ext, ".tga") == 0 ||
            strcasecmp(ext, ".bmp") == 0);
#endif
}

// Normalize path separators to forward slashes
static std::string normalizePath(const std::string& path)
{
    std::string out = path;
    for (auto& c : out)
    {
        if (c == '\\') c = '/';
    }
    return out;
}

// Recursively scan a directory and collect image files
static void scanDirectory(const std::string& dirPath, const std::string& relativePrefix, std::vector<FileEntry>& entries)
{
#ifdef _WIN32
    std::string searchPath = dirPath + "\\*.*";
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do
    {
        const char* name = findData.cFileName;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;

        std::string fullPath = dirPath + "\\" + name;

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            std::string subPrefix = relativePrefix.empty()
                ? std::string(name)
                : relativePrefix + "/" + name;
            scanDirectory(fullPath, subPrefix, entries);
        }
        else if (isImageFile(name))
        {
            FileEntry e;
            e.fullPath = fullPath;
            e.relativePath = relativePrefix.empty()
                ? std::string(name)
                : relativePrefix + "/" + name;
            e.dataOffset = 0;
            e.dataSize = 0;
            entries.push_back(std::move(e));
        }
    } while (FindNextFileA(hFind, &findData));

    FindClose(hFind);
#else
    DIR* dir = opendir(dirPath.c_str());
    if (!dir) return;

    struct dirent* ent;
    while ((ent = readdir(dir)) != nullptr)
    {
        const char* name = ent->d_name;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;

        std::string fullPath = dirPath + "/" + name;

        struct stat st;
        if (stat(fullPath.c_str(), &st) != 0) continue;

        if (S_ISDIR(st.st_mode))
        {
            std::string subPrefix = relativePrefix.empty()
                ? std::string(name)
                : relativePrefix + "/" + name;
            scanDirectory(fullPath, subPrefix, entries);
        }
        else if (S_ISREG(st.st_mode) && isImageFile(name))
        {
            FileEntry e;
            e.fullPath = fullPath;
            e.relativePath = relativePrefix.empty()
                ? std::string(name)
                : relativePrefix + "/" + name;
            e.dataOffset = 0;
            e.dataSize = 0;
            entries.push_back(std::move(e));
        }
    }

    closedir(dir);
#endif
}

// Get file size
static uint64_t getFileSize(const char* path)
{
#ifdef _WIN32
    HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ,
                                NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return 0;
    LARGE_INTEGER li;
    GetFileSizeEx(hFile, &li);
    CloseHandle(hFile);
    return (uint64_t)li.QuadPart;
#else
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return (uint64_t)st.st_size;
#endif
}

int main(int argc, char* argv[])
{
    const char* sourceFolder = "backgrounds_hd";
    const char* outputFile   = "backgrounds_hd.hda";

    if (argc >= 2) sourceFolder = argv[1];
    if (argc >= 3) outputFile   = argv[2];

    printf("pack_hda: Scanning '%s' ...\n", sourceFolder);

    // 1. Scan source folder
    std::vector<FileEntry> entries;
    scanDirectory(sourceFolder, "", entries);

    if (entries.empty())
    {
        fprintf(stderr, "Error: No image files found in '%s'\n", sourceFolder);
        return 1;
    }

    // Sort entries by relative path for deterministic output
    std::sort(entries.begin(), entries.end(),
              [](const FileEntry& a, const FileEntry& b)
              {
                  return a.relativePath < b.relativePath;
              });

    printf("  Found %zu files\n", entries.size());

    // 2. Measure file sizes
    uint64_t totalDataSize = 0;
    for (auto& e : entries)
    {
        e.dataSize = getFileSize(e.fullPath.c_str());
        if (e.dataSize == 0)
        {
            fprintf(stderr, "Warning: Skipping zero-length file '%s'\n", e.fullPath.c_str());
        }
        totalDataSize += e.dataSize;
    }

    // Remove zero-length entries
    entries.erase(
        std::remove_if(entries.begin(), entries.end(),
                        [](const FileEntry& e) { return e.dataSize == 0; }),
        entries.end());

    printf("  Total data: %.2f MB across %zu files\n",
           totalDataSize / (1024.0 * 1024.0), entries.size());

    // 3. Compute TOC size so we know where the data section begins
    //    Header: 4 + 4 + 4 = 12 bytes
    //    Each TOC entry: 2 (pathLen) + pathLen + 8 (offset) + 8 (size)
    uint64_t tocSize = 12; // header
    for (const auto& e : entries)
    {
        tocSize += 2 + (uint64_t)e.relativePath.size() + 8 + 8;
    }

    // Compute data offsets
    uint64_t dataStart = tocSize;
    uint64_t currentOffset = dataStart;
    for (auto& e : entries)
    {
        e.dataOffset = currentOffset;
        currentOffset += e.dataSize;
    }

    // 4. Write archive
    FILE* out = fopen(outputFile, "wb");
    if (!out)
    {
        fprintf(stderr, "Error: Cannot open output file '%s' for writing\n", outputFile);
        return 1;
    }

    printf("  Writing archive to '%s' ...\n", outputFile);

    // Header
    uint32_t magic   = HDARCHIVE_MAGIC;
    uint32_t version = HDARCHIVE_VERSION;
    uint32_t count   = (uint32_t)entries.size();
    fwrite(&magic,   4, 1, out);
    fwrite(&version, 4, 1, out);
    fwrite(&count,   4, 1, out);

    // TOC
    for (const auto& e : entries)
    {
        uint16_t pathLen = (uint16_t)e.relativePath.size();
        fwrite(&pathLen, 2, 1, out);
        fwrite(e.relativePath.c_str(), 1, pathLen, out);
        fwrite(&e.dataOffset, 8, 1, out);
        fwrite(&e.dataSize,   8, 1, out);
    }

    // Data section — copy each file
    const size_t BUF_SIZE = 1024 * 1024; // 1 MB copy buffer
    unsigned char* buf = (unsigned char*)malloc(BUF_SIZE);
    if (!buf)
    {
        fprintf(stderr, "Error: Out of memory\n");
        fclose(out);
        return 1;
    }

    int fileIndex = 0;
    int totalFiles = (int)entries.size();
    int lastPercent = -1;

    for (const auto& e : entries)
    {
        FILE* src = fopen(e.fullPath.c_str(), "rb");
        if (!src)
        {
            fprintf(stderr, "Error: Cannot open source file '%s'\n", e.fullPath.c_str());
            free(buf);
            fclose(out);
            return 1;
        }

        uint64_t remaining = e.dataSize;
        while (remaining > 0)
        {
            size_t toRead = (remaining > BUF_SIZE) ? BUF_SIZE : (size_t)remaining;
            size_t read = fread(buf, 1, toRead, src);
            if (read != toRead)
            {
                fprintf(stderr, "Error: Short read on '%s'\n", e.fullPath.c_str());
                fclose(src);
                free(buf);
                fclose(out);
                return 1;
            }
            fwrite(buf, 1, read, out);
            remaining -= read;
        }

        fclose(src);

        fileIndex++;
        int percent = (int)((uint64_t)fileIndex * 100 / totalFiles);
        if (percent != lastPercent)
        {
            printf("\r  Progress: %d%% (%d/%d files)", percent, fileIndex, totalFiles);
            fflush(stdout);
            lastPercent = percent;
        }
    }

    free(buf);
    fclose(out);

    // Report final size
    uint64_t archiveSize = getFileSize(outputFile);
    printf("\n  Done! Archive: %.2f MB (%zu entries)\n",
           archiveSize / (1024.0 * 1024.0), entries.size());

    return 0;
}
