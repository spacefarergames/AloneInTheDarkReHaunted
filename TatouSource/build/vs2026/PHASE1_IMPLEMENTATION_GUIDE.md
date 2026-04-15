# Phase 1 Implementation Guide

## Overview
This guide provides step-by-step instructions for adding the comprehensive documentation created in PHASE1_CORE_DOCUMENTATION.md to the actual source files.

## File-by-File Implementation

### 1. main.cpp - Documentation Updates

**File Location**: `D:\FITD\FitdLib\main.cpp`

#### Step 1: Update Main File Header (Lines 1-8)

**Current header** (lines 1-8):
```
///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Core game engine - initialization, rendering, collision, camera, and game state
///////////////////////////////////////////////////////////////////////////////
```

**Replace with** (see PHASE1_CORE_DOCUMENTATION.md - main.cpp header section)

**What to do**:
1. Open main.cpp in Visual Studio
2. Select lines 1-8 (the header)
3. Delete and replace with the comprehensive header from PHASE1_CORE_DOCUMENTATION.md
4. This adds 30+ lines of detailed module documentation

---

#### Step 2: Document Shake State Variables (After line 72)

**Location**: Before the shake state static variables (around line 74-76)

**Find**:
```cpp
int AntiRebond;

// Shake state - only triggers via life script LM_SHAKING opcode
static int s_shakeFramesRemaining = 0;
static int s_shakeMaxAmplitude = 0;
```

**Replace comment with** (from PHASE1_CORE_DOCUMENTATION.md):
```cpp
int AntiRebond;

///
/// Shake state variables - control camera shake effects
///
/// s_shakeFramesRemaining: How many frames of shake effect remain
/// s_shakeMaxAmplitude: Maximum shake intensity (amplitude) for current effect
/// Usage: Updated by setupShaking(), decremented each frame by updateShaking()
/// Thread-safe: No (main thread only)
///
// Shake state - only triggers via life script LM_SHAKING opcode
static int s_shakeFramesRemaining = 0;
static int s_shakeMaxAmplitude = 0;
```

---

#### Step 3: Document updateShaking() Function (Before line 78)

**Location**: Before function `void updateShaking()` at line 78

**Add documentation block** (from PHASE1_CORE_DOCUMENTATION.md):
```cpp
///
/// Update camera shake effect for current frame
///
/// Purpose:
///   Called once per frame to update the camera shake effect state.
///   If shake is active, generates random offsets and decrements duration.
///   Automatically stops shaking when duration expires.
/// [rest of documentation...]
///
```

**Note**: The function already has good inline comments. Just add the function header documentation above the function definition.

---

#### Step 4: Document setupShaking() Function (Before line 119)

**Location**: Before function `void setupShaking(int amplitude)` at line 119

**Add documentation block** (from PHASE1_CORE_DOCUMENTATION.md):
```cpp
///
/// Initialize camera shake effect with given amplitude
///
/// Purpose:
///   Called from LM_SHAKING life script opcode to start camera shake effect.
/// [rest of documentation...]
///
```

---

#### Step 5: Document stopShaking() Function (Before line 109)

**Location**: Before function `void stopShaking()` at line 109

**Add documentation block** (from PHASE1_CORE_DOCUMENTATION.md):
```cpp
///
/// Immediately stop camera shake effect
/// [rest of documentation...]
///
```

---

#### Step 6: Document pauseShaking() Function (Before line 132)

**Location**: Before function `void pauseShaking()` at line 132

**Add documentation block** (from PHASE1_CORE_DOCUMENTATION.md):
```cpp
///
/// Temporarily zero out shake offsets without clearing state
/// [rest of documentation...]
///
```

---

#### Step 7: Document getCVarsIdx() Functions (Before line 140 and 158)

**Locations**:
- `int getCVarsIdx(enumCVars searchedType)` at line 140
- `int getCVarsIdx(int searchedType)` at line 158

**Add documentation** (from PHASE1_CORE_DOCUMENTATION.md):
```cpp
///
/// Find index of configuration variable in CVars array
/// [rest of documentation...]
///
```

**Note**: Both versions should have the documentation above the first one (enumCVars version).

