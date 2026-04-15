///////////////////////////////////////////////////////////////////////////////
// Bytecode Patches - Runtime bug fixes for life script bytecode
//
// Purpose:
//   Provides a system to patch known bugs in life script bytecode execution
//   without modifying the original bytecode or using native script overrides.
//   Patches intercept execution at key points and can modify actor state,
//   skip instructions, or inject corrected behavior.
//
// Design:
//   - Patches are applied during bytecode interpretation in processLife()
//   - Can hook before/after specific opcodes for specific life scripts
//   - Can patch global state issues (e.g., stale pointers)
//   - Used for reference: nativeLifeHelpers.h documents the original APIs
//
// Key Components:
//   - Patch registry: stores all registered patches
//   - Patch condition: determines if patch should apply
//   - Patch action: executes the fix
//   - Opcode hooks: intercept bytecode before/after execution
//
// Dependencies:
//   - life.h (opcode definitions, processLife context)
//   - common.h (basic types)
//
// Author: Patch System
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "life.h"

///
/// Patch Condition Function
///
/// Determines whether a patch should be applied based on current game state.
///
/// Parameters:
///   - lifeNum: Current life script number
///   - opcode: Current bytecode opcode (if opcode-specific)
///   - actor: Current actor being processed
///
/// Returns:
///   - true: Apply the patch
///   - false: Skip the patch
///
/// Notes:
///   - Called frequently during bytecode execution
///   - Must be fast and not modify state
///   - Return false if patch doesn't apply
///
typedef bool (*PatchConditionFunc)(int lifeNum, int opcode, tObject* actor);

///
/// Patch Action Function
///
/// Executes the bug fix for a detected issue.
///
/// Parameters:
///   - lifeNum: Current life script number
///   - opcode: Current bytecode opcode (if opcode-specific)
///   - actor: Current actor being processed
///   - context: Additional context (e.g., which point in execution: before/after opcode)
///
/// Notes:
///   - May modify actor state, global variables, etc.
///   - Should be idempotent (safe to call multiple times)
///   - Must not corrupt bytecode interpreter state
///   - Can read from but should not modify currentLifePtr directly
///
typedef void (*PatchActionFunc)(int lifeNum, int opcode, tObject* actor, int context);

///
/// Register a patch that intercepts opcode execution
///
/// Parameters:
///   - lifeNum: Life script number (-1 for all scripts)
///   - opcode: Life macro opcode (or -1 for all opcodes)
///   - condition: Function that determines if patch applies
///   - action: Function that executes the fix
///   - description: Human-readable patch description for logging
///
/// Notes:
///   - If lifeNum is -1, patch applies to all life scripts with matching opcode
///   - If opcode is -1, patch applies to all opcodes in matching life script
///   - Multiple patches can apply to same lifeNum/opcode pair
///
void registerBytecodePatch(int lifeNum, int opcode, 
                          PatchConditionFunc condition, 
                          PatchActionFunc action,
                          const char* description);

///
/// Check if patches should be applied before opcode execution
///
/// Called from processLife() before executing each bytecode instruction.
/// Applies all matching pre-execution patches.
///
/// Parameters:
///   - lifeNum: Current life script number
///   - opcode: About to be executed opcode
///   - actor: Current actor
///
void applyBytecodePreOpcodePatches(int lifeNum, int opcode, tObject* actor);

///
/// Check if patches should be applied after opcode execution
///
/// Called from processLife() after executing each bytecode instruction.
/// Applies all matching post-execution patches.
///
/// Parameters:
///   - lifeNum: Current life script number
///   - opcode: Just executed opcode
///   - actor: Current actor
///
void applyBytecodePostOpcodePatches(int lifeNum, int opcode, tObject* actor);

///
/// Check if patches should be applied at lifecycle points
///
/// Called at specific game events (e.g., actor init, script start, etc.)
///
/// Parameters:
///   - event: Lifecycle event type
///   - lifeNum: Current life script number (-1 if not script-specific)
///   - actor: Actor involved (nullptr for non-actor events)
///
enum LifecycleEventType
{
    LIFECYCLE_ACTOR_INIT,     // Actor created
    LIFECYCLE_ACTOR_DELETE,   // Actor deleted
    LIFECYCLE_SCRIPT_START,   // Script execution begins
    LIFECYCLE_SCRIPT_END,     // Script execution ends
    LIFECYCLE_GAME_INIT,      // Game initialized
};

void applyBytecodeLifecyclePatches(LifecycleEventType event, int lifeNum, tObject* actor);

///
/// Initialize the bytecode patch system
///
/// Call during startup after life scripts are loaded.
/// Registers all known patches for identified bugs.
///
void initBytecodePatches();

///
/// Dump registered patches to console
///
void dumpBytecodePatches();

///
/// Get total number of registered patches
///
int getBytecodePatches();
