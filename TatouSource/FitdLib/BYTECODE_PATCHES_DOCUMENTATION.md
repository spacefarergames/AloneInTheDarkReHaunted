///////////////////////////////////////////////////////////////////////////////
// BYTECODE PATCH SYSTEM DOCUMENTATION
// Runtime Bug Fixes for Life Script Bytecode Interpreter
//
// Author: Patch System
// Created: 2026-04-15
// Status: ACTIVE - Native scripts DISABLED, using patches instead
///////////////////////////////////////////////////////////////////////////////

# OVERVIEW

The bytecode patch system replaces native life scripts with a cleaner, more
maintainable approach to fixing bugs in the life script bytecode interpreter.

**Key Philosophy:**
- Native C script overrides are complex and hard to maintain
- A patch system runs on top of the bytecode interpreter
- Patches are registered with conditions and actions
- All script logic stays in one place (the bytecode)
- Patches serve as a reference for the original native script APIs

**Status:** Native scripts are DISABLED. Only bytecode interpreter + patches run.

---

# ARCHITECTURE

## Design Pattern

```
processLife(lifeNum)
  ↓
  Load bytecode script
  ↓
  Main loop:
    - Read next opcode
    - [PATCH POINT: Pre-opcode patches]
    - Execute opcode (switch statement)
    - [PATCH POINT: Post-opcode patches]
  ↓
  Script complete
```

## Patch Registry

- Global `s_bytecodePatches` vector stores all registered patches
- Each patch entry:
  - `lifeNum`: Specific life script (-1 = all scripts)
  - `opcode`: Specific opcode (-1 = all opcodes)
  - `condition()`: Function determining if patch applies
  - `action()`: Function executing the fix
  - `description`: Human-readable patch name

## Patch Execution Flow

1. **Pre-Opcode Patches** (called BEFORE opcode switch statement):
   - Check all registered patches
   - Match by lifeNum and opcode
   - Call condition() to verify applicability
   - Call action(context=0) if condition met

2. **Opcode Execution** (main switch statement):
   - Execute the actual bytecode operation
   - May modify actor state, resources, etc.

3. **Post-Opcode Patches** (called AFTER opcode switch statement):
   - Check all registered patches
   - Same matching logic as pre-opcode
   - Call action(context=1) if condition met
   - Can verify state changes from opcode

---

# API REFERENCE

## Core Functions

### registerBytecodePatch()
```cpp
void registerBytecodePatch(int lifeNum, int opcode, 
                          PatchConditionFunc condition, 
                          PatchActionFunc action,
                          const char* description);
```

**Parameters:**
- `lifeNum`: Life script number or -1 for all scripts
- `opcode`: Life macro opcode or -1 for all opcodes
- `condition`: Function returning true if patch should apply
- `action`: Function executing the fix
- `description`: Logged patch description

**Example:**
```cpp
registerBytecodePatch(-1, LM_ANIM_ONCE,
                     patchCondition_AnimationValidation,
                     patchAction_AnimationValidation,
                     "Validate animation state after changes");
```

### Patch Condition Function
```cpp
typedef bool (*PatchConditionFunc)(int lifeNum, int opcode, tObject* actor);
```

**Returns:**
- `true`: Apply the patch
- `false`: Skip the patch

**Performance Note:**
- Called frequently during bytecode execution
- Must be fast (no expensive operations)
- Should return false quickly if patch doesn't apply

### Patch Action Function
```cpp
typedef void (*PatchActionFunc)(int lifeNum, int opcode, tObject* actor, int context);
```

**Parameters:**
- `lifeNum`: Current life script number
- `opcode`: Current bytecode opcode
- `actor`: Current actor being processed
- `context`: 0 for pre-opcode, 1 for post-opcode

**Design Notes:**
- May modify actor state (ANIM, bodyNum, etc.)
- Should be idempotent (safe to call multiple times)
- Must not corrupt bytecode interpreter state
- Should not modify currentLifePtr directly
- Can log warnings/errors using consoleLog()

---

# IMPLEMENTED PATCHES

## 1. Actor Pointer Validation

