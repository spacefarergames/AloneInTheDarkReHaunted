///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Native life script system - allows compiled C replacements for PAK bytecode
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "life.h"

// Signature for a native life script function.
// Returns true if the script handled execution (exitLife), false to keep running.
// The function is called once per frame (like processLife) and must handle its own
// state machine logic, similar to the bytecode interpreter.
typedef void (*NativeLifeFunc)(int lifeNum, bool callFoundLife);

// Register a native C function to override a specific life script number.
// When processLife is called with this lifeNum, the native function runs instead of bytecode.
void registerNativeLifeScript(int lifeNum, NativeLifeFunc func);

// Look up whether a native override exists for the given life script number.
// Returns the function pointer if registered, or nullptr if bytecode should be used.
NativeLifeFunc getNativeLifeScript(int lifeNum);

// Returns the total number of registered native life script overrides.
int getNativeLifeScriptCount();

// Initialize all native life script registrations (call during startup).
void initNativeLifeScripts();

// Dump a life script's bytecode to console as readable pseudo-C.
// If lifeNum is -1, dumps all life scripts.
void dumpLifeScript(int lifeNum);

// Dump all life scripts to console.
void dumpAllLifeScripts();

// Generate compilable C code for a single life script using nativeLifeHelpers.h
void generateNativeLifeScriptC(int lifeNum);

// Generate compilable C code for all life scripts to nativeLifeScripts_generated.cpp
void generateAllNativeLifeScripts();

// Validate registered native scripts against a LISTLIFE_dump.txt reference file.
// Any script whose live decompilation differs from the reference is removed from
// the registry so the bytecode interpreter runs instead.
void validateNativeLifeScriptsAgainstDump(const char* dumpFilePath);
