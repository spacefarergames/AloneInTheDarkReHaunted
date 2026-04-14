///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Auto-detect and copy original AITD game data (PAK/ITD files) from
// Steam, GOG, or CD-ROM into the game's working directory.
///////////////////////////////////////////////////////////////////////////////

#include "gameDataCopy.h"
#include "consoleLog.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <shlwapi.h>
#include <commctrl.h>
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "comctl32.lib")

// ---------------------------------------------------------------------------
// Progress window
// ---------------------------------------------------------------------------

struct CopyProgressWindow
{
    HWND hWnd;
    HWND hBar;
    HWND hLabel;
    int  total;
    int  current;
};

static void pumpMessages()
{
    MSG msg;
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

static CopyProgressWindow createProgressWindow(const char* title, int totalFiles)
{
    CopyProgressWindow pw = {};
    pw.total = totalFiles > 0 ? totalFiles : 1;
    pw.current = 0;

    INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_PROGRESS_CLASS };
    InitCommonControlsEx(&icex);

    const int clientW = 460, clientH = 100;
    DWORD style   = WS_POPUP | WS_CAPTION | WS_VISIBLE;
    DWORD exStyle = WS_EX_TOPMOST;

    RECT rc = { 0, 0, clientW, clientH };
    AdjustWindowRectEx(&rc, style, FALSE, exStyle);
    int W = rc.right - rc.left;
    int H = rc.bottom - rc.top;

    int sx = GetSystemMetrics(SM_CXSCREEN);
    int sy = GetSystemMetrics(SM_CYSCREEN);

    pw.hWnd = CreateWindowExA(
        exStyle, "STATIC", title,
        style,
        (sx - W) / 2, (sy - H) / 2, W, H,
        NULL, NULL, GetModuleHandleA(NULL), NULL);

    pw.hLabel = CreateWindowExA(
        0, "STATIC", "Preparing...",
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
        12, 10, clientW - 30, 20,
        pw.hWnd, NULL, GetModuleHandleA(NULL), NULL);

    pw.hBar = CreateWindowExA(
        0, PROGRESS_CLASSA, NULL,
        WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
        12, 38, clientW - 30, 24,
        pw.hWnd, NULL, GetModuleHandleA(NULL), NULL);

    SendMessageA(pw.hBar, PBM_SETRANGE32, 0, (LPARAM)pw.total);
    SendMessageA(pw.hBar, PBM_SETSTEP, 1, 0);

    pumpMessages();
    return pw;
}

static void updateProgress(CopyProgressWindow* pw, const char* fileName)
{
    if (!pw || !pw->hWnd)
        return;

    pw->current++;
    SendMessageA(pw->hBar, PBM_SETPOS, (WPARAM)pw->current, 0);

    char label[MAX_PATH + 32];
    _snprintf(label, sizeof(label), "Copying %s  (%d / %d)", fileName, pw->current, pw->total);
    label[sizeof(label) - 1] = '\0';
    SetWindowTextA(pw->hLabel, label);

    pumpMessages();
}

static void destroyProgressWindow(CopyProgressWindow* pw)
{
    if (pw && pw->hWnd)
    {
        DestroyWindow(pw->hWnd);
        pw->hWnd = NULL;
        pumpMessages();
    }
}

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
static const unsigned int STEAM_APP_ID = 548090;
static const char* GOG_GAME_ID        = "1207660923";

// Sentinel file — if this exists in the target directory, we assume all
// game data is present and skip the copy step.
static const char* SENTINEL_FILE      = "ITD_RESS.PAK";

// The original DOS game stores data in an INDARK subdirectory.
static const char* INDARK_SUBDIR      = "INDARK";

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

extern "C" {
    extern char homePath[512];
}

// Build a full path from directory + filename into buf (size chars).
static void buildPath(char* buf, size_t size, const char* dir, const char* file)
{
    _snprintf(buf, size, "%s\\%s", dir, file);
    buf[size - 1] = '\0';
}

