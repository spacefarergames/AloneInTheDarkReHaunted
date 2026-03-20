///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Exception and crash handler declarations
///////////////////////////////////////////////////////////////////////////////

#pragma once
#ifndef EXCEPTION_HANDLER_H
#define EXCEPTION_HANDLER_H

#ifdef _WIN32
#include <Windows.h>
#include <excpt.h>
#include <stdio.h>
#include <time.h>
#include <DbgHelp.h>
#include <crtdbg.h>

#pragma comment(lib, "Dbghelp.lib")

class ExceptionHandler
{
private:
    static inline FILE* crashLogFile = nullptr;
    static inline bool initialized = false;
    static inline bool continueOnCorruption = true;
    static inline PVOID vehHandle = nullptr;
    static inline volatile long heapWarningCount = 0;
    static inline volatile long breakpointSkipCount = 0;

    // Vectored Exception Handler - fires BEFORE all other handlers.
    // This is critical for catching __debugbreak() raised by the debug CRT
    // when it detects heap corruption during _free_dbg. The CRT issues an
    // int3 (STATUS_BREAKPOINT 0x80000003) which would otherwise kill the
    // process or trigger the debugger. We intercept it here, log it, and
    // skip past it so the DOS-era game code can keep running.
    static LONG WINAPI VectoredHandler(EXCEPTION_POINTERS* exceptionInfo)
    {
        DWORD code = exceptionInfo->ExceptionRecord->ExceptionCode;

        // STATUS_BREAKPOINT (0x80000003) - raised by __debugbreak() in CRT
        // debug heap when corruption is detected. Skip the int3 instruction
        // and continue execution so the game survives.
        if (code == STATUS_BREAKPOINT && continueOnCorruption)
        {
            long count = InterlockedIncrement(&breakpointSkipCount);
            // Log first few and then periodically to avoid flooding
            if (count <= 5 || (count % 100) == 0)
            {
                char msg[256];
                sprintf_s(msg, sizeof(msg),
                    "WARNING: CRT debug break #%ld at 0x%p skipped (DOS buffer overrun)\n",
                    count, exceptionInfo->ExceptionRecord->ExceptionAddress);
                OutputDebugStringA(msg);
                printf("%s", msg);

                LogExceptionQuiet(exceptionInfo, "CRT_BREAKPOINT_SKIP");
            }

            // Advance RIP past the int3 instruction (1 byte on x64 and x86)
#ifdef _M_AMD64
            exceptionInfo->ContextRecord->Rip += 1;
#else
            exceptionInfo->ContextRecord->Eip += 1;
#endif
            return EXCEPTION_CONTINUE_EXECUTION;
        }

        // STATUS_HEAP_CORRUPTION (0xC0000374) - sometimes raised directly
        // by ntdll RtlReportCriticalFailure instead of going through int3.
        if (code == 0xC0000374 && continueOnCorruption)
        {
            long count = InterlockedIncrement(&heapWarningCount);
            if (count <= 5 || (count % 100) == 0)
            {
                char msg[256];
                sprintf_s(msg, sizeof(msg),
                    "WARNING: Heap corruption #%ld detected and skipped (DOS buffer overrun)\n",
                    count);
                OutputDebugStringA(msg);
                printf("%s", msg);

                LogExceptionQuiet(exceptionInfo, "HEAP_CORRUPTION_SKIP");
            }

            return EXCEPTION_CONTINUE_EXECUTION;
        }

        // Everything else: let other handlers deal with it
        return EXCEPTION_CONTINUE_SEARCH;
    }

    static LONG WINAPI UnhandledExceptionFilter(EXCEPTION_POINTERS* exceptionInfo)
    {
        LogException(exceptionInfo, "UNHANDLED");

        // Show message box
        char message[512];
        sprintf_s(message, sizeof(message),
            "A critical error has occurred (0x%08lX).\n\n"
            "Details have been logged to crash_log.txt\n\n"
            "The application will now terminate.",
            exceptionInfo->ExceptionRecord->ExceptionCode);

        MessageBoxA(NULL, message, "Critical Error", MB_OK | MB_ICONERROR);

        return EXCEPTION_EXECUTE_HANDLER; // Terminate
    }

