#include "unityGpuBridge.h"
#include "filePaths.h"
#include <filesystem>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <cctype>
#include <condition_variable>
#include <cstring>
#include <memory>
#include <limits>
#include <deque>
#include <algorithm>

extern "C" int FitdUnity_GetState();
namespace {
std::filesystem::path contentRoot, saveRoot;
std::unordered_map<std::string,std::filesystem::path> contentFiles;
std::string folded(std::string value){for(auto& character:value){if(character=='\\')character='/';else character=char(std::tolower(static_cast<unsigned char>(character)));}return value;}
std::string utf8(const std::filesystem::path& path) {
    auto value=path.generic_u8string(); return std::string(reinterpret_cast<const char*>(value.data()),value.size());
}
std::filesystem::path root() { return contentRoot.empty() ? std::filesystem::current_path() : contentRoot; }
std::filesystem::path resolve(const char* raw, bool writing) {
    if (!raw || !*raw) throw std::invalid_argument("Empty native data path");
    auto path=std::filesystem::u8path(raw).lexically_normal();
    auto data=root();
    if(path.is_absolute()) {
        auto relative=path.lexically_relative(data);
        if(relative.empty() || *relative.begin()=="..") return path;
        path=relative;
    }
    if(!path.empty() && *path.begin()=="..") throw std::invalid_argument("Native resource path escapes the content root");
    if(!saveRoot.empty()) {
        auto saved=saveRoot/path;
        if(writing) { std::filesystem::create_directories(saved.parent_path()); return saved; }
        if(std::filesystem::is_regular_file(saved)) return saved;
    }
    auto found=contentFiles.find(folded(utf8(path)));
    auto result=found==contentFiles.end()?data/path:found->second;
    if(writing)std::filesystem::create_directories(result.parent_path());
    return result;
}
}

FITD_UNITY_API int FitdUnity_SetContentPaths(const char* content, const char* saves) {
    if(FitdUnity_GetState()!=0 || !content || !saves)return 0;
    try {
        auto data=std::filesystem::u8path(content).lexically_normal();
        auto writable=std::filesystem::u8path(saves).lexically_normal();
        if(!data.is_absolute() || !writable.is_absolute() || !std::filesystem::is_directory(data) || data==writable)return 0;
        std::filesystem::create_directories(writable);
        contentRoot=data;saveRoot=writable;contentFiles.clear();
        for(const auto& file:std::filesystem::recursive_directory_iterator(data))
            if(file.is_regular_file() || file.is_directory())contentFiles.emplace(folded(utf8(file.path().lexically_relative(data))),file.path());
        return 1;
    }catch(...){return 0;}
}
std::string fitdReadRoot(){return utf8(root());}
std::string fitdReadPath(const char* path){return utf8(resolve(path,false));}
std::string fitdWritePath(const char* path){return utf8(resolve(path,true));}
FILE* fitdFopen(const char* path,const char* mode) {
    bool writing=std::string(mode).find_first_of("wa+")!=std::string::npos;
    auto resolved=resolve(path,writing);
#ifdef _WIN32
    std::wstring wideMode;for(const char* p=mode;*p;++p)wideMode.push_back(wchar_t(*p));
    return _wfopen(resolved.c_str(),wideMode.c_str());
#else
    return std::fopen(resolved.c_str(),mode);
#endif
}


namespace {
constexpr uint64_t pageSize = 4 * 1024 * 1024;
constexpr size_t cacheBudget = 128 * 1024 * 1024;

struct MemoryFile {
    int id;
    uint64_t length;
};

struct MemoryPage {
    int file, index, expected;
    uint64_t lastUse = 0;
    bool finished = false;
    std::vector<unsigned char> bytes;
};

std::unordered_map<std::string, MemoryFile> memoryFiles;
std::unordered_map<int, uint64_t> memoryLengths;
std::unordered_map<uint64_t, std::shared_ptr<MemoryPage>> pages;
std::deque<std::shared_ptr<MemoryPage>> requests;
std::mutex memoryMutex;
std::condition_variable memoryReady;
uint64_t useCounter = 0;
size_t cacheBytes = 0;
bool memoryCancelled = false;

uint64_t pageKey(int file, int index) {
    return (uint64_t(uint32_t(file)) << 32) | uint32_t(index);
}

std::string memoryKey(const char* raw) {
    if (!raw || !*raw) return {};
    auto path = std::filesystem::u8path(raw).lexically_normal();
    if (path.is_absolute()) path = path.lexically_relative(root());
    if (path.empty() || *path.begin() == "..") return {};
    return folded(utf8(path));
}

const MemoryFile* findMemoryFile(const char* raw) {
    auto found = memoryFiles.find(memoryKey(raw));
    return found == memoryFiles.end() ? nullptr : &found->second;
}

std::shared_ptr<MemoryPage> getPage(int file, int index, int expected) {
    std::unique_lock<std::mutex> lock(memoryMutex);
    if (memoryCancelled) return {};
    auto key = pageKey(file, index);
    auto found = pages.find(key);
    std::shared_ptr<MemoryPage> page;
    if (found == pages.end()) {
        page = std::make_shared<MemoryPage>();
        page->file = file;
        page->index = index;
        page->expected = expected;
        pages.emplace(key, page);
        requests.push_back(page);
    } else {
        page = found->second;
    }
    page->lastUse = ++useCounter;
    memoryReady.wait(lock, [&] { return memoryCancelled || page->finished; });
    if (memoryCancelled || page->bytes.empty()) return {};
    return page;
}
}

