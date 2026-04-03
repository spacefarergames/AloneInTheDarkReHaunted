/*
 * DOSBox.exe stub — replaces the DOSBox launcher shipped with Alone In The Dark
 * (Steam / GOG) so that launching through those platforms starts Tatou.exe
 * (the FITD remastered engine) instead.
 *
 * The stub resolves its own directory and executes Tatou.exe from the same
 * folder, forwarding any command-line arguments.
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

    std::string targetExe = dir + "Tatou.exe";

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
        dir.empty() ? nullptr : dir.c_str(), // working directory
        &si,
        &pi);

    if (!ok)
    {
        std::string msg = "Could not start Tatou.exe.\n\nMake sure Tatou.exe "
                          "is in the same directory as DOSBox.exe.\n\nLooked for:\n" +
                          targetExe;
        MessageBoxA(nullptr, msg.c_str(), "DOSBox Stub", MB_OK | MB_ICONERROR);
        return 1;
    }

    // We don't wait for Tatou — just let it run and exit the stub.
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return 0;
}
