/*
 * DOSBox.exe stub — replaces the DOSBox launcher shipped with Alone In The Dark
 * (Steam / GOG) so that launching through those platforms starts Tatou.exe
 * (the FITD remastered engine) instead.
 *
 * When deployed into a Steam/GOG install the layout is:
 *   <install>/DOSBOX/DOSBox.exe    ← this stub
 *   <install>/INDARK/Tatou.exe     ← the remaster
 *
 * The stub first checks for ..\INDARK\Tatou.exe (deployed layout) and falls
 * back to looking in its own directory (development / standalone layout).
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>

int WINAPI WinMain(HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/,
                   LPSTR lpCmdLine, int /*nCmdShow*/)
{
    // Resolve the directory this executable lives in.
    char modulePath[MAX_PATH];
    DWORD len = GetModuleFileNameA(nullptr, modulePath, MAX_PATH);
    if (len == 0 || len >= MAX_PATH)
    {
        MessageBoxA(nullptr, "Failed to determine executable path.",
                    "DOSBox Stub", MB_OK | MB_ICONERROR);
        return 1;
    }

    // Strip the filename to get the directory.
    std::string dir(modulePath);
    auto pos = dir.find_last_of("\\/");
    if (pos != std::string::npos)
        dir.erase(pos + 1); // keep trailing slash
    else
        dir.clear();

    // Try the deployed Steam/GOG layout first: ..\INDARK\Tatou.exe
    std::string indarkDir = dir + "..\\INDARK\\";
    std::string targetExe = indarkDir + "Tatou.exe";
    std::string workDir   = indarkDir;

    DWORD attr = GetFileAttributesA(targetExe.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES)
    {
        // Fallback: same directory as this stub (dev / standalone layout)
        targetExe = dir + "Tatou.exe";
        workDir   = dir;
    }

    // Build the command line: "Tatou.exe" followed by any original arguments.
    std::string cmdLine = "\"" + targetExe + "\"";
    if (lpCmdLine && lpCmdLine[0] != '\0')
    {
        cmdLine += " ";
        cmdLine += lpCmdLine;
    }

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};

    // CreateProcessA requires a mutable buffer for the command line.
    std::string cmdLineBuf = cmdLine;

    BOOL ok = CreateProcessA(
        targetExe.c_str(),        // application
        &cmdLineBuf[0],           // command line (mutable)
        nullptr,                  // process security
        nullptr,                  // thread security
        FALSE,                    // inherit handles
        0,                        // creation flags
        nullptr,                  // environment
        workDir.c_str(),          // working directory (INDARK or stub dir)
        &si,
        &pi);

    if (!ok)
    {
        std::string msg = "Could not start Tatou.exe.\n\n"
                          "Make sure Tatou.exe is in the same directory as "
                          "DOSBox.exe, or in ..\\INDARK\\.\n\nLooked for:\n" +
                          targetExe;
        MessageBoxA(nullptr, msg.c_str(), "DOSBox Stub", MB_OK | MB_ICONERROR);
        return 1;
    }

    // We don't wait for Tatou — just let it run and exit the stub.
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return 0;
}