// Return true if the file exists on disk.
static bool fileExists(const char* path)
{
    DWORD attr = GetFileAttributesA(path);
    return (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
}

// Return true if the directory exists on disk.
static bool dirExists(const char* path)
{
    DWORD attr = GetFileAttributesA(path);
    return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY));
}

// Check if the sentinel file exists in `dir`.
static bool hasSentinel(const char* dir)
{
    char path[MAX_PATH];
    buildPath(path, sizeof(path), dir, SENTINEL_FILE);
    return fileExists(path);
}

// Count *.PAK and *.ITD files in `srcDir`.
static int countDataFiles(const char* srcDir)
{
    int count = 0;
    const char* patterns[] = { "\\*.PAK", "\\*.ITD" };
    for (int p = 0; p < 2; p++)
    {
        char searchPattern[MAX_PATH];
        _snprintf(searchPattern, sizeof(searchPattern), "%s%s", srcDir, patterns[p]);
        searchPattern[sizeof(searchPattern) - 1] = '\0';

        WIN32_FIND_DATAA fd;
        HANDLE hFind = FindFirstFileA(searchPattern, &fd);
        if (hFind == INVALID_HANDLE_VALUE)
            continue;
        do
        {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                count++;
        }
        while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }
    return count;
}

// Count all files recursively in `srcDir`.
static int countDirectoryFilesRecursive(const char* srcDir)
{
    int count = 0;
    char searchPattern[MAX_PATH];
    _snprintf(searchPattern, sizeof(searchPattern), "%s\\*", srcDir);
    searchPattern[sizeof(searchPattern) - 1] = '\0';

    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(searchPattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE)
        return 0;

    do
    {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            if (strcmp(fd.cFileName, ".") != 0 && strcmp(fd.cFileName, "..") != 0)
            {
                char sub[MAX_PATH];
                buildPath(sub, sizeof(sub), srcDir, fd.cFileName);
                count += countDirectoryFilesRecursive(sub);
            }
        }
        else
        {
            count++;
        }
    }
    while (FindNextFileA(hFind, &fd));
    FindClose(hFind);
    return count;
}

// Copy a single file.  Returns true on success.
static bool copyFile(const char* src, const char* dst)
{
    return CopyFileA(src, dst, FALSE) != 0;
}

// Copy all *.PAK and *.ITD files from `srcDir` into `dstDir`.
// Returns the number of files successfully copied.
static int copyDataFiles(const char* srcDir, const char* dstDir, CopyProgressWindow* pw = nullptr)
{
    int copied = 0;

    const char* patterns[] = { "\\*.PAK", "\\*.ITD" };
    for (int p = 0; p < 2; p++)
    {
        char searchPattern[MAX_PATH];
        _snprintf(searchPattern, sizeof(searchPattern), "%s%s", srcDir, patterns[p]);
        searchPattern[sizeof(searchPattern) - 1] = '\0';

        WIN32_FIND_DATAA fd;
        HANDLE hFind = FindFirstFileA(searchPattern, &fd);
        if (hFind == INVALID_HANDLE_VALUE)
            continue;

        do
        {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                continue;

            char srcPath[MAX_PATH];
            char dstPath[MAX_PATH];
            buildPath(srcPath, sizeof(srcPath), srcDir, fd.cFileName);
            buildPath(dstPath, sizeof(dstPath), dstDir, fd.cFileName);

            if (pw)
                updateProgress(pw, fd.cFileName);

            if (copyFile(srcPath, dstPath))
            {
                printf(DATA_OK "  Copied %s\n", fd.cFileName);
                copied++;
            }
            else
            {
                printf(DATA_WARN "  Failed to copy %s (err %lu)" CON_RESET "\n",
                       fd.cFileName, GetLastError());
            }
        }
        while (FindNextFileA(hFind, &fd));

        FindClose(hFind);
    }

    return copied;
}

