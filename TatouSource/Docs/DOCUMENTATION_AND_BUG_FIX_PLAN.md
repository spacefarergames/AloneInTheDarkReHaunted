# Solution-Wide Documentation & Bug Fix Initiative

## Overview

This document outlines the systematic approach to:
1. Add comprehensive documentation to all source files
2. Identify and fix bugs throughout the codebase
3. Implement coding standards
4. Improve code maintainability

---

## Phase 1: Documentation Standards

### File Header Template

Every `.cpp` and `.h` file should start with:

```cpp
///////////////////////////////////////////////////////////////////////////////
// [Module Name] - [Brief Description]
//
// Purpose:
//   [What this module does in 2-3 sentences]
//
// Key Components:
//   - [Component 1]: [Brief description]
//   - [Component 2]: [Brief description]
//
// Dependencies:
//   - [Dependency 1]
//   - [Dependency 2]
//
// Author: [Name] ([email])
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
///////////////////////////////////////////////////////////////////////////////
```

### Function Documentation Template

Every public function should have a detailed comment:

```cpp
///
/// [Function Name] - [Brief description]
///
/// Purpose:
///   [What the function does]
///
/// Parameters:
///   - param1: [Type and description]
///   - param2: [Type and description]
///   
/// Returns:
///   [Return value and meaning]
///
/// Example:
///   [Code example if complex]
///
/// Notes:
///   [Any special considerations, thread-safety, etc.]
///
int myFunction(int param1, const char* param2)
{
    // Implementation
}
```

### Variable Documentation

```cpp
// Brief description of what this variable stores
// Usage: Where/how it's used
// Thread-safe: Yes/No
// Owned by: Which subsystem
int myVariable;
```

---

## Phase 2: Critical Files needing Documentation

### HIGH PRIORITY

1. **main.cpp** (3000+ lines)
   - [ ] Add function documentation to all public functions
   - [ ] Document global variables
   - [ ] Add section headers for major code blocks

2. **life.cpp** (2000+ lines)
   - [ ] Document all opcodes (38+)
   - [ ] Document bytecode format
   - [ ] Add helper comments for complex logic

3. **object.cpp** (500+ lines)
   - [ ] Document InitObjet() parameters
   - [ ] Explain object structure fields
   - [ ] Document object lifecycle

4. **renderer.cpp** / **rendererBGFX.cpp** (1000+ lines)
   - [ ] Document rendering pipeline
   - [ ] Explain BGFX integration
   - [ ] Document coordinate systems

### MEDIUM PRIORITY

5. **nativeLifeHelpers.h** (1000+ lines)
   - [ ] Document each of 97 helpers
   - [ ] Explain field encodings
   - [ ] Document behavior differences

6. **music.cpp** (500+ lines)
   - [ ] Document audio backend selection
   - [ ] Explain music transitions
   - [ ] Document error handling

7. **hqr.cpp / pak.cpp** (500+ lines each)
   - [ ] Document file formats
   - [ ] Explain resource loading
   - [ ] Document error handling

8. **input.cpp** / **controlsMenu.cpp** (300+ lines each)
   - [ ] Document input processing
   - [ ] Explain control mapping
   - [ ] Document menu system

### LOWER PRIORITY

9. Various utility files (300+ combined)
   - Document helper functions
   - Add error condition handling

---

## Phase 3: Bug Identification & Fixes

### Known Issues (Already Fixed)
- ✅ currentLifePtr stale data during native script execution

### Potential Issues to Investigate

#### Category 1: Null Pointer Dereferences
**Files to check**: fileAccess.cpp, hqr.cpp, pak.cpp
**Pattern to look for**: 
```cpp
sBody* bodyPtr = HQR_Get(...);
// Missing: if (!bodyPtr) { ... }
```

**Status**: Need to audit all HQR_Get() calls

#### Category 2: Resource Leaks
**Files to check**: fileAccess.cpp, music.cpp, renderer.cpp
**Pattern to look for**:
```cpp
FILE* f = fopen(...);
// Process file
// Missing: fclose(f) in error path
```

**Status**: Need to audit error paths in file I/O