    // Lightweight log that avoids heavy symbol resolution (safe for VEH context)
    static void LogExceptionQuiet(EXCEPTION_POINTERS* exceptionInfo, const char* category)
    {
        if (!crashLogFile)
        {
            crashLogFile = fopen("crash_log.txt", "a");
        }
        if (!crashLogFile) return;

        time_t now = time(nullptr);
        char timeStr[64];
        ctime_s(timeStr, sizeof(timeStr), &now);
        timeStr[strlen(timeStr) - 1] = '\0';

        fprintf(crashLogFile, "[%s] %s: code=0x%08lX addr=0x%p\n",
            timeStr, category,
            exceptionInfo->ExceptionRecord->ExceptionCode,
            exceptionInfo->ExceptionRecord->ExceptionAddress);
        fflush(crashLogFile);
    }

#ifdef _DEBUG
    // CRT report hook - intercepts _CrtDbgReport calls (asserts, errors)
    // and suppresses the ones that would show dialogs or trigger breaks.
    static int __cdecl CrtReportHook(int reportType, char* message, int* returnValue)
    {
        if (!continueOnCorruption)
            return 0; // Let CRT handle it normally

        // Log the CRT message but don't break/abort
        if (crashLogFile || (crashLogFile = fopen("crash_log.txt", "a")) != nullptr)
        {
            fprintf(crashLogFile, "[CRT_REPORT type=%d] %s\n", reportType, message ? message : "(null)");
            fflush(crashLogFile);
        }

        if (message)
        {
            OutputDebugStringA("CRT suppressed: ");
            OutputDebugStringA(message);
        }

        // returnValue = 0 means "don't trigger __debugbreak()" in _CrtDbgReport
        if (returnValue)
            *returnValue = 0;

        return 1; // 1 = we handled it, don't call default CRT handler
    }
#endif