---

#### Step 8: Document executeFoundLife() Function (Before line 267)

**Location**: Before function `void executeFoundLife(int objIdx)` at line 267

**Add documentation block** (from PHASE1_CORE_DOCUMENTATION.md):
```cpp
///
/// Execute a life script found on an object (nested execution)
/// [complete documentation...]
///
```

**Why this is important**: This is a complex function managing nested script execution state. The documentation explains:
- What it does (nested script execution)
- All parameters saved and restored
- Global state modifications
- Why it matters (LM_FOUND opcode support)

---

### 2. life.cpp - Documentation Updates

**File Location**: `D:\FITD\FitdLib\life.cpp`

#### Step 1: Update Main File Header (Lines 1-8)

**Current header**:
```
///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Life script interpreter and AI behavior engine
///////////////////////////////////////////////////////////////////////////////
```

**Replace with** (see PHASE1_CORE_DOCUMENTATION.md - life.cpp header section)

**What to do**:
1. Open life.cpp in Visual Studio
2. Select lines 1-8 (the header)
3. Replace with comprehensive header from PHASE1_CORE_DOCUMENTATION.md
4. This adds 40+ lines with opcode categories, bytecode format, dependencies

---

#### Step 2: Document specialTable Variable (Around line 25)

**Find** (line 24-25):
```cpp
int groundLevel;
s16 specialTable[4] = { 144, 192, 48, 112 };
```

**Replace with** (add documentation block above specialTable):
```cpp
int groundLevel;

///
/// Special values table for AITD1-specific behavior
///
/// specialTable[0] = 144: Special threshold value for mode 0
/// specialTable[1] = 192: Special threshold value for mode 1
/// specialTable[2] = 48:  Special threshold value for mode 2
/// specialTable[3] = 112: Special threshold value for mode 3
///
/// Used in AITD1-specific opcode handlers for backward compatibility
/// with original game behavior. Not used in AITD2/AITD3.
///
s16 specialTable[4] = { 144, 192, 48, 112 };
```

---

### 3. object.cpp - Documentation Updates

**File Location**: `D:\FITD\FitdLib\object.cpp`

#### Step 1: Update Main File Header (Lines 1-8)

**Current header**:
```
///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Game object initialization and body/animation setup
///////////////////////////////////////////////////////////////////////////////
```

**Replace with** (see PHASE1_CORE_DOCUMENTATION.md - object.cpp header section)

**What to do**:
1. Open object.cpp in Visual Studio
2. Select lines 1-8 (the header)
3. Replace with comprehensive header from PHASE1_CORE_DOCUMENTATION.md
4. This adds 30+ lines documenting object lifecycle, integration points

---

#### Step 2: Document InitObjet() Function (Before line 13)

**Location**: Before function `int InitObjet(int body, int typeZv, ...)`

**Add documentation block** (from PHASE1_CORE_DOCUMENTATION.md):

This is the most critical documentation. The InitObjet() function has **13 parameters** that need explanation:

```cpp
///
/// Initialize a game object/actor and add it to the active object list
///
/// Purpose:
///   Creates a new game object with specified properties and adds it to the
///   global active object list (ListObjets). Sets up all initial fields
///   including position, rotation, animation, collision, and physical properties.
///   Returns immediately; object will be processed each frame by main loop.
///
/// Parameters:
///   - body: Body model ID from HQR (-1 = invisible/no visual)
///   - typeZv: Z-buffer type for rendering priority (HZ_SPRITE, HZ_POLY, etc.)
///   - hardZvIdx: Collision/hardness volume index (links to collision data)
///   - objectType: Type flags (AF_ANIMATED, AF_DRAWABLE, AF_3D, etc.)
///   - x, y, z: Initial world coordinates (in room coordinate space)
///   - stage: Game stage/version (STAGE_AITD1, STAGE_AITD2, etc.)
///   - room: Starting room index
///   - alpha: 10-bit rotation around Y axis (horizontal rotation, 0-1024)
///   - beta: 10-bit rotation around X axis (vertical rotation, 0-1024)
///   - gamma: 10-bit rotation around Z axis (roll, 0-1024)
///   - anim: Animation ID from HQR (-1 = no animation)
///   - frame: Starting frame within animation (0-based)
///   - animtype: Animation mode (ANIM_ONCE, ANIM_REPEAT, ANIM_ALL_ONCE, etc.)
///   - animInfo: Animation-specific parameter (varies by animtype)
///
/// Returns:
///   - >= 0: Object index in ListObjets array (success)
///   - -1: No free slots (LIST_MAX_OBJECT limit reached)
///
/// [rest of documentation from PHASE1_CORE_DOCUMENTATION.md...]
///
int InitObjet(int body, int typeZv, int hardZvIdx, s16 objectType, ...
```

