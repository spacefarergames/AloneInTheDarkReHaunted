///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Archive Builder Tool - build_hda_archive.exe
//
// Rebuilds the backgrounds_hd.hda archive from source PNG files
// in a backgrounds_hd/ directory.
//
// Usage: build_hda_archive <source_dir> <output_archive>
//   Example: build_hda_archive "..\..\Shipping\backgrounds_hd" "backgrounds_hd.hda"
//
///////////////////////////////////////////////////////////////////////////////

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

namespace fs = std::filesystem;

// Archive format constants (must match hdArchive.h)
static const uint32_t HDARCHIVE_MAGIC = 0x47424448;   // "HDBG"
static const uint32_t HDARCHIVE_VERSION = 1;

struct ArchiveEntry
{
    std::string relativePath;  // Relative path within archive
    std::vector<uint8_t> data; // File contents
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

// Build the archive file
bool buildArchive(const std::string& sourceDir, const std::string& outputPath)
{
    printf("Building HD Archive...\n");
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
    uint64_t dataOffset = 12; // header size
    for (const auto& entry : entries)
    {
        // 2 bytes path length + path + 8 bytes offset + 8 bytes size
        dataOffset += 2 + entry.relativePath.length() + 8 + 8;
    }

    // Write table of contents and collect data
    std::vector<uint8_t> allData;
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

        uint64_t offset = dataOffset + allData.size();
        uint64_t size = (uint64_t)entry.data.size();

        if (fwrite(&offset, 8, 1, out) != 1 ||
            fwrite(&size, 8, 1, out) != 1)
        {
            fprintf(stderr, "Error: Failed to write offset/size\n");
            fclose(out);
            return false;
        }

        allData.insert(allData.end(), entry.data.begin(), entry.data.end());
    }

    // Write data section
    if (fwrite(allData.data(), 1, allData.size(), out) != allData.size())
    {
        fprintf(stderr, "Error: Failed to write data section\n");
        fclose(out);
        return false;
    }

    fclose(out);

    printf("Archive created successfully!\n");
    printf("Total size: %zu bytes\n", 12 + dataOffset - 12 + allData.size());
    return true;
}

int main(int argc, char* argv[])
{
    if (argc < 3)
    {
        printf("build_hda_archive - HD Background Archive Builder\n");
        printf("Usage: build_hda_archive <source_dir> <output_archive>\n");
        printf("Example: build_hda_archive \"backgrounds_hd\" \"backgrounds_hd.hda\"\n");
        return 1;
    }

    std::string sourceDir = argv[1];
    std::string outputPath = argv[2];

    if (!buildArchive(sourceDir, outputPath))
    {
        return 1;
    }

    return 0;
}
