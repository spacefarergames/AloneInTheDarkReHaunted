# FITD Architecture Guide

This document describes the high-level architecture of the FITD (Free In The Dark / Re-Haunted) codebase. It is intended for new contributors who want to understand where things live and how the major subsystems interact.

---

## Overview

FITD is a **C++17** reimplementation of the engine used by the *Alone in the Dark* trilogy (1992–1995). The code is organised as a thin executable (`Fitd`) that calls into a large static library (`FitdLib`) containing all engine logic. Rendering, windowing, audio, and input are delegated to third-party libraries pulled in via Git submodules.

```
┌─────────────────────────────────────────────────┐
│                    Fitd (EXE)                   │
│        WinMain / main  →  FitdInit / FitdMain   │
└──────────────────────┬──────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────┐
│                  FitdLib (Static Lib)            │
│                                                  │
│  ┌──────────┐ ┌────────┐ ┌───────┐ ┌─────────┐ │
│  │Game Logic│ │Renderer│ │ Audio │ │  Input  │ │
│  │(Life,    │ │(bgfx,  │ │(SoLoud│ │(SDL3,   │ │
│  │ rooms,   │ │ shaders│ │ ADLIB)│ │ gamepad)│ │
│  │ objects) │ │ ImGui) │ │       │ │         │ │
│  └──────────┘ └────────┘ └───────┘ └─────────┘ │
│                                                  │
│  ┌──────────┐ ┌────────┐ ┌───────────────────┐ │
│  │ Resource │ │ Config │ │ Remaster Features │ │
│  │ (HQR/PAK│ │        │ │ (HD bg, TTF font, │ │
│  │  zlib)   │ │        │ │  post-processing) │ │
│  └──────────┘ └────────┘ └───────────────────┘ │
└──────────────────────────────────────────────────┘
         │            │             │
    ┌────▼───┐   ┌────▼───┐   ┌────▼────┐
    │  bgfx  │   │  SDL3  │   │ SoLoud  │
    │  bimg  │   │        │   │         │
    │  ImGui │   │        │   │         │
    └────────┘   └────────┘   └─────────┘
```

---

## CMake Build Targets

| Target | Type | Description |
|--------|------|-------------|
| `Fitd` | Executable | Entry point; links to `FitdLib` and all third-party libraries. Output binary: `Tatou.exe` |
| `FitdLib` | Static library | All engine code (~140 `.cpp` / `.h` files) |
| `bgfx` | Static library | Cross-platform GPU rendering (submodule: `ThirdParty/bgfx.cmake`) |
| `SDL3-static` | Static library | Windowing, input, platform abstraction (submodule: `ThirdParty/SDL`) |
| `soloud` | Static library | Audio mixing and playback (submodule: `ThirdParty/soloud.cmake`) |
| `zlibstatic` | Static library | Decompression for HQR/PAK archives (submodule: `ThirdParty/zlib`) |

ImGui sources are compiled directly into `FitdLib` (no separate target).

---

## FitdLib Module Map

The source files in `FitdLib/` can be grouped into the following logical modules:

### Core Engine

| File(s) | Responsibility |
|---------|---------------|
| `main.cpp` / `main.h` | Engine initialisation, camera, collision detection, scene orchestration |
| `mainLoop.cpp` / `mainLoop.h` | Primary game loop (`PlayWorld`) — processes one tick of game logic |
| `vars.cpp` / `vars.h` | Global variables, game-type enum (`AITD1`, `JACK`, `AITD2`, `AITD3`, `TIMEGATE`), core data structures |
| `common.h` / `config.h` | Shared includes, base type definitions |
| `baseTypes.h` / `endianess.h` | Portable integer types (`s16`, `u32`, etc.) and byte-order helpers |
| `version.h` / `version.cpp` | Build version string |
| `gameTime.cpp` / `gameTime.h` | Frame timing and game clock |

### Game Logic & Scripting

| File(s) | Responsibility |
|---------|---------------|
| `life.cpp` / `life.h` | **Life script interpreter** — the original game's scripting VM. Executes *Life* macros (`LM_DO_MOVE`, `LM_HIT`, `LM_CAMERA`, …) |
| `lifeMacroTable.cpp` | Life macro opcode table |
| `evalVar.cpp` / `evalVar.h` | Script variable evaluation |
| `AITD1.cpp` / `AITD2.cpp` / `AITD3.cpp` / `JACK.cpp` | Game-specific logic and quirk handling |
| `AITD1_Tatou.cpp` | AITD1 title-screen and menu flow |
| `anim.cpp` / `anim.h` | Skeletal animation processing |
| `anim2d.cpp` / `anim2d.h` | 2D sprite animation |
| `animAction.cpp` / `animAction.h` | Animation-triggered actions (sounds, hits, etc.) |
| `object.cpp` / `object.h` | Game object creation (`InitObjet`) |
| `actorList.cpp` / `actorList.h` | Active actor sorting and management |
| `track.cpp` / `track.h` | Path/track following for actor movement |

