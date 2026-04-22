///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Archive Unpacker Tool - unpack_hda_archive.exe
//
// Unpacks a .hda (HDBG) archive produced by build_hda_archive into a
// directory, transparently decompressing zlib-compressed entries.
//
// Supports archive format v1 and v2.
//
// Usage: unpack_hda_archive <archive.hda> [output_directory]
//   If output_directory is omitted, a directory named after the archive
//   (without extension) is used in the current working directory.
//
// Examples:
//   unpack_hda_archive backgrounds_hd.hda
//   unpack_hda_archive backgrounds_hd.hda extracted/
//   unpack_hda_archive atlases.hda my_atlases/
//
///////////////////////////////////////////////////////////////////////////////

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>

#include "zlib.h"

namespace fs = std::filesystem;

// ── Archive format constants (must match hdArchive.h) ────────────────────────
static const uint32_t HDARCHIVE_MAGIC_V1 = 0x47424448; // "HDBG"

enum : uint32_t
{
    HDARCHIVE_FLAG_COMPRESSED = 1u << 0,
};

// ── TOC entry as stored in memory ────────────────────────────────────────────
struct TocEntry
{
    std::string path;
    uint64_t    offset;
    uint64_t    size;               // stored (on-disk) size
    uint64_t    uncompressedSize;   // == size for uncompressed entries
    uint32_t    flags;
};

// ── Helper: create directories recursively (portable) ────────────────────────
static bool mkdirs(const fs::path& p)
{
    std::error_code ec;
    fs::create_directories(p, ec);
    return !ec;
}

// ── Helper: read a little-endian value from file ─────────────────────────────
template<typename T>
static bool readLE(FILE* f, T& out)
{
    if (fread(&out, sizeof(T), 1, f) != 1)
        return false;
    // The tools only run on little-endian hosts, nothing to swap.
    return true;
}

// ── Decompress a zlib stream ─────────────────────────────────────────────────
static bool zlibDecompress(const uint8_t* src, size_t srcLen,
                            uint8_t* dst, size_t dstLen)
{
    uLongf outLen = (uLongf)dstLen;
    int rc = uncompress(dst, &outLen, src, (uLong)srcLen);
    if (rc != Z_OK)
    {
        fprintf(stderr, "  zlib error %d during decompression\n", rc);
        return false;
    }
    if ((size_t)outLen != dstLen)
    {
        fprintf(stderr, "  zlib output size mismatch: expected %zu, got %lu\n",
                dstLen, (unsigned long)outLen);
        return false;
    }
    return true;
}