// Copy all files (non-recursive) from srcDir to dstDir.
// Returns the number of files successfully copied.
static int copyAllFilesInDir(const char* srcDir, const char* dstDir, CopyProgressWindow* pw = nullptr)
{
    int copied = 0;

    char searchPattern[MAX_PATH];
    _snprintf(searchPattern, sizeof(searchPattern), "%s\\*", srcDir);
    searchPattern[sizeof(searchPattern) - 1] = '\0';

    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(searchPattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE)
        return 0;

    do
    {
        // Skip directories (handled separately for recursion)
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;

        char srcPath[MAX_PATH];
        char dstPath[MAX_PATH];
        buildPath(srcPath, sizeof(srcPath), srcDir, fd.cFileName);
        buildPath(dstPath, sizeof(dstPath), dstDir, fd.cFileName);

        if (pw)
            updateProgress(pw, fd.cFileName);

        if (copyFile(srcPath, dstPath))
            copied++;
        else
            printf(DATA_WARN "  Failed to deploy %s (err %lu)" CON_RESET "\n",
                   fd.cFileName, GetLastError());
    }
    while (FindNextFileA(hFind, &fd));

    FindClose(hFind);
    return copied;
}

// Recursively copy all files and subdirectories from srcDir to dstDir.
static int copyDirectoryRecursive(const char* srcDir, const char* dstDir, CopyProgressWindow* pw = nullptr)
{
    // Ensure destination directory exists
    CreateDirectoryA(dstDir, nullptr);

    int copied = copyAllFilesInDir(srcDir, dstDir, pw);

    // Now recurse into subdirectories
    char searchPattern[MAX_PATH];
    _snprintf(searchPattern, sizeof(searchPattern), "%s\\*", srcDir);
    searchPattern[sizeof(searchPattern) - 1] = '\0';

    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(searchPattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE)
        return copied;

    do
    {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            continue;
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
            continue;

        char srcSub[MAX_PATH];
        char dstSub[MAX_PATH];
        buildPath(srcSub, sizeof(srcSub), srcDir, fd.cFileName);
        buildPath(dstSub, sizeof(dstSub), dstDir, fd.cFileName);

        copied += copyDirectoryRecursive(srcSub, dstSub, pw);
    }
    while (FindNextFileA(hFind, &fd));

    FindClose(hFind);
    return copied;
}

// Return true if two paths refer to the same directory (case-insensitive).
static bool isSameDirectory(const char* a, const char* b)
{
    char fullA[MAX_PATH] = "";
    char fullB[MAX_PATH] = "";
    GetFullPathNameA(a, MAX_PATH, fullA, nullptr);
    GetFullPathNameA(b, MAX_PATH, fullB, nullptr);
    return _stricmp(fullA, fullB) == 0;
}