### World & Rooms

| File(s) | Responsibility |
|---------|---------------|
| `room.cpp` / `room.h` | Room data structures: collision hulls (`hardColStruct`), scene zones (`sceZoneStruct`), camera zones |
| `floor.cpp` / `floor.h` | Floor/stage loading (`LoadEtage`) |
| `zv.cpp` / `zv.h` | ZV (zone de vie / bounding volume) calculations |

### Rendering

| File(s) | Responsibility |
|---------|---------------|
| `renderer.cpp` / `renderer.h` | High-level 3D object rendering, point transformation, shadow drawing |
| `rendererBGFX.cpp` | bgfx-specific render submission |
| `bgfxGlue.cpp` / `bgfxGlue.h` | bgfx/SDL3 window initialisation, frame begin/end |
| `screen.cpp` / `screen.h` | Screen/framebuffer management |
| `videoMode.cpp` / `videoMode.h` | Resolution and display mode handling |
| `polys.cpp` | Polygon rasterisation |
| `lines.cpp` | Line drawing primitives |
| `sprite.cpp` / `sprite.h` | 2D sprite rendering |
| `palette.cpp` / `palette.h` | VGA palette management |
| `font.cpp` / `font.h` | Original bitmap font rendering |
| `debugFont.cpp` / `debugFont.h` | Debug overlay text |
| `sequence.cpp` / `sequence.h` | Full-motion video / cutscene playback |
| `shaders/` | bgfx shader programs (vertex/fragment/varying definitions) |

### Remaster Enhancements

| File(s) | Responsibility |
|---------|---------------|
| `configRemaster.cpp` / `configRemaster.h` | `RemasterConfig` struct and `fitd_remaster.cfg` parsing |
| `hdBackground.cpp` / `hdBackground.h` | HD background image loading (PNG/TGA via stb_image/bimg) |
| `hdBackgroundRenderer.cpp` / `hdBackgroundRenderer.h` | Submitting HD backgrounds to the GPU |
| `hdArchive.cpp` / `hdArchive.h` | HD asset archive access |
| `postProcessing.cpp` / `postProcessing.h` | Bloom, film grain, SSAO post-processing pipeline |
| `fontTTF.cpp` / `fontTTF.h` | TrueType font overlay via ImGui |
| `imguiBGFX.cpp` / `imguiBGFX.h` | ImGui ↔ bgfx integration |
| `updateChecker.cpp` / `updateChecker.h` | Online version/update check |

### Input

| File(s) | Responsibility |
|---------|---------------|
| `input.cpp` / `input.h` | SDL3 keyboard and gamepad polling, controller hotplug |
| `controlsMenu.cpp` / `controlsMenu.h` | In-game controls/key-binding menu |

### Audio

| File(s) | Responsibility |
|---------|---------------|
| `music.cpp` / `music.h` | Music playback orchestration, ADLIB/external track switching |
| `osystemAL.cpp` / `osystemAL.h` | ADLIB OPL2 emulation layer |
| `osystemAL_adlib.cpp` | ADLIB music driver |
| `osystemAL_mp3.cpp` / `osystemAL_mp3.h` | External MP3 music support |
| `fmopl.cpp` / `fmopl.h` | FM OPL emulator (Yamaha OPL2 chip) |
| `vocDecoder.cpp` / `vocDecoder.h` | Creative VOC audio format decoder |

### Resource / File I/O

