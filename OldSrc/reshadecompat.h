///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// ReShade compatibility diagnostics
///////////////////////////////////////////////////////////////////////////////

#pragma once

// ReShade Compatibility Diagnostics
// Include this in main.cpp or osystemSDL.cpp before initialization

#ifdef WIN32
#include <windows.h>
#include <stdio.h>

// Simple crash handler to help diagnose ReShade conflicts
LONG WINAPI ReShadeCrashHandler(EXCEPTION_POINTERS* ExceptionInfo)
{
    FILE* f = fopen("crash_log.txt", "w");
    if (f)
    {
        fprintf(f, "=== ALONE IN THE DARK Crash Report ===\n");
        fprintf(f, "Exception Code: 0x%08X\n", ExceptionInfo->ExceptionRecord->ExceptionCode);
        fprintf(f, "Exception Address: 0x%p\n", ExceptionInfo->ExceptionRecord->ExceptionAddress);
        fprintf(f, "\nCommon causes with ReShade:\n");
        fprintf(f, "- 0xC0000005 (Access Violation): Hook conflict or incorrect DLL\n");
        fprintf(f, "- 0xC000001D (Illegal Instruction): Wrong renderer type\n");
        fprintf(f, "- 0xC0000409 (Stack corruption): Buffer overflow in shader compilation\n");
        fprintf(f, "\nSuggested fixes:\n");
        fprintf(f, "1. Rename dxgi.dll to d3d11.dll\n");
        fprintf(f, "2. Try command line: -d3d11 or -opengl\n");
        fprintf(f, "3. Disable depth buffer effects in ReShade\n");
        fprintf(f, "4. Use ReShade 5.9+ for better BGFX compatibility\n");
        fclose(f);
        
        MessageBoxA(NULL, 
            "Game crashed! Check crash_log.txt for details.\n\n"
            "Please report this to Spacefarer Games\n"
            "- Include memory log\n"
            "- Crash logs\n"
            "- Send to jake@spacefarergames.com",
            "ALONE IN THE DARK Crash",
            MB_OK | MB_ICONERROR);
    }
    
    return EXCEPTION_EXECUTE_HANDLER;
}

// Call this early in main() or FitdInit()
inline void InitReShadeCompatibility()
{
    // Set up crash handler
    SetUnhandledExceptionFilter(ReShadeCrashHandler);
    
    // Check for ReShade DLLs
    bool foundDXGI = GetModuleHandleA("dxgi.dll") != NULL;
    bool foundD3D11 = GetModuleHandleA("d3d11.dll") != NULL;
    
    if (foundDXGI || foundD3D11)
    {
        printf("=== ReShade Detected ===\n");
        if (foundDXGI) printf("Found: dxgi.dll\n");
        if (foundD3D11) printf("Found: d3d11.dll\n");
        printf("ReShade compatibility mode enabled.\n");
        printf("If crashes occur, check crash_log.txt\n");
        printf("========================\n");
    }
    
    // Set environment variables that can help ReShade
    SetEnvironmentVariableA("RESHADE_DISABLE_PERFORMANCE_MODE", "0");
    SetEnvironmentVariableA("RESHADE_DISABLE_DEPTH", "0"); // Try 1 if crashes
}

#else
// Non-Windows platforms
inline void InitReShadeCompatibility() 
{
    // ReShade is Windows-only
}
#endif