// Deploy the remaster into the Steam/GOG install directory so that the
// DOSBox.exe stub will launch our version next time.
//   sourceDataDir = the INDARK directory where we found PAK/ITD files
//   gameDir       = our current working directory (the remaster build)
static void deployRemasterToInstall(const char* sourceDataDir, const char* gameDir)
{
    if (isSameDirectory(sourceDataDir, gameDir))
    {
        printf(DATA_TAG "Already running from install directory — skipping deployment\n");
        return;
    }

    printf(DATA_TAG "Deploying Re-Haunted to %s ...\n", sourceDataDir);

    // Count files first, then copy with progress
    int totalFiles = countDirectoryFilesRecursive(gameDir);
    CopyProgressWindow pw = createProgressWindow("Deploying Re-Haunted...", totalFiles);

    int deployed = copyDirectoryRecursive(gameDir, sourceDataDir, &pw);

    destroyProgressWindow(&pw);
    printf(DATA_OK "Deployed %d file(s) to %s\n", deployed, sourceDataDir);

    // Also copy DOSBox.exe into the sibling DOSBOX directory so that
    // Steam/GOG launches our stub instead of the original DOSBox emulator.
    // Layout: <install>/DOSBOX/DOSBox.exe  ← stub
    //         <install>/INDARK/Tatou.exe   ← remaster

    // Find the DOSBOX sibling directory (go up from INDARK, then into DOSBOX)
    char installRoot[MAX_PATH];
    strncpy(installRoot, sourceDataDir, sizeof(installRoot));
    installRoot[sizeof(installRoot) - 1] = '\0';

    // Strip trailing slashes
    size_t len = strlen(installRoot);
    while (len > 0 && (installRoot[len - 1] == '\\' || installRoot[len - 1] == '/'))
        installRoot[--len] = '\0';

    // Go up one level (remove INDARK)
    char* lastSep = strrchr(installRoot, '\\');
    if (!lastSep)
        lastSep = strrchr(installRoot, '/');
    if (lastSep)
        *lastSep = '\0';

    char dosboxDir[MAX_PATH];
    _snprintf(dosboxDir, sizeof(dosboxDir), "%s\\DOSBOX", installRoot);
    dosboxDir[sizeof(dosboxDir) - 1] = '\0';

    if (dirExists(dosboxDir))
    {
        // Copy DOSBox.exe from the deployed INDARK to DOSBOX
        char srcStub[MAX_PATH];
        char dstStub[MAX_PATH];
        buildPath(srcStub, sizeof(srcStub), sourceDataDir, "DOSBox.exe");
        buildPath(dstStub, sizeof(dstStub), dosboxDir, "DOSBox.exe");

        if (fileExists(srcStub))
        {
            if (copyFile(srcStub, dstStub))
                printf(DATA_OK "Deployed DOSBox.exe stub to %s\n", dosboxDir);
            else
                printf(DATA_WARN "Failed to deploy DOSBox.exe to %s (err %lu)" CON_RESET "\n",
                       dosboxDir, GetLastError());
        }
        else
        {
            // Try from the game directory directly
            buildPath(srcStub, sizeof(srcStub), gameDir, "DOSBox.exe");
            if (fileExists(srcStub))
            {
                if (copyFile(srcStub, dstStub))
                    printf(DATA_OK "Deployed DOSBox.exe stub to %s\n", dosboxDir);
                else
                    printf(DATA_WARN "Failed to deploy DOSBox.exe to %s (err %lu)" CON_RESET "\n",
                           dosboxDir, GetLastError());
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Steam detection
// ---------------------------------------------------------------------------

// Read the Steam install path from the Windows registry.
static bool getSteamInstallPath(char* buf, size_t size)
{
    const char* keys[] = {
        "SOFTWARE\\Valve\\Steam",
        "SOFTWARE\\WOW6432Node\\Valve\\Steam"
    };

    for (int i = 0; i < 2; i++)
    {
        HKEY hKey;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, keys[i], 0, KEY_READ, &hKey) == ERROR_SUCCESS)
        {
            DWORD type = 0;
            DWORD cbData = (DWORD)size;
            LONG result = RegQueryValueExA(hKey, "InstallPath", nullptr, &type, (LPBYTE)buf, &cbData);
            RegCloseKey(hKey);
            if (result == ERROR_SUCCESS && type == REG_SZ && cbData > 1)
                return true;
        }
    }
    return false;
}

// Simple VDF parser — extract all "path" values from libraryfolders.vdf
// and store them in `paths` (up to maxPaths).  Returns number found.
static int parseSteamLibraryFolders(const char* vdfPath, char paths[][MAX_PATH], int maxPaths)
{
    FILE* f = fopen(vdfPath, "r");
    if (!f)
        return 0;

    int count = 0;
    char line[1024];

    while (fgets(line, sizeof(line), f) && count < maxPaths)
    {
        // Look for lines like:   "path"    "D:\\SteamLibrary"
        char* pathKey = strstr(line, "\"path\"");
        if (!pathKey)
            continue;

        // Find the value — the second quoted string after "path"
        char* firstQuote = strchr(pathKey + 6, '"');
        if (!firstQuote)
            continue;
        firstQuote++; // skip opening quote

        char* endQuote = strchr(firstQuote, '"');
        if (!endQuote)
            continue;

        size_t len = endQuote - firstQuote;
        if (len >= MAX_PATH)
            len = MAX_PATH - 1;

        // Copy and unescape backslashes
        int j = 0;
        for (size_t i = 0; i < len && j < MAX_PATH - 1; i++)
        {
            if (firstQuote[i] == '\\' && i + 1 < len && firstQuote[i + 1] == '\\')
            {
                paths[count][j++] = '\\';
                i++; // skip second backslash
            }
            else
            {
                paths[count][j++] = firstQuote[i];
            }
        }
        paths[count][j] = '\0';
        count++;
    }

    fclose(f);
    return count;
}

// Try to find the original AITD data in Steam library folders.
// Returns true and sets `outDir` to the INDARK data directory if found.
static bool findSteamDataDir(char* outDir, size_t outSize)
{
    char steamPath[MAX_PATH];
    if (!getSteamInstallPath(steamPath, sizeof(steamPath)))
    {
        printf(DATA_TAG "Steam not found in registry\n");
        return false;
    }

    printf(DATA_TAG "Steam install: %s\n", steamPath);

    // Parse library folders
    char vdfPath[MAX_PATH];
    _snprintf(vdfPath, sizeof(vdfPath), "%s\\steamapps\\libraryfolders.vdf", steamPath);
    vdfPath[sizeof(vdfPath) - 1] = '\0';

    char libraryPaths[16][MAX_PATH];
    int numLibraries = parseSteamLibraryFolders(vdfPath, libraryPaths, 16);

    if (numLibraries == 0)
    {
        printf(DATA_TAG "No Steam library folders found in %s\n", vdfPath);
        return false;
    }

    // For each library, check for the app manifest to get the install dir name,
    // then check for INDARK subfolder with the sentinel file.
    for (int i = 0; i < numLibraries; i++)
    {
        // Check if the app manifest exists in this library
        char manifestPath[MAX_PATH];
        _snprintf(manifestPath, sizeof(manifestPath), "%s\\steamapps\\appmanifest_%u.acf",
                  libraryPaths[i], STEAM_APP_ID);
        manifestPath[sizeof(manifestPath) - 1] = '\0';

        if (!fileExists(manifestPath))
            continue;

        // Parse installdir from manifest
        char installDir[MAX_PATH] = "";
        FILE* mf = fopen(manifestPath, "r");
        if (mf)
        {
            char mline[1024];
            while (fgets(mline, sizeof(mline), mf))
            {
                char* key = strstr(mline, "\"installdir\"");
                if (!key)
                    continue;

                char* q1 = strchr(key + 12, '"');
                if (!q1) break;
                q1++;
                char* q2 = strchr(q1, '"');
                if (!q2) break;

                size_t len = q2 - q1;
                if (len >= MAX_PATH) len = MAX_PATH - 1;
                memcpy(installDir, q1, len);
                installDir[len] = '\0';
                break;
            }
            fclose(mf);
        }

        if (installDir[0] == '\0')
            continue;

        // Build the full path: <library>/steamapps/common/<installdir>/INDARK
        char candidate[MAX_PATH];
        _snprintf(candidate, sizeof(candidate), "%s\\steamapps\\common\\%s\\%s",
                  libraryPaths[i], installDir, INDARK_SUBDIR);
        candidate[sizeof(candidate) - 1] = '\0';

        if (hasSentinel(candidate))
        {
            printf(DATA_OK "Found AITD data in Steam: %s\n", candidate);
            strncpy(outDir, candidate, outSize);
            outDir[outSize - 1] = '\0';
            return true;
        }

        // Also check the game root (some installations put files directly there)
        _snprintf(candidate, sizeof(candidate), "%s\\steamapps\\common\\%s",
                  libraryPaths[i], installDir);
        candidate[sizeof(candidate) - 1] = '\0';

        if (hasSentinel(candidate))
        {
            printf(DATA_OK "Found AITD data in Steam (root): %s\n", candidate);
            strncpy(outDir, candidate, outSize);
            outDir[outSize - 1] = '\0';
            return true;
        }
    }

    printf(DATA_TAG "AITD data not found in any Steam library\n");
    return false;
}

// ---------------------------------------------------------------------------
// GOG detection
// ---------------------------------------------------------------------------

static bool findGogDataDir(char* outDir, size_t outSize)
{
    // Try registry first
    const char* regKeys[] = {
        "SOFTWARE\\GOG.com\\Games\\" ,
        "SOFTWARE\\WOW6432Node\\GOG.com\\Games\\"
    };

    for (int i = 0; i < 2; i++)
    {
        char keyPath[MAX_PATH];
        _snprintf(keyPath, sizeof(keyPath), "%s%s", regKeys[i], GOG_GAME_ID);
        keyPath[sizeof(keyPath) - 1] = '\0';

        HKEY hKey;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, keyPath, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
        {
            char gogPath[MAX_PATH] = "";
            DWORD type = 0;
            DWORD cbData = sizeof(gogPath);
            LONG result = RegQueryValueExA(hKey, "PATH", nullptr, &type, (LPBYTE)gogPath, &cbData);
            RegCloseKey(hKey);

            if (result == ERROR_SUCCESS && type == REG_SZ && gogPath[0] != '\0')
            {
                printf(DATA_TAG "GOG registry path: %s\n", gogPath);

                // Check INDARK subfolder
                char candidate[MAX_PATH];
                _snprintf(candidate, sizeof(candidate), "%s\\%s", gogPath, INDARK_SUBDIR);
                candidate[sizeof(candidate) - 1] = '\0';

                if (hasSentinel(candidate))
                {
                    printf(DATA_OK "Found AITD data in GOG: %s\n", candidate);
                    strncpy(outDir, candidate, outSize);
                    outDir[outSize - 1] = '\0';
                    return true;
                }

                // Check root
                if (hasSentinel(gogPath))
                {
                    printf(DATA_OK "Found AITD data in GOG (root): %s\n", gogPath);
                    strncpy(outDir, gogPath, outSize);
                    outDir[outSize - 1] = '\0';
                    return true;
                }
            }
        }
    }

    // Try common GOG install directories
    const char* commonPaths[] = {
        "C:\\GOG Games\\Alone in the Dark",
        "C:\\Program Files (x86)\\GOG Galaxy\\Games\\Alone in the Dark",
        "C:\\Program Files\\GOG Galaxy\\Games\\Alone in the Dark",
        "D:\\GOG Games\\Alone in the Dark",
    };

    for (int i = 0; i < 4; i++)
    {
        if (!dirExists(commonPaths[i]))
            continue;

        char candidate[MAX_PATH];
        _snprintf(candidate, sizeof(candidate), "%s\\%s", commonPaths[i], INDARK_SUBDIR);
        candidate[sizeof(candidate) - 1] = '\0';

        if (hasSentinel(candidate))
        {
            printf(DATA_OK "Found AITD data in GOG: %s\n", candidate);
            strncpy(outDir, candidate, outSize);
            outDir[outSize - 1] = '\0';
            return true;
        }

        if (hasSentinel(commonPaths[i]))
        {
            printf(DATA_OK "Found AITD data in GOG (root): %s\n", commonPaths[i]);
            strncpy(outDir, commonPaths[i], outSize);
            outDir[outSize - 1] = '\0';
            return true;
        }
    }

    printf(DATA_TAG "AITD data not found in GOG installations\n");
    return false;
}

// ---------------------------------------------------------------------------
// CD/DVD detection
// ---------------------------------------------------------------------------

static bool findCdDataDir(char* outDir, size_t outSize)
{
    // Enumerate all drive letters and check for CD/DVD drives
    char drivePath[] = "A:\\";
    for (char letter = 'D'; letter <= 'Z'; letter++)
    {
        drivePath[0] = letter;
        UINT driveType = GetDriveTypeA(drivePath);
        if (driveType != DRIVE_CDROM)
            continue;

        printf(DATA_TAG "Checking CD drive %c:\\\n", letter);

        // Check INDARK subfolder on the disc
        char candidate[MAX_PATH];
        _snprintf(candidate, sizeof(candidate), "%c:\\%s", letter, INDARK_SUBDIR);
        candidate[sizeof(candidate) - 1] = '\0';

        if (hasSentinel(candidate))
        {
            printf(DATA_OK "Found AITD data on CD: %s\n", candidate);
            strncpy(outDir, candidate, outSize);
            outDir[outSize - 1] = '\0';
            return true;
        }

        // Check root of disc
        char rootPath[8];
        _snprintf(rootPath, sizeof(rootPath), "%c:\\", letter);
        rootPath[sizeof(rootPath) - 1] = '\0';

        if (hasSentinel(rootPath))
        {
            printf(DATA_OK "Found AITD data on CD (root): %s\n", rootPath);
            strncpy(outDir, rootPath, outSize);
            outDir[outSize - 1] = '\0';
            return true;
        }
    }

    printf(DATA_TAG "AITD data not found on any CD/DVD drive\n");
    return false;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool gameDataCopy_EnsureDataFiles()
{
    // Determine the target directory (homePath or current working directory)
    char targetDir[MAX_PATH];
    if (homePath[0] != '\0')
    {
        strncpy(targetDir, homePath, sizeof(targetDir));
        targetDir[sizeof(targetDir) - 1] = '\0';
        // Strip trailing slash/backslash for consistency
        size_t len = strlen(targetDir);
        while (len > 0 && (targetDir[len - 1] == '\\' || targetDir[len - 1] == '/'))
            targetDir[--len] = '\0';
    }
    else
    {
        GetCurrentDirectoryA(sizeof(targetDir), targetDir);
    }

    printf(DATA_TAG "Game data directory: %s\n", targetDir);

    // Always search for Steam/GOG install so we can deploy back to it
    char sourceDir[MAX_PATH] = "";
    bool foundSource = false;

    foundSource = findSteamDataDir(sourceDir, sizeof(sourceDir));
    if (!foundSource)
        foundSource = findGogDataDir(sourceDir, sizeof(sourceDir));

    // Check if sentinel file already exists in our working directory
    if (hasSentinel(targetDir))
    {
        printf(DATA_OK "Game data files already present\n");

        // Deploy the remaster back to the install directory so that
        // Steam/GOG will launch our version next time
        if (foundSource)
            deployRemasterToInstall(sourceDir, targetDir);

        return true;
    }

    printf(DATA_TAG "Game data files not found — searching for original AITD installation...\n");

    // Also try CD/DVD if Steam/GOG didn't have the data
    if (!foundSource)
        foundSource = findCdDataDir(sourceDir, sizeof(sourceDir));

    if (!foundSource)
    {
        printf(DATA_WARN "No original AITD installation found — game will use embedded data" CON_RESET "\n");
        return false;
    }

    // Copy PAK/ITD files from source to our working directory
    printf(DATA_TAG "Copying game data from %s to %s ...\n", sourceDir, targetDir);

    int totalFiles = countDataFiles(sourceDir);
    CopyProgressWindow pw = createProgressWindow("Copying Game Data...", totalFiles);

    int copied = copyDataFiles(sourceDir, targetDir, &pw);

    destroyProgressWindow(&pw);

    if (copied > 0)
    {
        printf(DATA_OK "Copied %d game data file(s) successfully\n", copied);

        // Deploy the remaster back to the install directory
        deployRemasterToInstall(sourceDir, targetDir);

        return true;
    }
    else
    {
        printf(DATA_WARN "No files were copied — game will recomp instead" CON_RESET "\n");
        return false;
    }
}

#else
// ---- Non-Windows stub ----
bool gameDataCopy_EnsureDataFiles() { return false; }

#endif // _WIN32