#### Category 3: Buffer Overflows
**Files to check**: sprite.cpp, font.cpp, debugFont.cpp
**Pattern to look for**:
```cpp
char buffer[256];
strcpy(buffer, input);  // No bounds checking!
```

**Status**: Need to audit string handling

#### Category 4: Uninitialized Variables
**Pattern to look for**:
```cpp
int value;
if (condition) {
    value = 10;
}
return value;  // Value may be uninitialized!
```

**Status**: Need to audit variable initialization

#### Category 5: Integer Overflows
**Pattern to look for**:
```cpp
int x = actor->worldX + offset;  // May overflow!
```

**Status**: Need to audit coordinate calculations

---

## Phase 4: Coding Standards Document

### Variable Naming
- **Global variables**: `g_variableName` (g_ prefix)
- **Static variables**: `s_variableName` (s_ prefix)
- **Member variables**: `m_variableName` (m_ prefix)
- **Constants**: `CONSTANT_NAME` (UPPERCASE)

### Function Naming
- **Public**: `CamelCase()` (starts with uppercase)
- **Internal**: `camelCase()` (starts with lowercase)
- **Helpers**: `_internalHelper()` (underscore prefix)

### Comments
- Use `///` for API documentation
- Use `//` for inline comments
- Use `/* ... */` for multi-line comments only when needed

### Error Handling
```cpp
// Preferred pattern
if (!resource) {
    consoleLog("Error: Failed to load resource");
    return -1;  // Error code
}

// Not preferred
if (!resource) {
    printf("Error");  // Use consoleLog() instead
}
```

### Memory Management
```cpp
// Use RAII where possible
sBody* body = HQR_Get(...);  // Managed by HQR system

// Explicit cleanup for malloc'd memory
char* data = (char*)malloc(size);
if (data) {
    // Use data
    free(data);
}
```

---

## Phase 5: Implementation Order

### Step 1: Document Core Systems (Week 1)
- [ ] main.cpp - Game loop
- [ ] life.cpp - Script interpreter
- [ ] object.cpp - Object management

### Step 2: Add Function Documentation (Week 2)
- [ ] nativeLifeHelpers.h (97 functions)
- [ ] renderer.cpp APIs
- [ ] music.cpp APIs

### Step 3: Audit & Fix Bugs (Week 3)
- [ ] Null pointer checks
- [ ] Error handling paths
- [ ] Resource cleanup

### Step 4: Code Quality (Week 4)
- [ ] Buffer overflow checks
- [ ] Integer overflow prevention
- [ ] Uninitialized variable detection

### Step 5: Documentation Review (Week 5)
- [ ] Verify all functions documented
- [ ] Create architecture diagrams
- [ ] Update README

---

## Specific Actions Needed

### action-1: Document InitObjet() in object.cpp

Current:
```cpp
int InitObjet(int body, int typeZv, int hardZvIdx, s16 objectType, int x, int y, int z, int stage, int room, int alpha, int beta, int gamma, int anim, int frame, int animtype, int animInfo)
{
    // ...
}
```

Needs:
```cpp
///
/// Initialize a game object/actor and add it to the active list
///
/// Parameters:
///   - body: Body model ID (-1 for invisible object)
///   - typeZv: Z-buffer type for rendering
///   - hardZvIdx: Hardness/collision index
///   - objectType: Type flags (AF_ANIMATED, AF_DRAWABLE, etc.)
///   - x, y, z: Initial world coordinates
///   - stage: Game stage/version
///   - room: Room where object starts
///   - alpha, beta, gamma: 10-bit rotation angles
///   - anim: Initial animation ID (-1 for no animation)
///   - frame: Starting frame number
///   - animtype: Animation mode (ONCE, REPEAT, etc.)
///   - animInfo: Animation-specific parameter
///
/// Returns:
///   - >= 0: Object index in actor list
///   - -1: No free slots (max objects reached)
///
int InitObjet(...)
```

### action-2: Add Magic Number Explanations