    static void LogException(EXCEPTION_POINTERS* exceptionInfo, const char* category)
    {
        if (!crashLogFile)
        {
            crashLogFile = fopen("crash_log.txt", "a");
        }

        if (!crashLogFile) return;

        time_t now = time(nullptr);
        char timeStr[64];
        ctime_s(timeStr, sizeof(timeStr), &now);
        timeStr[strlen(timeStr) - 1] = '\0'; // Remove newline

        fprintf(crashLogFile, "\n=== %s EXCEPTION: %s ===\n", category, timeStr);
        fprintf(crashLogFile, "Exception Code: 0x%08lX\n", exceptionInfo->ExceptionRecord->ExceptionCode);
        fprintf(crashLogFile, "Exception Address: 0x%p\n", exceptionInfo->ExceptionRecord->ExceptionAddress);
        fprintf(crashLogFile, "Exception Flags: 0x%08lX\n", exceptionInfo->ExceptionRecord->ExceptionFlags);

        // Decode common exception codes
        switch (exceptionInfo->ExceptionRecord->ExceptionCode)
        {
        case EXCEPTION_ACCESS_VIOLATION:
        {
            fprintf(crashLogFile, "Type: ACCESS VIOLATION\n");
            if (exceptionInfo->ExceptionRecord->NumberParameters >= 2)
            {
                ULONG_PTR accessType = exceptionInfo->ExceptionRecord->ExceptionInformation[0];
                ULONG_PTR address = exceptionInfo->ExceptionRecord->ExceptionInformation[1];
                fprintf(crashLogFile, "  %s at address 0x%p\n",
                    accessType == 0 ? "Read" : (accessType == 1 ? "Write" : "Execute"),
                    (void*)address);
            }
            break;
        }
        case 0xC0000374: // STATUS_HEAP_CORRUPTION
            fprintf(crashLogFile, "Type: HEAP CORRUPTION\n");
            fprintf(crashLogFile, "  The heap has been corrupted. This usually indicates:\n");
            fprintf(crashLogFile, "  - Buffer overrun/underrun\n");
            fprintf(crashLogFile, "  - Double-free\n");
            fprintf(crashLogFile, "  - Use-after-free\n");
            fprintf(crashLogFile, "  - Mismatched allocation/deallocation\n");
            break;
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
            fprintf(crashLogFile, "Type: ARRAY BOUNDS EXCEEDED\n");
            break;
        case EXCEPTION_DATATYPE_MISALIGNMENT:
            fprintf(crashLogFile, "Type: DATATYPE MISALIGNMENT\n");
            break;
        case EXCEPTION_ILLEGAL_INSTRUCTION:
            fprintf(crashLogFile, "Type: ILLEGAL INSTRUCTION\n");
            break;
        case EXCEPTION_STACK_OVERFLOW:
            fprintf(crashLogFile, "Type: STACK OVERFLOW\n");
            break;
        default:
            fprintf(crashLogFile, "Type: UNKNOWN (0x%08lX)\n", exceptionInfo->ExceptionRecord->ExceptionCode);
            break;
        }

        // Get stack trace
        fprintf(crashLogFile, "\nStack Trace:\n");
        
        CONTEXT context = *exceptionInfo->ContextRecord;
        STACKFRAME64 stackFrame;
        memset(&stackFrame, 0, sizeof(STACKFRAME64));

#ifdef _M_AMD64
        DWORD machineType = IMAGE_FILE_MACHINE_AMD64;
        stackFrame.AddrPC.Offset = context.Rip;
        stackFrame.AddrFrame.Offset = context.Rbp;
        stackFrame.AddrStack.Offset = context.Rsp;
#else
        DWORD machineType = IMAGE_FILE_MACHINE_I386;
        stackFrame.AddrPC.Offset = context.Eip;
        stackFrame.AddrFrame.Offset = context.Ebp;
        stackFrame.AddrStack.Offset = context.Esp;
#endif
        
        stackFrame.AddrPC.Mode = AddrModeFlat;
        stackFrame.AddrFrame.Mode = AddrModeFlat;
        stackFrame.AddrStack.Mode = AddrModeFlat;

        HANDLE process = GetCurrentProcess();
        HANDLE thread = GetCurrentThread();
        
        // Initialize symbol handler
        SymInitialize(process, NULL, TRUE);
        
        int frameNum = 0;
        while (StackWalk64(machineType, process, thread, &stackFrame, &context,
            NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL))
        {
            if (frameNum >= 20) break; // Limit stack depth
            
            fprintf(crashLogFile, "  [%d] 0x%016llX", frameNum, stackFrame.AddrPC.Offset);
            
            // Try to get symbol information
            char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
            PSYMBOL_INFO symbol = (PSYMBOL_INFO)buffer;
            symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
            symbol->MaxNameLen = MAX_SYM_NAME;
            
            DWORD64 displacement = 0;
            if (SymFromAddr(process, stackFrame.AddrPC.Offset, &displacement, symbol))
            {
                fprintf(crashLogFile, " - %s+0x%llX", symbol->Name, displacement);
            }
            
            // Try to get line information
            IMAGEHLP_LINE64 line;
            line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
            DWORD lineDisplacement = 0;
            if (SymGetLineFromAddr64(process, stackFrame.AddrPC.Offset, &lineDisplacement, &line))
            {
                fprintf(crashLogFile, " (%s:%d)", line.FileName, line.LineNumber);
            }
            
            fprintf(crashLogFile, "\n");
            frameNum++;
        }
        
        SymCleanup(process);

        fprintf(crashLogFile, "=== End of Exception Report ===\n\n");
        fflush(crashLogFile);
    }

public:
    static void Initialize(bool continueOnHeapCorruption = true)
    {
        if (initialized) return;

        initialized = true;
        continueOnCorruption = continueOnHeapCorruption;

        // Register Vectored Exception Handler FIRST (1 = first in chain).
        // VEH fires before SEH and before SetUnhandledExceptionFilter, so it
        // can intercept __debugbreak() from the debug CRT before the process
        // is killed. This is essential for surviving DOS-era buffer overruns.
        vehHandle = AddVectoredExceptionHandler(1, VectoredHandler);

        // Set unhandled exception filter as fallback for anything VEH doesn't catch
        SetUnhandledExceptionFilter(UnhandledExceptionFilter);

#ifdef _DEBUG
        if (continueOnHeapCorruption)
        {
            // Reduce CRT debug heap checking to avoid constant __debugbreak()
            // calls. Keep leak checking at exit but disable per-operation checks.
            int flags = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
            flags &= ~_CRTDBG_CHECK_ALWAYS_DF;  // Don't check heap on every alloc/free
            flags &= ~_CRTDBG_DELAY_FREE_MEM_DF; // Don't keep freed blocks
            flags |= _CRTDBG_ALLOC_MEM_DF;       // Keep basic allocation tracking
            _CrtSetDbgFlag(flags);

            // Install report hook to suppress CRT assert/error dialogs and breaks
            _CrtSetReportHook2(_CRT_RPTHOOK_INSTALL, CrtReportHook);

            // Redirect CRT reports to debug output instead of popup dialogs
            _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);
            _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
            _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_DEBUG);
        }