// ── Main unpacking logic ──────────────────────────────────────────────────────
static bool unpackArchive(const std::string& archivePath, const std::string& outputDir)
{
    FILE* f = fopen(archivePath.c_str(), "rb");
    if (!f)
    {
        fprintf(stderr, "Error: Cannot open archive: %s\n", archivePath.c_str());
        return false;
    }

    // ── Header ──
    uint32_t magic, version, entryCount;
    if (!readLE(f, magic) || !readLE(f, version) || !readLE(f, entryCount))
    {
        fprintf(stderr, "Error: Failed to read archive header\n");
        fclose(f);
        return false;
    }

    if (magic != HDARCHIVE_MAGIC_V1)
    {
        fprintf(stderr, "Error: Not a valid HDBG archive (magic 0x%08X)\n", magic);
        fclose(f);
        return false;
    }

    if (version != 1 && version != 2)
    {
        fprintf(stderr, "Error: Unsupported archive version %u (supported: 1, 2)\n", version);
        fclose(f);
        return false;
    }

    printf("Archive: %s\n", archivePath.c_str());
    printf("Version: %u\n", version);
    printf("Entries: %u\n\n", entryCount);

    // ── Table of contents ──
    std::vector<TocEntry> toc;
    toc.resize(entryCount);

    for (uint32_t i = 0; i < entryCount; i++)
    {
        TocEntry& e = toc[i];

        uint16_t pathLen;
        if (!readLE(f, pathLen))
        {
            fprintf(stderr, "Error: Failed to read path length for entry %u\n", i);
            fclose(f);
            return false;
        }

        e.path.resize(pathLen);
        if (pathLen > 0 && fread(e.path.data(), 1, pathLen, f) != pathLen)
        {
            fprintf(stderr, "Error: Failed to read path for entry %u\n", i);
            fclose(f);
            return false;
        }

        if (!readLE(f, e.offset) || !readLE(f, e.size))
        {
            fprintf(stderr, "Error: Failed to read offset/size for entry '%s'\n", e.path.c_str());
            fclose(f);
            return false;
        }

        if (version >= 2)
        {
            if (!readLE(f, e.uncompressedSize) || !readLE(f, e.flags))
            {
                fprintf(stderr, "Error: Failed to read v2 fields for entry '%s'\n", e.path.c_str());
                fclose(f);
                return false;
            }
        }
        else
        {
            // v1: always uncompressed
            e.uncompressedSize = e.size;
            e.flags = 0;
        }
    }

    // ── Output directory ──
    if (!mkdirs(fs::path(outputDir)))
    {
        fprintf(stderr, "Error: Failed to create output directory: %s\n", outputDir.c_str());
        fclose(f);
        return false;
    }

    // ── Extract entries ──
    uint64_t totalBytesOut = 0;
    uint32_t compressedCount = 0;
    uint32_t failCount = 0;

    for (uint32_t i = 0; i < entryCount; i++)
    {
        const TocEntry& e = toc[i];

        // Seek to this entry's data
        if (fseek(f, (long)e.offset, SEEK_SET) != 0)
        {
            fprintf(stderr, "  [%u/%u] SKIP (seek failed): %s\n", i + 1, entryCount, e.path.c_str());
            failCount++;
            continue;
        }

        // Read stored bytes
        std::vector<uint8_t> stored(e.size);
        if (e.size > 0 && fread(stored.data(), 1, (size_t)e.size, f) != (size_t)e.size)
        {
            fprintf(stderr, "  [%u/%u] SKIP (read failed): %s\n", i + 1, entryCount, e.path.c_str());
            failCount++;
            continue;
        }

        // Decompress if needed
        std::vector<uint8_t> output;
        bool compressed = (e.flags & HDARCHIVE_FLAG_COMPRESSED) != 0;

        if (compressed)
        {
            output.resize((size_t)e.uncompressedSize);
            if (!zlibDecompress(stored.data(), (size_t)e.size,
                                output.data(), (size_t)e.uncompressedSize))
            {
                fprintf(stderr, "  [%u/%u] SKIP (decompress failed): %s\n",
                        i + 1, entryCount, e.path.c_str());
                failCount++;
                continue;
            }
            compressedCount++;
        }
        else
        {
            output = std::move(stored);
        }

        // Build destination path — ensure forward slashes become OS separators
        fs::path destPath = fs::path(outputDir) / fs::path(e.path);

        // Create parent directories
        if (!mkdirs(destPath.parent_path()))
        {
            fprintf(stderr, "  [%u/%u] SKIP (mkdir failed): %s\n",
                    i + 1, entryCount, e.path.c_str());
            failCount++;
            continue;
        }

        // Write file
        FILE* out = fopen(destPath.string().c_str(), "wb");
        if (!out)
        {
            fprintf(stderr, "  [%u/%u] SKIP (open failed): %s\n",
                    i + 1, entryCount, e.path.c_str());
            failCount++;
            continue;
        }

        bool wrote = (output.empty() || fwrite(output.data(), 1, output.size(), out) == output.size());
        fclose(out);

        if (!wrote)
        {
            fprintf(stderr, "  [%u/%u] SKIP (write failed): %s\n",
                    i + 1, entryCount, e.path.c_str());
            failCount++;
            continue;
        }

        totalBytesOut += output.size();

        printf("  [%u/%u] %s  (%llu bytes%s)\n",
               i + 1, entryCount,
               e.path.c_str(),
               (unsigned long long)output.size(),
               compressed ? ", decompressed" : "");
    }

    fclose(f);

    // ── Summary ──
    printf("\nDone.\n");
    printf("  Extracted:   %u / %u entries", entryCount - failCount, entryCount);
    if (failCount > 0)
        printf("  (%u failed)", failCount);
    printf("\n");
    printf("  Decompressed: %u entries\n", compressedCount);
    printf("  Total output: %llu bytes\n", (unsigned long long)totalBytesOut);
    printf("  Output dir:   %s\n", outputDir.c_str());

    return failCount == 0;
}

// ── Entry point ───────────────────────────────────────────────────────────────
int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        printf("unpack_hda_archive - HD Asset Archive Unpacker\n");
        printf("Reads HDBG (.hda) archives produced by build_hda_archive\n\n");
        printf("Usage: unpack_hda_archive <archive.hda> [output_directory]\n\n");
        printf("  archive.hda      - path to the .hda archive\n");
        printf("  output_directory - where to extract files (default: archive name without .hda)\n\n");
        printf("Examples:\n");
        printf("  unpack_hda_archive backgrounds_hd.hda\n");
        printf("  unpack_hda_archive backgrounds_hd.hda extracted/\n");
        printf("  unpack_hda_archive atlases.hda my_atlases/\n");
        return 1;
    }

    std::string archivePath = argv[1];

    // Derive default output directory from archive filename (strip extension)
    std::string outputDir;
    if (argc >= 3)
    {
        outputDir = argv[2];
    }
    else
    {
        fs::path p(archivePath);
        outputDir = p.stem().string();
        if (outputDir.empty())
            outputDir = "hda_output";
    }

    // Strip trailing separator from outputDir for consistency
    while (!outputDir.empty() && (outputDir.back() == '/' || outputDir.back() == '\\'))
        outputDir.pop_back();

    return unpackArchive(archivePath, outputDir) ? 0 : 1;
}
