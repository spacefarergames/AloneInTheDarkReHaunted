///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Archive Builder Tool - build_hda_archive.exe
//
// Generic HD archive builder supporting:
// - backgrounds_hd.hda from image files in backgrounds_hd/ directory
// - atlases.hda from image files in atlases/ directory
// - audio.hda from WAV/OGG/MP3 files in audio/ directory
//
// Produces v2 archives with per-entry zlib compression by default.
// Use --no-compress to write entries uncompressed (still v2 format).
//
// Usage: build_hda_archive [--no-compress] <source_directory> <output_archive.hda>
//   Example: build_hda_archive "backgrounds_hd" "backgrounds_hd.hda"
//   Example: build_hda_archive "atlases" "atlases.hda"
//   Example: build_hda_archive "audio" "audio.hda"
//
///////////////////////////////////////////////////////////////////////////////

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>

#include "zlib.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

namespace fs = std::filesystem;

// Archive format constants (must match hdArchive.h)
static const uint32_t HDARCHIVE_MAGIC   = 0x47424448; // "HDBG"
static const uint32_t HDARCHIVE_VERSION = 2;

// Per-entry flags (must match HDArchiveEntryFlags in hdArchive.h)
enum : uint32_t
{
    HDARCHIVE_FLAG_NONE       = 0,
    HDARCHIVE_FLAG_COMPRESSED = 1 << 0,
};

struct ArchiveEntry
{
    std::string relativePath;        // Relative path within archive
    std::vector<uint8_t> data;       // Original (uncompressed) file contents
    std::vector<uint8_t> compressed; // zlib-compressed data (empty if not compressed)
    uint32_t flags = 0;              // HDARCHIVE_FLAG_*
};

// Recursively scan directory and collect all files
void scanDirectory(const fs::path& dirPath, const fs::path& baseDir, std::vector<ArchiveEntry>& entries)
{
    try
    {
        for (const auto& entry : fs::recursive_directory_iterator(dirPath))
        {
            if (entry.is_regular_file())
            {
                // Get relative path from base directory
                fs::path relPath = fs::relative(entry.path(), baseDir);
                
                // Convert to forward slashes for archive (cross-platform)
                std::string archivePath = relPath.string();
                std::replace(archivePath.begin(), archivePath.end(), '\\', '/');

                // Read file
                FILE* f = fopen(entry.path().string().c_str(), "rb");
                if (!f)
                {
                    fprintf(stderr, "Warning: Failed to open %s\n", entry.path().string().c_str());
                    continue;
                }

                fseek(f, 0, SEEK_END);
                size_t fileSize = ftell(f);
                fseek(f, 0, SEEK_SET);

                std::vector<uint8_t> fileData(fileSize);
                if (fread(fileData.data(), 1, fileSize, f) != fileSize)
                {
                    fprintf(stderr, "Warning: Failed to read %s\n", entry.path().string().c_str());
                    fclose(f);
                    continue;
                }
                fclose(f);

                // Add to entries
                ArchiveEntry archEntry;
                archEntry.relativePath = archivePath;
                archEntry.data = fileData;
                entries.push_back(archEntry);

                printf("  Added: %s (%zu bytes)\n", archivePath.c_str(), fileSize);
            }
        }
    }
    catch (const std::exception& e)
    {
        fprintf(stderr, "Error scanning directory: %s\n", e.what());
    }
}

// Compress a single entry in-place using zlib.
// Sets entry.compressed and entry.flags if compression is beneficial.
static void compressEntry(ArchiveEntry& entry)
{
    const size_t srcLen = entry.data.size();
    if (srcLen == 0)
        return;

    // Allocate worst-case output buffer
    uLongf destLen = compressBound((uLong)srcLen);
    std::vector<uint8_t> buf(destLen);

    int rc = compress2(buf.data(), &destLen,
                       entry.data.data(), (uLong)srcLen, Z_BEST_COMPRESSION);
    if (rc != Z_OK)
    {
        fprintf(stderr, "Warning: zlib compress failed for %s (rc=%d), storing uncompressed\n",
                entry.relativePath.c_str(), rc);
        return;
    }

    // Only use compressed version if it is actually smaller
    if (destLen < srcLen)
    {
        buf.resize(destLen);
        entry.compressed = std::move(buf);
        entry.flags = HDARCHIVE_FLAG_COMPRESSED;
    }
}

