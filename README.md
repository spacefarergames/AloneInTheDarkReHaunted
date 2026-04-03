![BCO b91ab279-5d06-4e0e-9661-41d0371e3ac4(1)](https://github.com/user-attachments/assets/867772e8-ada8-41a1-a9f6-46f80fda76be)

# Alone In The Dark: Re-Haunted

**A faithful remaster of the original 1992 survival horror classic.**

*Copyright © 2026 Infogrames / Spacefarer Retro Remasters LLC*
*Author: Jake Jackson (jake@spacefarergames.com)*

**A cross-platform engine reimplementation for the classic *Alone in the Dark* trilogy (1992–1995), with modern remaster enhancements.**

AITD-R (also known as *Alone In The Dark Re-Haunted*) lets you play the original *Alone in the Dark* games on modern hardware. It is a from-scratch C++ reimplementation of the Infogrames engine, built with modern rendering (bgfx), audio (SoLoud), and input (SDL3) backends. The project is released under the **GNU General Public License v2**.

> **You must own the original game data** — purchase *Alone in the Dark* on [Steam](https://store.steampowered.com/app/548090/Alone_in_the_Dark_1/) or [GOG](https://www.gog.com/en/game/alone_in_the_dark_the_trilogy_123). The game data files are **not** included.

---

## Screenshots
<img width="1386" height="884" alt="image" src="https://github.com/user-attachments/assets/7d49eb5a-8d31-4474-a939-ff9876fdc9df" />
<img width="982" height="706" alt="image" src="https://github.com/user-attachments/assets/b0206b5f-8026-46d0-89a6-b5dc6b7ba73e" />
---

## Supported Games

| Game | Steam | GOG | Status |
|------|-------|-----|--------|
| Alone in the Dark 1 | [Store page](https://store.steampowered.com/app/548090/Alone_in_the_Dark_1/) | [Trilogy](https://www.gog.com/en/game/alone_in_the_dark_the_trilogy_123) | ✅ Completable

Future Rehaunted projects. AKA Jack Is Back (Again!) and AITD3 are coming soon.

---

## Quick Start (Windows)

```bash
# 1. Clone with submodules
git clone --recurse-submodules https://github.com/spacefarergames/AloneInTheDarkReHaunted.git
cd FITD

# 2. Generate the VS solution
build\vs2022.bat          # Visual Studio 2022
# — or —
build\vs2026.bat          # Visual Studio 2026
```

In Visual Studio:

1. Set **Fitd** as the startup project.
2. Set the **Working Directory** (Project → Properties → Debugging) to the folder containing your game data files (e.g. your AITD1 Steam install directory).
3. Press **F5** to build and run.

> The output executable is named `Tatou.exe`.

For full multi-platform build instructions see **[BUILDING.md](BUILDING.md)**.

---

## How to Play

### Starting a New Game

1. Launch `Tatou.exe` from the game data directory (the folder containing the original `.PAK` / `.ITD` files).
2. The engine auto-detects which game is installed based on the data files present.
3. For AITD1: after the Infogrames logo and armadillo animation, select your character — **Edward Carnby** (detective) or **Emily Hartwood** (heiress).
4. The game begins in the attic of Derceto Manor.

### Gameplay Basics

*Alone in the Dark* is a survival-horror adventure. You explore a haunted mansion, solve puzzles, and fight (or flee from) supernatural enemies.

- **Movement** — Walk forward, backward, and turn left/right (tank controls).
- **Action** — Press the action key/button near objects to interact: open doors, pick up items, push objects, and fight enemies.
- **Inventory** — Press **F1** (or access via the menu) to open your inventory. Select items to use, equip, or examine them. Combine items by placing them in your "hand" slot.
- **Combat** — Equip a weapon from inventory, then use the action key with directional input to punch, kick, slash, or shoot. Some enemies can only be defeated with specific items or approaches.
- **Running** — Hold forward and press the action key to run (useful for dodging enemies).
- **Quick Turn** — Press **Q** or **E** (or shoulder buttons on a controller) to perform a quick 180° turn.

### Saving and Loading

- Press **Escape** to open the **System Menu** during gameplay.
- Select **Save** to write your progress to a save slot.
- Select **Load** to restore a previous save.
- Settings (fullscreen, sound, detail level, hints, controls) are also accessible from this menu and are saved automatically when the menu closes.

### Tips

- Search everything — many items are hidden in drawers, cabinets, and on shelves.
- Read all books and letters — they contain crucial puzzle hints and story background.
- Not every enemy needs to be fought — sometimes running or pushing furniture to block a doorway is the best strategy.
- Save often — Derceto is unforgiving.

---

## Controls

### Keyboard (Default)

| Action | Key | Description |
|--------|-----|-------------|
| Move Forward | **↑** (Up Arrow) | Walk forward |
| Move Backward | **↓** (Down Arrow) | Walk backward |
| Turn Left | **←** (Left Arrow) | Turn left |
| Turn Right | **→** (Right Arrow) | Turn right |
| Action / Fight | **Space** | Interact with objects, attack in combat |
| Confirm / Enter | **Enter** | Confirm menu selections |
| Cancel / Menu | **Escape** | Open system menu, cancel dialogs |
| Quick Turn Left | **Q** | 180° turn to the left |
| Quick Turn Right | **E** | 180° turn to the right |
| Fullscreen Toggle | **F11** or **Alt+Enter** | Toggle fullscreen / windowed mode |
| Fullscreen Toggle | **Double-click** | Double-click the window to toggle fullscreen |

> All keyboard bindings are fully rebindable via the **Controls** option in the in-game system menu, or by editing `aitd_remaster.cfg`.

### Gamepad (Default — Xbox Layout)

| Action | Button | PlayStation Equivalent |
|--------|--------|-----------------------|
| Move | **Left Stick** / **D-Pad** | Left Stick / D-Pad |
| Action / Fight | **A** | **✕ (Cross)** |
| Confirm / Enter | **Start** | **Options** |
| Cancel / Menu | **B** | **○ (Circle)** |
| Quick Turn Left | **LB** | **L1** |
| Quick Turn Right | **RB** | **R1** |

- Controllers are hot-pluggable — connect or disconnect at any time.
- Analog stick deadzone and sensitivity are configurable in `aitd_remaster.cfg`.
- All gamepad bindings are rebindable via the **Controls** menu.

### In-Game System Menu

Press **Escape** during gameplay to access:

| Option | Description |
|--------|-------------|
| Continue | Return to the game |
| Save | Save your game to a slot |
| Load | Load a saved game |
| Music On/Off | Toggle ADLIB / external music |
| Sound On/Off | Toggle sound effects |
| Detail Low/High | Toggle between original graphics and HD remaster mode |
| Display: Windowed/Fullscreen | Toggle fullscreen mode (persists across sessions) |
| Controls | Rebind keyboard and gamepad controls |
| Hints: On/Off | Toggle interactive object hints |
| Quit | Quit to the main menu |

---

## Remaster Features

The *Re-Haunted* fork adds several enhancements on top of the original FITD engine:

| Feature | Status | Details |
|---------|--------|---------|
| **HD backgrounds** | ✅ Available | Upscaled camera views (2×–8×) with PNG/TGA support, including animated backgrounds |
| **Textured 3D models** | ✅ Available | High-quality textured replacements for core 3D models via texture atlas |
| **TTF font rendering** | ✅ Available | Smooth anti-aliased overlay fonts via ImGui (configurable font, size, and style) |
| **Post-processing** | ✅ Available | Bloom, film grain, SSAO, vignette, SSGI, light probes |
| **Controller support** | ✅ Available | Xbox, PlayStation, Switch Pro, and other SDL3-compatible gamepads with rebindable controls |
| **Fullscreen mode** | ✅ Available | Toggle via F11, Alt+Enter, double-click, or system menu; persists in config |
| **In-game maps** | ✅ Available | Interactive mansion and underground maps with real-time position tracking |
| **Interactive hints** | ✅ Available | Highlights interactable objects in the game world |
| **Voice-over playback** | ✅ Available | CD voice-over for AITD1 book/letter reading sequences |
| **HD depth masks** | ✅ Available | Hand-edited masks for correct 3D object occlusion with HD backgrounds |
| **Atmospheric particles** | ✅ Available | Dust mote particles in the attic (floor 7) |
| **Transparent menus** | ✅ Available | Blurred, semi-transparent system menu overlays |
| **External music** | 🚧 In progress | OGG/MP3 track replacement via SoLoud |
| **Crash recovery** | ✅ Available | Exception handler with crash logging to `crash_log.txt` |
| **Auto-update checker** | ✅ Available | Non-blocking check for new GitHub releases at startup |

All remaster features are configurable through `aitd_remaster.cfg` — see [`fitd_remaster.cfg.example`](fitd_remaster.cfg.example) for the full reference. Detailed documentation is available in [`REMASTER.md`](REMASTER.md).

---

## Configuration

Copy `fitd_remaster.cfg.example` to `aitd_remaster.cfg` alongside the game data and edit to taste. Key sections:

| Section | Key Settings |
|---------|-------------|
| **Controller** | `controller.enable`, `controller.deadzone`, `controller.sensitivity`, `controller.invertY` |
| **Graphics** | `graphics.hdBackgrounds`, `graphics.backgroundScale`, `graphics.filtering`, `graphics.fullscreen` |
| **Post-processing** | `postprocessing.bloom`, `postprocessing.filmGrain`, `postprocessing.ssao`, `postprocessing.vignette`, `postprocessing.ssgi`, `postprocessing.lightProbes` |
| **Music** | `music.external`, `music.folder` |
| **Font** | `font.enableTTF`, `font.path`, `font.size`, `font.hideOriginal` |
| **Controls** | `controls.key.*`, `controls.pad.*` — per-action keyboard scancode and gamepad button bindings |
| **Gameplay** | `gameplay.hints` — interactive hint overlay |
| **Masks** | `masks.dump`, `masks.load` — HD depth mask dumping and loading |

---

## Project Layout

```
FITD/
├── Fitd/                  # Executable entry point (WinMain / main)
├── FitdLib/               # Core engine static library (~75 source files)
│   ├── shaders/           # bgfx shader programs (.sc)
│   └── embedded/          # Embedded game data (PAK files, textures, etc.)
├── ThirdParty/            # Git submodules
│   ├── bgfx.cmake         # Cross-platform rendering (bgfx + bimg + bx)
│   ├── SDL/               # SDL3 — windowing, input, audio
│   ├── soloud.cmake       # SoLoud audio library
│   ├── imgui/             # Dear ImGui (debug UI, TTF font overlay)
│   └── zlib/              # Compression (HQR/PAK archives)
├── tools/                 # Build tools (HDA archive builder, etc.)
├── build/                 # Generated build directories
├── .github/workflows/     # CI (CMake multi-platform)
├── CMakeLists.txt         # Root CMake project
├── README.md              # This file
├── BUILDING.md            # Detailed build instructions
├── ARCHITECTURE.md        # Codebase architecture guide
├── REMASTER.md            # Remaster feature documentation
├── CONTRIBUTING.md        # Contributor guidelines
├── RELEASE_NOTES.md       # Version history and changelogs
├── TTF_FONT_README.md     # TTF font feature documentation
└── LICENSE                # GNU General Public License v2
```

For a deeper dive into the code modules and data flow, see **[ARCHITECTURE.md](ARCHITECTURE.md)**.

---

## Contributing

Contributions are welcome! Please see **[CONTRIBUTING.md](CONTRIBUTING.md)** for guidelines on setting up a development environment, coding standards, and the pull request workflow.

---

## License

This project is licensed under the **GNU General Public License v2** — see the [LICENSE](LICENSE) file for details.

The original game data files are **not** included and must be obtained separately (e.g., from Steam or GOG).

## Steam Assets
Box Art
<img width="1024" height="1536" alt="image" src="https://github.com/user-attachments/assets/62b94c3b-7fd4-4b7d-8f81-f504382f798d" />
Logo
<img width="1536" height="1024" alt="Copilot_20260403_111155" src="https://github.com/user-attachments/assets/ad5073b8-d9b0-4d57-a2a8-13f63bf98bfc" />