#endif

        // Create or clear crash log
        crashLogFile = fopen("crash_log.txt", "w");
        if (crashLogFile)
        {
            fprintf(crashLogFile, "=== Application Started - Exception Handler Active ===\n");
            fprintf(crashLogFile, "Continue on heap corruption: %s\n", continueOnHeapCorruption ? "YES" : "NO");
            fprintf(crashLogFile, "Vectored Exception Handler: %s\n", vehHandle ? "REGISTERED" : "FAILED");
#ifdef _DEBUG
            fprintf(crashLogFile, "CRT debug heap checks: REDUCED (DOS compatibility mode)\n");
            fprintf(crashLogFile, "CRT report hook: INSTALLED\n");
#endif
            fprintf(crashLogFile, "\n");
            fclose(crashLogFile);
            crashLogFile = nullptr; // Will be reopened on exception
        }
    }

    static void Shutdown()
    {
#ifdef _DEBUG
        if (continueOnCorruption)
        {
            _CrtSetReportHook2(_CRT_RPTHOOK_REMOVE, CrtReportHook);
        }
#endif

        if (vehHandle)
        {
            RemoveVectoredExceptionHandler(vehHandle);
            vehHandle = nullptr;
        }

        if (crashLogFile || (crashLogFile = fopen("crash_log.txt", "a")) != nullptr)
        {
            fprintf(crashLogFile, "\n=== Shutdown ===\n");
            fprintf(crashLogFile, "Breakpoints skipped: %ld\n", breakpointSkipCount);
            fprintf(crashLogFile, "Heap corruptions skipped: %ld\n", heapWarningCount);
            fclose(crashLogFile);
            crashLogFile = nullptr;
        }
    }

    // Exception filter for critical sections (used by PROTECTED_CALL macro)
    static int HandleException(EXCEPTION_POINTERS* exceptionInfo, const char* context)
    {
        DWORD code = exceptionInfo->ExceptionRecord->ExceptionCode;

        // For heap corruption, skip and continue (lightweight log only)
        if (code == 0xC0000374) // STATUS_HEAP_CORRUPTION
        {
            LogExceptionQuiet(exceptionInfo, context);
            printf("WARNING: Heap corruption detected in %s - logged and continuing\n", context);
            return EXCEPTION_EXECUTE_HANDLER; // Execute the __except block
        }

        // For breakpoints from CRT debug heap, execute handler to skip (lightweight log only)
        if (code == STATUS_BREAKPOINT)
        {
            LogExceptionQuiet(exceptionInfo, context);
            printf("WARNING: CRT debug break in %s - skipped\n", context);
            return EXCEPTION_EXECUTE_HANDLER;
        }

        // For access violations and other real crashes, write FULL diagnostic log
        // with stack trace, symbol names, and access violation details (read/write addr)
        LogException(exceptionInfo, context);

        if (code == EXCEPTION_ACCESS_VIOLATION)
        {
            printf("ERROR: Access violation in %s - full stack trace written to crash_log.txt\n", context);
            return EXCEPTION_EXECUTE_HANDLER;
        }

        // For other critical exceptions, continue the search
        return EXCEPTION_CONTINUE_SEARCH;
    }
};

// Macro for protected execution
#define PROTECTED_CALL(code, context) \
    __try { \
        code; \
    } \
    __except(ExceptionHandler::HandleException(GetExceptionInformation(), context)) { \
        printf("Exception handled in " context "\n"); \
    }

#else // Non-Windows platforms

class ExceptionHandler
{
public:
    static void Initialize(bool continueOnHeapCorruption = true) {}
    static void Shutdown() {}
};

#define PROTECTED_CALL(code, context) code

#endif // _WIN32

#endif // EXCEPTION_HANDLER_H
