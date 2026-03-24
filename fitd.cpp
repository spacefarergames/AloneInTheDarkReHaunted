///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Application entry point (WinMain and main)
///////////////////////////////////////////////////////////////////////////////

extern "C" {
    int FitdMain(int argc, char* argv[]);
}

int FitdInit(int argc, char* argv[]);

#ifdef _WIN32
#include <windows.h>

// Windows subsystem entry point (no console window)
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    FitdInit(0, nullptr);
    FitdMain(0, nullptr);
    return 0;
}
#endif

// Console subsystem entry point (for debug builds)
int main(int argc, char* argv[])
{
    FitdInit(argc, argv);
    FitdMain(argc, argv);
    return 0;
}