**Issue:**
- Some opcodes receive invalid actor pointers
- Can cause crashes or undefined behavior

**Solution:**
- Before certain opcodes (animations, movement), validate actor is valid
- Check pointer is non-null and indexInWorld is in valid range
- Log warning if invalid

**Applied To Opcodes:**
- LM_ANIM_ONCE, LM_ANIM_REPEAT, LM_ANIM_ALL_ONCE
- LM_DO_MOVE, LM_MOVE

## 2. Resource Pointer Checking

**Issue:**
- HQR_Get() can return nullptr if resource doesn't exist
- Some opcodes don't check return value before dereferencing

**Solution:**
- After resource-loading opcodes, verify resources exist
- Log warning if body, animation, or other resource missing
- Prevent use of invalid pointers

**Applied To Opcodes:**
- LM_BODY, LM_ANIM_ONCE, LM_ANIM_REPEAT, LM_ANIM_ALL_ONCE
- LM_SAMPLE, LM_MUSIC, LM_LIGHT

## 3. Animation State Validation

**Issue:**
- Animation opcodes may set invalid animation IDs
- Can cause rendering glitches or crashes
- Frame numbers may be out of bounds

**Solution:**
- After animation change opcodes, validate animation ID exists
- Reset to -1 (no animation) if invalid
- Log warning about invalid animation

**Applied To Opcodes:**
- LM_ANIM_ONCE, LM_ANIM_REPEAT, LM_ANIM_ALL_ONCE

---

# ADDING NEW PATCHES

## Step 1: Create Condition Function

```cpp
static bool patchCondition_MyPatch(int lifeNum, int opcode, tObject* actor)
{
    // Only apply to specific opcodes
    if (opcode != LM_MY_OPCODE)
        return false;
    
    // Check if this life script is affected
    if (lifeNum != 10 && lifeNum != 15)  // specific scripts
        return false;
    
    // Check if actor state triggers the bug
    if (actor == nullptr)
        return false;
    
    return true;  // Condition met, apply patch
}
```

## Step 2: Create Action Function

```cpp
static void patchAction_MyPatch(int lifeNum, int opcode, tObject* actor, int context)
{
    // Pre-opcode context (context == 0)
    if (context == 0)
    {
        // Validate state before opcode executes
        // Example: check parameter ranges
    }
    
    // Post-opcode context (context == 1)
    if (context == 1)
    {
        // Verify opcode effects
        // Example: validate result state is correct
        if (actor->ANIM < -1 || actor->ANIM >= MAX_ANIMS)
        {
            consoleLog(LIFE_WARN "Bytecode patch: Invalid state in life %d" CON_RESET, lifeNum);
            actor->ANIM = -1;  // Correct the invalid state
        }
    }
}
```

## Step 3: Register in initBytecodePatches()

```cpp
void initBytecodePatches()
{
    // ... existing patches ...
    
    registerBytecodePatch(10,  // lifeNum (or -1 for all)
                         LM_MY_OPCODE,  // opcode
                         patchCondition_MyPatch,
                         patchAction_MyPatch,
                         "Brief description of the bug and fix");
}
```

---

# MIGRATION FROM NATIVE SCRIPTS

## Files Kept for Reference

- **nativeLife.h / nativeLife.cpp**: Contains original native script framework
  - Code is preserved for reference
  - Functions are no longer called
  - Used to understand original intent of scripts

- **nativeLifeHelpers.h**: Documents 97+ native helper functions
  - Each helper is documented with parameters
  - Shows what operations scripts could perform
  - Reference for understanding bytecode semantics

## Known Native Scripts (Not Used)

The following life scripts had native overrides (no longer called):
- AITD1-specific life scripts (various actor behaviors)
- Complex AI routines
- Puzzle-specific logic

## Advantages of Patch System

| Aspect | Native Scripts | Bytecode Patches |
|--------|---|---|
| Code organization | Scattered C code | Centralized patch registry |
| Maintenance | Hard to correlate bugs | Clear condition + action |
| Debugging | Complex state machines | Simple before/after hooks |
| Performance | Full function overhead | Minimal overhead checks |
| Extensibility | Hard to add new overrides | Easy to register new patches |
| Version control | Large binary diffs | Small text changes |