FITD_UNITY_API int FitdUnity_RegisterMemoryFile(const char* name, int id, uint64_t length) {
    if (FitdUnity_GetState() != 0 || !name || id < 0 || length > INT64_MAX) return 0;
    auto key = memoryKey(name);
    if (key.empty() || memoryLengths.count(id)) return 0;
    if (!memoryFiles.emplace(key, MemoryFile{id, length}).second) return 0;
    memoryLengths.emplace(id, length);
    return 1;
}

FITD_UNITY_API int FitdUnity_GetMemoryRequest(int* file, int* index, int* expected) {
    std::lock_guard<std::mutex> lock(memoryMutex);
    if (memoryCancelled || requests.empty()) return 0;
    const auto request = requests.front();
    requests.pop_front();
    *file = request->file;
    *index = request->index;
    *expected = request->expected;
    return 1;
}

FITD_UNITY_API void FitdUnity_CompleteMemoryRequest(int file, int index, const unsigned char* data, int length) {
    {
        std::lock_guard<std::mutex> lock(memoryMutex);
        const auto found = pages.find(pageKey(file,index));
        if (memoryCancelled || found == pages.end()) return;
        auto page = found->second;
        if (page->finished) return;
        if (data && length == page->expected) {
            page->bytes.assign(data, data + length);
            cacheBytes += page->bytes.size();
        }
        page->finished = true;
        while (cacheBytes > cacheBudget) {
            auto oldest = pages.end();
            for (auto it = pages.begin(); it != pages.end(); ++it) {
                if (it->second == page || it->second.use_count() != 1 || !it->second->finished) continue;
                if (oldest == pages.end() || it->second->lastUse < oldest->second->lastUse) oldest = it;
            }
            if (oldest == pages.end()) break;
            cacheBytes -= oldest->second->bytes.size();
            pages.erase(oldest);
        }
    }
    memoryReady.notify_all();
}

void fitdCancelMemoryReads() {
    {
        std::lock_guard<std::mutex> lock(memoryMutex);
        memoryCancelled = true;
        requests.clear();
    }
    memoryReady.notify_all();
}

bool fitdHasMemoryFile(const char* path) { return findMemoryFile(path) != nullptr; }

std::vector<std::string> fitdListMemoryFiles(const char* folder) {
    std::vector<std::string> result;
    auto prefix = memoryKey(folder);
    if (prefix.empty()) return result;
    prefix += '/';
    for (const auto& file : memoryFiles) {
        if (file.first.compare(0, prefix.size(), prefix) != 0) continue;
        auto name = file.first.substr(prefix.size());
        if (name.find('/') == std::string::npos) result.push_back(std::move(name));
    }
    return result;
}

struct FitdReadFile {
    FILE* disk = nullptr;
    const MemoryFile* memory = nullptr;
    uint64_t position = 0;
};

FitdReadFile* fitdOpenRead(const char* path, const char* mode) {
    auto key = memoryKey(path);
    const bool saved = !key.empty() && !saveRoot.empty() &&
        std::filesystem::is_regular_file(saveRoot / std::filesystem::u8path(key));
    if (!saved) {
        if (auto* memory = findMemoryFile(path)) return new FitdReadFile{nullptr, memory, 0};
    }
    auto* disk = fitdFopen(path, mode);
    return disk ? new FitdReadFile{disk, nullptr, 0} : nullptr;
}

size_t fitdRead(void* data, size_t size, size_t count, FitdReadFile* file) {
    if (file->disk) return std::fread(data, size, count, file->disk);
    if (!size || !count || file->position >= file->memory->length) return 0;
    uint64_t available = file->memory->length - file->position;
    size_t remaining = size * std::min<uint64_t>(count, available / size);
    size_t copied = 0;
    while (remaining) {
        const auto position = file->position;
        const auto pageOffset = position / pageSize * pageSize;
        const auto expected = int(std::min(pageSize, file->memory->length - pageOffset));
        auto page = getPage(file->memory->id, int(position / pageSize), expected);
        if (!page) {
            std::memset(static_cast<unsigned char*>(data) + copied, 0, remaining);
            break;
        }
        const auto offset = size_t(position - pageOffset);
        const auto amount = std::min(remaining, page->bytes.size() - offset);
        std::memcpy(static_cast<unsigned char*>(data) + copied, page->bytes.data() + offset, amount);
        copied += amount;
        remaining -= amount;
        file->position += amount;
    }
    return copied / size;
}

int fitdSeek(FitdReadFile* file, int64_t offset, int origin) {
    if (file->disk) {
#ifdef _WIN32
        return _fseeki64(file->disk, offset, origin);
#else
        return fseeko(file->disk, offset, origin);
#endif
    }
    const int64_t base = origin == SEEK_SET ? 0 : origin == SEEK_CUR ? int64_t(file->position) :
                         origin == SEEK_END ? int64_t(file->memory->length) : -1;
    if (base < 0 || offset < -base || offset > INT64_MAX - base) return -1;
    file->position = uint64_t(base + offset);
    return 0;
}

int64_t fitdTell(FitdReadFile* file) {
    if (!file->disk) return int64_t(file->position);
#ifdef _WIN32
    return _ftelli64(file->disk);
#else
    return ftello(file->disk);
#endif
}

int fitdClose(FitdReadFile* file) {
    const int result = file->disk ? std::fclose(file->disk) : 0;
    delete file;
    return result;
}