// Build the archive file
bool buildArchive(const std::string& sourceDir, const std::string& outputPath,
                  bool enableCompression)
{
    printf("Building HD Archive (v%u%s)...\n", HDARCHIVE_VERSION,
           enableCompression ? ", compressed" : ", no compression");
    printf("Source: %s\n", sourceDir.c_str());
    printf("Output: %s\n\n", outputPath.c_str());

    // Collect all files
    std::vector<ArchiveEntry> entries;
    fs::path baseDir(sourceDir);

    if (!fs::exists(baseDir))
    {
        fprintf(stderr, "Error: Source directory does not exist: %s\n", sourceDir.c_str());
        return false;
    }

    scanDirectory(baseDir, baseDir, entries);

    if (entries.empty())
    {
        fprintf(stderr, "Error: No files found in source directory\n");
        return false;
    }

    printf("\nTotal files: %zu\n\n", entries.size());

    // Sort entries alphabetically for deterministic output
    std::sort(entries.begin(), entries.end(),
        [](const ArchiveEntry& a, const ArchiveEntry& b)
        {
            return a.relativePath < b.relativePath;
        });

    // Compress entries
    uint64_t totalRaw = 0;
    uint64_t totalStored = 0;
    uint32_t compressedCount = 0;

    for (auto& entry : entries)
    {
        totalRaw += entry.data.size();
        if (enableCompression)
            compressEntry(entry);

        if (entry.flags & HDARCHIVE_FLAG_COMPRESSED)
        {
            totalStored += entry.compressed.size();
            compressedCount++;
        }
        else
        {
            totalStored += entry.data.size();
        }
    }

    // Open output file
    FILE* out = fopen(outputPath.c_str(), "wb");
    if (!out)
    {
        fprintf(stderr, "Error: Failed to open output file: %s\n", outputPath.c_str());
        return false;
    }

    // Write header
    uint32_t magic = HDARCHIVE_MAGIC;
    uint32_t version = HDARCHIVE_VERSION;
    uint32_t entryCount = (uint32_t)entries.size();

    if (fwrite(&magic, 4, 1, out) != 1 ||
        fwrite(&version, 4, 1, out) != 1 ||
        fwrite(&entryCount, 4, 1, out) != 1)
    {
        fprintf(stderr, "Error: Failed to write header\n");
        fclose(out);
        return false;
    }

    // Calculate TOC size to determine where data section starts
    // v2 TOC per entry: 2 (pathLen) + path + 8 (offset) + 8 (size) + 8 (uncompressedSize) + 4 (flags)
    uint64_t dataOffset = 12; // header size
    for (const auto& entry : entries)
    {
        dataOffset += 2 + entry.relativePath.length() + 8 + 8 + 8 + 4;
    }

    // Write table of contents and collect data
    std::vector<uint8_t> allData;
    allData.reserve((size_t)totalStored);

    for (auto& entry : entries)
    {
        uint16_t pathLen = (uint16_t)entry.relativePath.length();

        if (fwrite(&pathLen, 2, 1, out) != 1)
        {
            fprintf(stderr, "Error: Failed to write path length\n");
            fclose(out);
            return false;
        }

        if (fwrite(entry.relativePath.c_str(), 1, pathLen, out) != pathLen)
        {
            fprintf(stderr, "Error: Failed to write path\n");
            fclose(out);
            return false;
        }

        // Determine stored data for this entry
        const std::vector<uint8_t>& storedData =
            (entry.flags & HDARCHIVE_FLAG_COMPRESSED) ? entry.compressed : entry.data;

        uint64_t offset           = dataOffset + allData.size();
        uint64_t size             = (uint64_t)storedData.size();
        uint64_t uncompressedSize = (uint64_t)entry.data.size();
        uint32_t flags            = entry.flags;

        if (fwrite(&offset, 8, 1, out) != 1 ||
            fwrite(&size, 8, 1, out) != 1 ||
            fwrite(&uncompressedSize, 8, 1, out) != 1 ||
            fwrite(&flags, 4, 1, out) != 1)
        {
            fprintf(stderr, "Error: Failed to write TOC entry\n");
            fclose(out);
            return false;
        }

        allData.insert(allData.end(), storedData.begin(), storedData.end());
    }

    // Write data section
    if (fwrite(allData.data(), 1, allData.size(), out) != allData.size())
    {
        fprintf(stderr, "Error: Failed to write data section\n");
        fclose(out);
        return false;
    }

    fclose(out);

    // Print summary
    uint64_t archiveSize = dataOffset + allData.size();
    printf("\nArchive created successfully!\n");
    printf("  Entries:      %u (%u compressed, %u stored raw)\n",
           entryCount, compressedCount, entryCount - compressedCount);
    printf("  Raw data:     %llu bytes\n", (unsigned long long)totalRaw);
    printf("  Stored data:  %llu bytes\n", (unsigned long long)totalStored);
    if (totalRaw > 0)
    {
        double ratio = 100.0 * (1.0 - (double)totalStored / (double)totalRaw);
        printf("  Compression:  %.1f%% reduction\n", ratio);
    }
    printf("  Archive size: %llu bytes\n", (unsigned long long)archiveSize);
    return true;
}

int main(int argc, char* argv[])
{
    // Parse optional flags
    bool enableCompression = true;
    int argIdx = 1;

    while (argIdx < argc && argv[argIdx][0] == '-')
    {
        if (strcmp(argv[argIdx], "--no-compress") == 0)
        {
            enableCompression = false;
        }
        else
        {
            fprintf(stderr, "Unknown option: %s\n", argv[argIdx]);
            return 1;
        }
        argIdx++;
    }

    // build_hda_archive [--no-compress] <source_dir> <output_file>
    if (argIdx + 2 <= argc)
    {
        std::string sourceDir  = argv[argIdx];
        std::string outputFile = argv[argIdx + 1];

        if (buildArchive(sourceDir, outputFile, enableCompression))
            return 0;
        return 1;
    }

    // Show help
    printf("build_hda_archive - HD Asset Archive Builder (v%u)\n", HDARCHIVE_VERSION);
    printf("Packages directory contents into HDBG archive format with zlib compression\n\n");
    printf("Usage: build_hda_archive [--no-compress] <source_directory> <output_archive.hda>\n\n");
    printf("Options:\n");
    printf("  --no-compress  Store entries uncompressed (still writes v2 format)\n\n");
    printf("Examples:\n");
    printf("  build_hda_archive \"backgrounds_hd\" \"backgrounds_hd.hda\"\n");
    printf("  build_hda_archive \"atlases\" \"atlases.hda\"\n");
    printf("  build_hda_archive \"audio\" \"audio.hda\"\n\n");
    printf("Supported archive types:\n");
    printf("  backgrounds_hd.hda  - HD background textures (PNG)\n");
    printf("  atlases.hda         - Model texture atlases (PNG)\n");
    printf("  audio.hda           - Audio files (WAV/OGG/MP3)\n");
    return 1;
}