Example in main.cpp line 90:
```cpp
// Current - unclear
g_shakeOffsetX = ((float)(rand() % 200) - 100.0f) / 100.0f * currentAmplitude * 2.0f;

// Should be:
// Calculate shake offset: random value from -1.0 to +1.0
// Scale by amplitude (larger amplitude = stronger shake)
// Multiply by 2.0 for screen pixel scaling
// (1.0 amplitude unit ≈ 10 pixel displacement)
g_shakeOffsetX = ((float)(rand() % 200) - 100.0f) / 100.0f * currentAmplitude * 2.0f;
```

### action-3: Add Error Handling

Example in fileAccess.cpp:
```cpp
// Current
FILE* fHandle = fopen(...);
fseek(fHandle, 0, SEEK_END);

// Should be:
FILE* fHandle = fopen(...);
if (!fHandle) {
    consoleLog("Error: Cannot open file %s", name);
    return NULL;
}
fseek(fHandle, 0, SEEK_END);
```

### action-4: Document Global Variables

Add to each subsystem file:
```cpp
// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

// Current actor being processed by life scripts
// Updated by: processLife()
// Used by: All life script helper functions
// Thread-safe: No (main thread only)
extern tObject* currentProcessedActorPtr;
extern int currentProcessedActorIdx;

// Current life script bytecode position
// Updated by: processLife() and bytecode interpreter
// Used by: executeFoundLife() for nested script execution
// Thread-safe: No (main thread only)
// NOTE: Set to nullptr during native script execution to prevent stale reads
extern char* currentLifePtr;
```

---

## Verification Checklist

### For Each File Modified:
- [ ] Added comprehensive header comment
- [ ] Documented all public functions
- [ ] Documented all global variables
- [ ] Explained magic numbers
- [ ] Added error handling comments
- [ ] Marked thread-unsafe code with warnings
- [ ] Code compiles without warnings
- [ ] Verified no functionality changed

### For Each Function Documented:
- [ ] Purpose clearly stated
- [ ] All parameters documented
- [ ] Return value documented
- [ ] Preconditions documented (if any)
- [ ] Side effects documented (if any)
- [ ] Thread-safety documented
- [ ] Example provided (if complex)

---

## Tools & Scripts for Automation

### Find Undocumented Functions
```cpp
// Pattern: public function without /// comment
grep -n "^[a-zA-Z_][a-zA-Z0-9_]*(" *.cpp | grep -v "///"
```

### Find Missing Error Checks
```cpp
// Pattern: HQR_Get() without null check
grep -n "HQR_Get" *.cpp | grep -v "if.*!" | grep -v "assert"
```

### Find Magic Numbers
```cpp
// Pattern: numeric literals in code
grep -E "[^a-zA-Z0-9_](([0-9]+)|([0-9]+\.[0-9]+))[^a-zA-Z0-9_]" *.cpp
```

---

## Success Criteria

✅ **Phase 1 Complete When:**
- All core systems have header documentation
- All public APIs documented with parameters/returns
- All global variables documented with usage

✅ **Phase 2 Complete When:**
- All 97 helper functions documented
- All opcodes documented with behavior
- All data structures documented

✅ **Phase 3 Complete When:**
- No null pointer dereference issues
- Error paths properly handled
- Resource cleanup on all error conditions

✅ **Phase 4 Complete When:**
- No buffer overflows possible
- Integer overflow prevention in place
- All variables properly initialized

---

## Metrics to Track

- **Lines of documentation added**: Target 5000+
- **Functions documented**: Target 100%
- **Bugs fixed**: All identified issues
- **Code coverage by documentation**: Target 80%+
- **Build warnings**: Target 0

---

## Schedule

**Week 1**: Core system documentation (main, life, object)
**Week 2**: Function-level documentation (helpers, APIs)
**Week 3**: Bug fixes (null checks, error handling)
**Week 4**: Code quality improvements (overflows, initialization)
**Week 5**: Review and finalization

---

## Resources Provided

1. ✅ Architecture documentation (ARCHITECTURE_DOCUMENTATION.md)
2. ✅ Native scripts reference (NATIVE_SCRIPTS_QUICK_REFERENCE.md)
3. ✅ Comprehensive audit (COMPREHENSIVE_SOLUTION_AUDIT.md)
4. ✅ This document (DOCUMENTATION_AND_BUG_FIX_PLAN.md)