---

# DEBUGGING PATCHES

## Console Output

When `initBytecodePatches()` runs:
```
=== Initializing Bytecode Patch System ===
Native life scripts: DISABLED
Using bytecode interpreter with runtime patches
Registered bytecode patch (all scripts): Validate actor pointer validity
Registered bytecode patch (all scripts): Check for null resource pointers
Registered bytecode patch (all scripts): Validate animation state after changes
Bytecode patch system initialized: 3 patches registered
```

## Dumping Patch Status

Call `dumpBytecodePatches()` to see:
- All registered patches
- How many times each has been executed
- Detailed condition matching info

## Per-Patch Logging

Patches log warnings when issues are detected:
```
[LIFE] Bytecode patch: Invalid actor in life 5, opcode 1
[LIFE] Bytecode patch: Missing body 99 in life 10, actor 3
[LIFE] Bytecode patch: Invalid animation 500 in life 15, actor 7
```

---

# PERFORMANCE CONSIDERATIONS

## Overhead

- **Per-opcode cost:** 2 loop iterations over patch registry
  - Pre-opcode loop: ~O(n) where n = number of patches
  - Post-opcode loop: ~O(n)
- **Typical:** With 3 patches, ~6 condition() calls per opcode
- **Mitigation:** Patches short-circuit on first mismatch

## Optimization Techniques

1. **Condition Fast-Fail:**
```cpp
// Bad: Multiple checks
if (opcode == LM_BODY && lifeNum == 5 && actor != nullptr)

// Better: Return early
if (opcode != LM_BODY) return false;
if (lifeNum != 5) return false;
if (!actor) return false;
return true;
```

2. **Cache Validity Checks:**
```cpp
// Don't call expensive checks every frame
// Only validate when state actually changes
```

3. **Selective Patching:**
```cpp
// Don't apply global patches to unaffected opcodes
if (opcode == -1)  // Apply to all opcodes
   // Condition must be very cheap!
```

---

# KNOWN LIMITATIONS

1. **State Outside Bytecode:**
   - Patches can't intercept native helper calls
   - Can only patch opcode execution
   - Global state changes must be detected after-the-fact

2. **Bytecode Modification:**
   - Patches can't rewrite bytecode instructions
   - Can only validate/correct resulting state
   - For true script fixes, bytecode must be regenerated

3. **Conditional Patches:**
   - Can't skip opcode execution from patch
   - Only pre- and post-execution hooks
   - For skipping opcodes, would need bytecode modification

---

# FUTURE ENHANCEMENTS

1. **Opcode Skip Support:**
   - Add ability for patch to signal "skip this opcode"
   - Requires bytecode interpreter changes

2. **Lifecycle Patches:**
   - LIFECYCLE_ACTOR_INIT: Apply when actor created
   - LIFECYCLE_ACTOR_DELETE: Apply when actor removed
   - LIFECYCLE_SCRIPT_START: Apply at script start
   - (Framework already in place, just not implemented)

3. **Patch Priorities:**
   - Allow patches to specify execution order
   - Prevents patch conflicts

4. **Bytecode Substitution:**
   - Replace entire script with patched bytecode
   - Better than multiple small patches

---

# RELATED FILES

- `bytecodePatches.h`: API definitions
- `bytecodePatches.cpp`: Implementations
- `life.cpp`: Bytecode interpreter (lines 1040-2940)
- `nativeLife.h`: Reference for original APIs
- `nativeLifeHelpers.h`: Reference for helper functions
- `DOCUMENTATION_AND_BUG_FIX_PLAN.md`: High-level bug planning

---

# CONTACT & REFERENCES

For understanding the bytecode opcodes, see:
- NATIVE_SCRIPTS_QUICK_REFERENCE.md: Opcode documentation
- nativeLifeHelpers.h: Helper function documentation
- ARCHITECTURE_DOCUMENTATION.md: Overall system design

For game-specific life script numbers:
- AITD1 scripts: See AITD1_life.cpp references
- AITD2 scripts: Similar pattern in respective game code