---

## Implementation Checklist

### main.cpp
- [ ] Update main file header (lines 1-8)
- [ ] Add shake state variables documentation (line 74)
- [ ] Add updateShaking() documentation (line 78)
- [ ] Add setupShaking() documentation (line 119)
- [ ] Add stopShaking() documentation (line 109)
- [ ] Add pauseShaking() documentation (line 132)
- [ ] Add getCVarsIdx() documentation (lines 140, 158)
- [ ] Add executeFoundLife() documentation (line 267)
- **Total lines added**: ~80 lines of documentation

### life.cpp
- [ ] Update main file header (lines 1-8)
- [ ] Add specialTable variable documentation (line 25)
- **Total lines added**: ~50 lines of documentation

### object.cpp
- [ ] Update main file header (lines 1-8)
- [ ] Add InitObjet() documentation (line 13)
- **Total lines added**: ~60 lines of documentation

### Verification
- [ ] All files compile without warnings
- [ ] No functionality changes (only documentation added)
- [ ] All functions documented with parameters, returns, notes
- [ ] Build succeeds with 0 errors

---

## Manual Implementation Instructions

Since the automated replace tool experienced issues with file encoding, here's a manual process:

### For Each File (main.cpp, life.cpp, object.cpp):

1. **Open file in Visual Studio**
2. **Select lines to replace** (use Edit > Find & Replace if needed)
3. **Copy documentation from PHASE1_CORE_DOCUMENTATION.md**
4. **Paste into file** at the specified location
5. **Verify no syntax errors** (red squiggles in editor)
6. **Build solution** to confirm no regressions

### Using Visual Studio Search & Replace:
1. Open File > Replace (Ctrl+H)
2. Find: The old comment/function header
3. Replace: The documented version from PHASE1_CORE_DOCUMENTATION.md
4. Click "Replace All" or "Replace" (one at a time for safety)

---

## Key Documentation Benefits

After completing Phase 1:

✅ **main.cpp**:
- Clear explanation of game loop orchestration
- Shake system well documented (3 related functions)
- executeFoundLife() clarified (complex state management)
- All 7 key functions have purpose + parameters + notes

✅ **life.cpp**:
- Bytecode interpreter clearly explained
- Opcode categories documented
- AITD1 special values explained
- 38+ opcodes organized into categories

✅ **object.cpp**:
- Object initialization clearly explained
- 13 InitObjet() parameters fully documented
- Coordinate space conversion explained
- Animation and collision integration documented

---

## Next Steps After Phase 1

Once documentation is added and verified:

1. **Run build verification** to ensure no regressions
2. **Commit documentation changes** to git
3. **Create pull request** with documentation improvements
4. **Progress to Phase 2** (Week 2): Function-level documentation for:
   - All 97 native life helpers (nativeLifeHelpers.h)
   - Renderer APIs (renderer.cpp)
   - Music APIs (music.cpp)

---

## Additional Resources

- **PHASE1_CORE_DOCUMENTATION.md**: Complete text of all documentation
- **DOCUMENTATION_AND_BUG_FIX_PLAN.md**: Overall 5-week plan
- **ARCHITECTURE_DOCUMENTATION.md**: System-wide architecture
- **COMPREHENSIVE_SOLUTION_AUDIT.md**: Code quality assessment

---

**Estimated Time**: 30-45 minutes to add all Phase 1 documentation
**Difficulty Level**: Easy (copy-paste documentation, no code changes)
**Risk Level**: Very Low (only adding comments, no logic changes)
