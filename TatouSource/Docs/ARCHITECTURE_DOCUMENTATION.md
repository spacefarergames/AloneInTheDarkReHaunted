# FITD Architecture & Component Documentation

## Complete System Map

### 1. **Game Loop & Initialization**

**Files**: `main.cpp`, `mainLoop.cpp`, `gameTime.cpp`

**Responsibilities**:
- Window creation and SDL initialization
- Game state machine management
- Frame timing and synchronization
- Main update/render loop coordination
- Global state initialization

**Key Entry Point**: `main()`
- Initializes all subsystems
- Enters main loop
- Handles shutdown

**Frame Cycle** (mainLoop.cpp):
1. Input processing
2. Game logic update (life scripts, AI, physics)
3. State transitions
4. Rendering

**Data Structures**:
```cpp
// Shake effect state (in main.cpp)
static int s_shakeFramesRemaining;  // How many frames to shake
static int s_shakeMaxAmplitude;     // Maximum shake intensity
float g_shakeOffsetX/Y;             // Current frame's shake offset
```

---

### 2. **Life Script System** (Comprehensive)

**Files**: `life.cpp`, `nativeLife.cpp`, `nativeLife.h`, `nativeLifeHelpers.h`, `nativeLifeHelpersImpl.cpp`, `nativeLifeScripts_generated.cpp`, `nativeLifeScripts_AITD1.cpp`

**Architecture**:
```
LISTLIFE.PAK (bytecode scripts, 562 scripts)
    ↓
Two Execution Paths:
├─ Path 1: Native C (Fast)
│  └─ nativeLifeScripts_generated.cpp calls 97 helper functions
│     └─ nativeLifeHelpers.h/cpp (optimized C code)
│
└─ Path 2: Bytecode Interpreter (Fallback)
   └─ life.cpp (virtual machine)
      ├─ Reads LISTLIFE bytecode
      ├─ Executes 38+ opcodes (LM_* macros)
      └─ Calls same 97 helper functions
```

**Global State Management** (CRITICAL - FIXED):
```cpp
// Current actor being processed
extern tObject* currentProcessedActorPtr;  // Pointer to actor struct
extern int currentProcessedActorIdx;       // Index in actor list

// Life script context
extern char* currentLifePtr;               // Current bytecode position
extern int currentLifeNum;                 // Current life script number
extern int currentLifeActorIdx;            // Which actor script is for
extern tObject* currentLifeActorPtr;       // Pointer to that actor

// Fixed in processLife():
//   - currentLifePtr set to nullptr before native execution
//   - Prevents stale pointer reads in nested executeFoundLife()
//   - Restored after native function returns
```

**Bytecode Opcodes** (38+ in life.cpp):
- `LM_DO_MOVE`: Move along track
- `LM_ANIM_*`: Animation control (ONCE, REPEAT, ALL_ONCE)
- `LM_BODY`: Change body model
- `LM_IF_*`: Conditional branches (6 types)
- `LM_GOTO/RETURN`: Control flow
- `LM_HIT/FIRE/MESSAGE`: Gameplay events
- `LM_VAR`: Variable assignment
- `LM_SHAKING`: Camera shake effect
- ... (32 more)

**Animation Constants**:
```cpp
ANIM_ONCE = 0           // Play once, stop
ANIM_REPEAT = 1         // Loop animation
ANIM_UNINTERRUPTABLE = 2 // Can't be interrupted
ANIM_RESET = 4          // Reset animation
```

**Helper Functions** (97 total in nativeLifeHelpers.h):
- **Field Access**: `life_GetBODY()`, `life_GetCOL()`, `life_GetANIM()`, etc. (39 functions)
- **Animation**: `life_AnimOnce()`, `life_AnimRepeat()`, etc. (5 functions)
- **Movement**: `life_Move()`, `life_DoMove()` (3 functions)
- **Object Operations**: `life_Found()`, `life_ObjGetField()`, `life_WorldObj_*()` (13 functions)
- **Combat**: `life_HIT()`, `life_FIRE()` (5 functions)
- **Audio**: `life_Sample()`, `life_Music()` (4 functions)
- ... and more

---

### 3. **Object & Actor Management**

**Files**: `object.cpp`, `actorList.cpp`, `track.cpp`, `aitdBox.cpp`

**Object Initialization** (object.cpp):
```cpp
int InitObjet(int body, int typeZv, int hardZvIdx, 
              s16 objectType, int x, int y, int z, 
              int stage, int room, int alpha, int beta, int gamma, 
              int anim, int frame, int animtype, int animInfo)
```

