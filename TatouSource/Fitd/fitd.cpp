///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark 2
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Application entry point (WinMain and main)
///////////////////////////////////////////////////////////////////////////////

#include <cstdio>

extern "C" {
    int FitdMain(int argc, char* argv[]);
}

int FitdInit(int argc, char* argv[]);

#ifdef _WIN32
#include <windows.h>

static void setupConsole()
{
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE)
        return;

    // Enable virtual terminal sequences for color support
    DWORD mode = 0;
    GetConsoleMode(hOut, &mode);
    SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    // Set background to dark teal (RGB 0,128,128) and clear screen
    printf("\x1b[48;2;0;128;128m");
    printf("\x1b[37;1m"); // bright white text
    printf("\x1b[2J\x1b[H"); // clear screen and move cursor home

    printf("\n");
    printf("       _    _     ___  _   _ _____   ___ _   _\n");
    printf("      / \\  | |   / _ \\| \\ | | ____| |_ _| \\ | |\n");
    printf("     / _ \\ | |  | | | |  \\| |  _|    | ||  \\| |\n");
    printf("    / ___ \\| |__| |_| | |\\  | |___   | || |\\  |\n");
    printf("   /_/   \\_\\____\\___/|_| \\_|_____| |___|_| \\_|\n");
    printf("\n");
    printf("    _____ _   _ _____   ____    _    ____  _  __\n");
    printf("   |_   _| | | | ____| |  _ \\  / \\  |  _ \\| |/ /\n");
    printf("     | | | |_| |  _|   | | | |/ _ \\ | |_) | ' / \n");
    printf("     | | |  _  | |___  | |_| / ___ \\|  _ <| . \\ \n");
    printf("     |_| |_| |_|_____| |____/_/   \\_\\_| \\_\\_|\\_\\\n");
    printf("\n");
    printf("    ____  _____      _   _    _   _   _ _   _ _____ _____ ____\n");
    printf("   |  _ \\| ____|    | | | |  / \\ | | | | \\ | |_   _| ____|  _ \\ \n");
    printf("   | |_) |  _| ___ | |_| | / _ \\| | | |  \\| | | | |  _| | | | |\n");
    printf("   |  _ <| |__|___||  _  |/ ___ \\ |_| | |\\  | | | | |___| |_| |\n");
    printf("   |_| \\_\\_____|   |_| |_/_/   \\_\\___/|_| \\_| |_| |_____|____/ \n");
    printf("\n");
    printf("   -----------------------------------------------------------------\n");
    printf("\n");
    printf("\x1b[0m\x1b[48;2;0;128;128m"); // reset bold, keep background
}

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
#ifdef _WIN32
    // Hide the console window at startup; it will be shown after the main window is created
    HWND hConsole = GetConsoleWindow();
    if (hConsole)
        ShowWindow(hConsole, SW_HIDE);

    setupConsole();
#endif

    FitdInit(argc, argv);
    FitdMain(argc, argv);
    return 0;
}
