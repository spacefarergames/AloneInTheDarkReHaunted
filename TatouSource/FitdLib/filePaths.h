#pragma once
#include <cerrno>
#include <cstdio>
#include <string>
#include <cstdint>
#include <vector>

#ifdef FITD_UNITY_COMMANDS
std::string fitdReadPath(const char* path);
std::string fitdWritePath(const char* path);
std::string fitdReadRoot();
FILE* fitdFopen(const char* path, const char* mode);
#else
inline std::string fitdReadPath(const char* path) { return path; }
inline std::string fitdWritePath(const char* path) { return path; }
inline FILE* fitdFopen(const char* path, const char* mode) { return std::fopen(path, mode); }
#endif
inline int fitdFopen_s(FILE** file, const char* path, const char* mode) {
    if (!file || !path || !mode) return EINVAL;
    *file = fitdFopen(path, mode); return *file ? 0 : errno ? errno : ENOENT;
}
// Read-only engine streams can address either a bundled file or a loose file.
#ifdef FITD_UNITY_COMMANDS
struct FitdReadFile;
FitdReadFile* fitdOpenRead(const char* path, const char* mode = "rb");
size_t fitdRead(void* data, size_t size, size_t count, FitdReadFile* file);
int fitdSeek(FitdReadFile* file, int64_t offset, int origin);
int64_t fitdTell(FitdReadFile* file);
int fitdClose(FitdReadFile* file);
bool fitdHasMemoryFile(const char* path);
std::vector<std::string> fitdListMemoryFiles(const char* folder);
void fitdCancelMemoryReads();
#else
using FitdReadFile = FILE;
inline FitdReadFile* fitdOpenRead(const char* path, const char* mode = "rb") { return fitdFopen(path, mode); }
inline size_t fitdRead(void* data, size_t size, size_t count, FitdReadFile* file) { return std::fread(data, size, count, file); }
inline int fitdSeek(FitdReadFile* file, int64_t offset, int origin) {
#ifdef _WIN32
    return _fseeki64(file, offset, origin);
#else
    return fseeko(file, offset, origin);
#endif
}
inline int64_t fitdTell(FitdReadFile* file) { return std::ftell(file); }
inline int fitdClose(FitdReadFile* file) { return std::fclose(file); }
#endif
inline int fitdOpenRead_s(FitdReadFile** file, const char* path, const char* mode) {
    if (!file || !path || !mode) return EINVAL;
    *file = fitdOpenRead(path, mode);
    return *file ? 0 : errno ? errno : ENOENT;
}

// Keep C++ path temporaries in a separate function: some legacy image readers
// contain Windows SEH blocks and cannot own objects requiring stack unwinding.
template<class Loader,class... Args>
inline auto fitdReadImagePath(Loader loader,const char* path,Args... args)->decltype(loader(path,args...)) {
    auto resolved=fitdReadPath(path);return loader(resolved.c_str(),args...);
}
template<class Writer,class... Args>
inline auto fitdWriteImagePath(Writer writer,const char* path,Args... args)->decltype(writer(path,args...)) {
    auto resolved=fitdWritePath(path);return writer(resolved.c_str(),args...);
}
#ifdef FITD_UNITY_COMMANDS
template<class Loader, class MemoryLoader, class... Args>
inline auto fitdReadImage(Loader loader, MemoryLoader memoryLoader, const char* path, Args... args)
    -> decltype(loader(path, args...))
{
    if (!fitdHasMemoryFile(path)) return fitdReadImagePath(loader, path, args...);
    auto* file = fitdOpenRead(path);
    if (!file) return nullptr;
    fitdSeek(file, 0, SEEK_END);
    const auto length = fitdTell(file);
    fitdSeek(file, 0, SEEK_SET);
    if (length <= 0 || length > 512 * 1024 * 1024) { fitdClose(file); return nullptr; }
    std::vector<unsigned char> bytes(static_cast<size_t>(length));
    const auto read = fitdRead(bytes.data(), 1, bytes.size(), file);
    fitdClose(file);
    return read == bytes.size() ? memoryLoader(bytes.data(), int(length), args...) : nullptr;
}
#define fitdLoadImage(path, ...) fitdReadImage(stbi_load, stbi_load_from_memory, path, __VA_ARGS__)
#else
#define fitdLoadImage(path, ...) fitdReadImagePath(stbi_load, path, __VA_ARGS__)
#endif
#define fitdWritePng(path, ...) fitdWriteImagePath(stbi_write_png, path, __VA_ARGS__)
