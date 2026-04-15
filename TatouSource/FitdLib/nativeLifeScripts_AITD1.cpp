///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// AITD1 native life script registrations.
// Includes auto-generated native scripts from LISTLIFE bytecode.
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "nativeLife.h"
#include "consoleLog.h"

// Forward declaration for generated scripts
extern void registerGeneratedNativeLifeScripts();

void registerAITD1NativeLifeScripts()
{
    // Register all auto-generated native life scripts
    registerGeneratedNativeLifeScripts();

    // Validate each registered script against the reference dump.
    // Scripts whose bytecode has changed since the native was generated are
    // removed so the bytecode interpreter runs instead of stale native code.
    validateNativeLifeScriptsAgainstDump("LISTLIFE_dump.txt");

    printf(CON_PINK "Native Life scripts loaded: %d - native code execution and bug fixes applied \xE2\x9D\xA4\xEF\xB8\x8F" CON_RESET "\n",
        getNativeLifeScriptCount());
}