**Object Structure** (tObject in common.h):
```cpp
struct tObject {
    int indexInWorld;        // -1 if not used
    int bodyNum;             // Body model ID
    int objectType;          // Type flags (AF_ANIMATED, AF_DRAWABLE, etc.)
    int stage, room;         // Location
    int worldX/Y/Z;          // World coordinates
    int roomX/Y/Z;           // Room-relative coordinates
    int alpha, beta, gamma;  // Rotation angles (10-bit)
    
    // Animation state
    int ANIM;                // Current animation ID
    int frame;               // Current frame
    int animType;            // Animation mode (ONCE, REPEAT, etc.)
    int numOfFrames;         // Total frames in animation
    int flagEndAnim;         // End of animation flag
    
    // Collision
    int COL[3];              // Collision results
    int COL_BY;              // Who collided with us
    int HARD_DEC;            // Hardness decrease
    int HARD_COL;            // Hardness collision
    
    // Hit/Damage
    int HIT;                 // Hit target
    int HIT_BY;              // Hit by actor
    
    // ... (40+ more fields)
};
```

**Actor Lists**:
```cpp
extern MYLIST<tObject> ListObjets;      // All game objects/actors
extern int NUM_MAX_OBJECT;              // Maximum objects (typically 500)
```

**Actor Iteration Pattern**:
```cpp
for (int i = 0; i < NUM_MAX_OBJECT; i++) {
    if (ListObjets[i].indexInWorld != -1) {
        // Object is active
        processActor(&ListObjets[i]);
    }
}
```

---

### 4. **Animation System**

**Files**: `anim.cpp`, `anim2d.cpp`, `animAction.cpp`

**Animation Structures**:
```cpp
struct sAnimation {
    int nbFrames;           // Number of frames
    sFrame* frames;         // Array of frame data
    // ... more fields
};

struct sFrame {
    s16 dummy;
    s16 vertexCount;
    // ... vertex data
};
```

**Animation Playback**:
- Frame selection based on animation ID and frame number
- Interpolation for smooth transitions
- Animation-specific actions (hit detection, sound cues)

---

### 5. **Variable System**

**Files**: `vars.cpp`, `vars.h`, `lifeMacroTable.cpp`

**Game Variables**:
```cpp
// 256+ game variables for state tracking
extern int vars[256];      // Global game variables
```

**Variable Encoding**:
- Variables can be accessed by index or symbolic name
- Persisted in save games
- Used throughout scripts for state tracking

**Field IDs** (0x00-0x26 in evalVar.cpp):
```cpp
0x00 = COL           // Collision result
0x01 = HARD_DEC      // Hardness decrease
0x02 = HARD_COL      // Hardness collision value
0x05 = ANIM          // Animation ID
0x06 = END_ANIM      // End animation flag
0x09 = BODY          // Body model
0x16 = Special field // Context-specific
// ... 20+ more
```

---

### 6. **Audio/Music System**

**Files**: `music.cpp`, `osystemAL.cpp`, `osystemAL_*.cpp`, `osystemSDL.cpp`

**Architecture**:
```
Audio Backend Selection:
├─ AdLib (FM synthesis) - AITD1
├─ MP3 (compressed audio) - AITD2+
└─ SDL Audio - Modern systems
```

**Music Management**:
- Load/unload music tracks
- Fade in/out
- Cross-fading between tracks
- Looping and sequencing

---

### 7. **File I/O & Resource Loading**

**Files**: `hqr.cpp`, `pak.cpp`, `fileAccess.cpp`, `hdArchive.cpp`

**Resource Formats**:

**HQR Format**:
- Standard AITD resource container
- Indexed entries (bodies, animations, scripts, etc.)
- Used for LISTLIFE, HQ_Bodys, HQ_Anims, etc.

**PAK Format**:
- Packed data files
- Used for LISTLIFE.PAK (all life scripts)
- Binary data sections

**File Loading Flow**:
```
1. Check filesystem (D:\FITD\data\)
2. Check embedded resources (in binary)
3. Check HD archives
4. Fail with error if not found
```

**Error Handling**:
```cpp
// Files have error checking:
if (!ptr) {
    fclose(fHandle);
    fatalError(1, name);  // Fatal error, exit game
    return NULL;
}
```

---

### 8. **Rendering System**