| File(s) | Responsibility |
|---------|---------------|
| `hqr.cpp` / `hqr.h` | HQR archive reader (the original game's resource container format) |
| `pak.cpp` / `pak.h` | PAK archive reader |
| `fileAccess.cpp` / `fileAccess.h` | Cross-platform file access helpers |
| `unpack.cpp` / `unpack.h` | Custom decompression routines |
| `resourceGC.cpp` / `resourceGC.h` | Resource garbage collection / cache management |
| `save.cpp` / `save.h` | Save/load game state |

### Menus & UI

| File(s) | Responsibility |
|---------|---------------|
| `startupMenu.cpp` / `startupMenu.h` | Game selection / startup menu |
| `systemMenu.cpp` / `systemMenu.h` | In-game system/pause menu |
| `inventory.cpp` / `inventory.h` | Inventory management and display |
| `tatou.cpp` / `tatou.h` | Title screen and intro sequence |
| `aitdBox.cpp` / `aitdBox.h` | UI dialog boxes |
| `debugger.cpp` / `debugger.h` | Debug overlay / inspector |
| `consoleLog.h` | Console logging macros |

### Platform Abstraction

| File(s) | Responsibility |
|---------|---------------|
| `osystem.h` | Base OS system type definitions |
| `osystemSDL.cpp` | SDL3 platform backend |
| `exceptionHandler.cpp` / `exceptionHandler.h` | Crash/exception handling |

### Embedded Data

The `embedded/` subdirectory contains C++ arrays with embedded game data (camera definitions, language packs, floor layouts, textures) compiled directly into the binary. These are generated from original PAK files and allow the engine to function when certain original data files are missing or need overriding.

---

## Key Data Flow

### Startup Sequence

```
main() / WinMain()
  └─ FitdInit()         — parse args, detect game type
  └─ FitdMain()
       └─ initBgfxGlue()   — create SDL3 window, init bgfx
       └─ OpenProgram()     — load HQR resources, init subsystems
       └─ loadRemasterConfig() — read fitd_remaster.cfg
       └─ startGame()       — load initial floor/room
       └─ PlayWorld() loop  — main game loop
```

### Main Loop (one tick of `PlayWorld`)

1. **Input** — `readKeyboard()` / `updateController()` poll SDL3 events
2. **Scripting** — Life scripts are evaluated for all active actors
3. **Animation** — Skeletal and 2D animations advance
4. **Physics / Collision** — ZV intersection tests, actor-world and actor-actor
5. **Camera** — Camera zone evaluation, potential camera switch
6. **Render** — `StartFrame()` → background → 3D objects → sprites → UI overlay → post-processing → `EndFrame()`
7. **Audio** — `callMusicUpdate()` updates the audio stream

### Rendering Pipeline

```
StartFrame()
  ├─ Clear framebuffer
  ├─ Draw background (original or HD)
  ├─ Draw 3D actors (AffObjet → transform → submit to bgfx)
  ├─ Draw 2D sprites, text, UI overlays
  ├─ ImGui pass (TTF fonts, debug UI)
  ├─ Post-processing (bloom → film grain → SSAO → composite)
  └─ EndFrame() → bgfx::frame()
```

---

## Shader Programs

Shaders live in `FitdLib/shaders/` and are compiled via bgfx's `shaderc` into C headers at build time. Each shader program consists of a vertex shader (`*_vs.sc`), a fragment shader (`*_ps.sc`), and a varying definition (`*.varying.def.sc`).

| Program | Purpose |
|---------|---------|
| `ui` | 2D UI quads |
| `background` / `hdBackground` | Camera background rendering |
| `maskBackground` / `maskHDBackground` | Depth-masked background overlays |
| `flat` | Flat-shaded 3D geometry |
| `noise` / `selective_noise` | Noise/dither effects |
| `ramp` | Gradient ramp shading |
| `sphere` | Sphere-mapped lighting |
| `brightpass` / `blur` / `composite` | Bloom post-processing chain |
| `ssao` / `ssao_blur` | Screen-space ambient occlusion |

---

## Game-Specific Code

Each supported game has a dedicated source file (`AITD1.cpp`, `AITD2.cpp`, `AITD3.cpp`, `JACK.cpp`) containing game-specific:

- Initialisation and teardown
- Special-case logic (e.g., AITD1's intro cinematic flow in `AITD1_Tatou.cpp`)
- Workarounds for differences between game versions

The active game is identified at runtime by the `g_gameId` global (`gameTypeEnum` in `vars.h`), and dispatched to the appropriate handlers.

---

## Third-Party Libraries

| Library | Version | Purpose | Submodule Path |
|---------|---------|---------|---------------|
| **bgfx** (+ bimg, bx) | Latest | Cross-platform rendering (D3D11/12, Vulkan, Metal, OpenGL) | `ThirdParty/bgfx.cmake` |
| **SDL3** | Latest | Windowing, input, gamepad, platform abstraction | `ThirdParty/SDL` |
| **SoLoud** | Latest | Audio mixing, WAV/MP3/OGG playback | `ThirdParty/soloud.cmake` |
| **Dear ImGui** | Latest | Immediate-mode debug UI, TTF font rendering | `ThirdParty/imgui` |
| **zlib** | Latest | Decompression for HQR/PAK resource archives | `ThirdParty/zlib` |

---

## Configuration System

Runtime configuration is managed through `RemasterConfig` (defined in `configRemaster.h`). The struct contains nested sub-structs for each feature area:

- `controller` — deadzone, sensitivity, Y-inversion, analog movement toggle
- `graphics` — HD backgrounds, filtering, blurred menus, fullscreen, hints, artwork
- `postProcessing` — bloom, film grain, SSAO, vignette, SSGI, light probes
- `music` — external music enable and folder path
- `font` — TTF enable, font path, size, original text visibility
- `controls` — keyboard and gamepad key/button bindings (9 actions)
- `masks` — HD depth mask dumping and loading

Values are loaded from `aitd_remaster.cfg` at startup via `loadRemasterConfig()` and can be saved back with `saveRemasterConfig()`.