**Files**: `renderer.cpp`, `rendererBGFX.cpp`, `bgfxGlue.cpp`, `hdBackground.cpp`, `hdBackgroundRenderer.cpp`, `sprite.cpp`, `zv.cpp`

**Rendering Pipeline**:
```
1. Clear screen
2. Render background (room geometry)
3. Render 3D objects (actors, items)
4. Render Z-buffer (depth sorting)
5. Render 2D overlays (UI, HUD)
6. Submit to BGFX
```

**Z-Buffer System** (zv.cpp):
- Depth sorting for rendering order
- Perspective correction
- Multi-object ordering

---

### 9. **Collision & Physics**

**Files**: `aitdBox.cpp`, `floor.cpp`, `room.cpp`

**Collision Detection**:
- AABB (axis-aligned bounding box) tests
- Floor collision
- Object-to-object collision
- Results stored in `COL[]` array

---

### 10. **Game-Specific Code**

**Files**: `AITD1.cpp`, `AITD2.cpp`, `AITD3.cpp`, `JACK.cpp`

**Purpose**: Game-specific initialization and constants
- Game ID detection
- Asset paths
- Game-specific opcodes
- Version-specific behavior

**Game IDs**:
```cpp
enum GameID {
    AITD1,   // Alone in the Dark 1
    AITD2,   // Alone in the Dark 2
    AITD3,   // Alone in the Dark 3
    JACK     // Jack in the Dark (unsupported in AITD2+)
};
extern GameID g_gameId;
```

---

## Data Flow Diagrams

### Main Update Loop
```
Frame Start
    ↓
Process Input (input.cpp)
    ↓
Update Game State:
    ├─ Process life scripts (life.cpp via processLife)
    ├─ Update animations (anim.cpp)
    ├─ Update positions/collisions
    └─ Update audio (music.cpp)
    ↓
Render Frame:
    ├─ Render background
    ├─ Render objects (sorted by Z)
    ├─ Render UI overlays
    └─ Submit to BGFX
    ↓
Frame End
```

### Script Execution Path
```
Game Loop wants to run actor's life script
    ↓
processLife(lifeNum)
    ├─ Check for native C version
    │  ├─ YES: Call nativeLifeScript_N()
    │  │        ├─ Reset currentLifePtr to nullptr (BUG FIX)
    │  │        ├─ Call 97 helper functions
    │  │        └─ Restore currentLifePtr
    │  │
    │  └─ NO: Execute bytecode interpreter
    │         ├─ Load script from LISTLIFE.PAK
    │         ├─ Execute bytecode opcodes
    │         └─ Call same 97 helper functions
    ↓
Results stored in actor state
    ├─ Animation changes
    ├─ Position changes
    ├─ Collision results
    └─ Found object triggers
```

---

## Critical Bug Fixes Applied

### 1. currentLifePtr Stale Data Bug ✅ FIXED
**Location**: life.cpp, main.cpp
**Issue**: Global pointer not reset during native execution
**Impact**: Corrupted nested script execution
**Fix**: Reset to nullptr before/after native function calls

---

## Code Patterns & Conventions

### Actor Processing
```cpp
// Standard pattern for processing all actors
for (int i = 0; i < NUM_MAX_OBJECT; i++) {
    tObject* actor = &ListObjets[i];
    if (actor->indexInWorld == -1) continue;  // Skip inactive
    
    // Set as current actor
    currentProcessedActorPtr = actor;
    currentProcessedActorIdx = i;
    
    // Process actor...
    processLife(actor->lifeNum, true);
}
```

### Resource Loading with Error Checking
```cpp
// Pattern used throughout
sBody* body = HQR_Get(HQ_Bodys, bodyID);
if (!body) {
    consoleLog("Error loading body %d", bodyID);
    return -1;
}
```

---

## Performance Considerations

### Native Script Optimization
- Bytecode execution: ~100 cycles per opcode
- Native C execution: ~10 cycles per operation
- 562 scripts × 60 FPS = significant savings with native path

### Memory Layout
- Actors stored in contiguous array (cache-friendly)
- HQR resources loaded on-demand
- HD textures cached in VRAM

---

## Future Improvements

1. **Documentation**
   - Add function-level documentation to all APIs
   - Create design documents for complex systems

2. **Code Quality**
   - Add more error checking to file I/O
   - Add logging for debugging

3. **Performance**
   - Profile memory allocations
   - Optimize hot paths

4. **Maintainability**
   - Split large files for readability
   - Create helper modules for common patterns

