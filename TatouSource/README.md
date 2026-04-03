# FITD — Free In The Dark

**A cross-platform engine reimplementation for the classic *Alone in the Dark* trilogy (1992–1995).**

FITD (also known as *Alone In The Dark Re-Haunted*) lets you play the original *Alone in the Dark* games on modern hardware. It is a from-scratch C++ reimplementation of the Infogrames engine, built with modern rendering (bgfx), audio (SoLoud), and input (SDL3) backends. The project is released under the **GNU General Public License v2**.

---

## Supported Games

| Game | Steam | GOG | Status |
|------|-------|-----|--------|
| Alone in the Dark 1 | [Store page](https://store.steampowered.com/app/548090/Alone_in_the_Dark_1/) | [Trilogy](https://www.gog.com/en/game/alone_in_the_dark_the_trilogy_123) | Completable (some missing features) |
| Alone in the Dark 2 | [Store page](https://store.steampowered.com/app/548890/Alone_in_the_Dark_2/) | [Trilogy](https://www.gog.com/en/game/alone_in_the_dark_the_trilogy_123) | In progress |
| Alone in the Dark 3 | [Store page](https://store.steampowered.com/app/548900/Alone_in_the_Dark_3/) | [Trilogy](https://www.gog.com/en/game/alone_in_the_dark_the_trilogy_123) | In progress |
| Jack in the Dark | Included with AITD1 | Included with Trilogy | Completable |
| Time Gate: Knight's Chase | [Store page](https://store.steampowered.com/app/781280/Time_Gate_Knights_Chase/) | [Store page](https://www.gog.com/en/game/time_gate_knights_chase) | Very early / experimental |

> **Note:** The focus is on the DOS versions currently sold on Steam and GOG. Some other releases (e.g., the Windows version of AITD3) are not yet compatible.

---

## Screenshots

![image](https://github.com/user-attachments/assets/d12b7c66-6c57-4507-b2b3-540cb2cc6806)
![image](https://github.com/user-attachments/assets/ba4508ba-1b7f-4ac1-b573-169e3f8e7bab)
![image](https://github.com/user-attachments/assets/653d3dc5-4a35-4a7f-95bc-76e7b42899d9)
![image](https://github.com/user-attachments/assets/310c3151-18cf-4914-8992-7de1dd763653)

---

## Quick Start (Windows / Visual Studio 2022)

```bash
# 1. Clone with submodules
git clone --recurse-submodules https://github.com/<your-org>/FITD.git
cd FITD

# 2. Generate the VS solution via the helper script
build\vs2022.bat

# 3. Open the solution
start build\vs2022\FITD.sln
```

In Visual Studio:
1. Set **Fitd** as the startup project.
2. Set the working directory to the folder containing your game data files (e.g. your AITD1 install).
3. Press **F5** to build and run.

For full multi-platform build instructions see **[BUILDING.md](BUILDING.md)**.

---

## Remaster Features

The *Re-Haunted* fork adds several enhancements on top of the original FITD engine:

| Feature | Status | Details |
|---------|--------|---------|
| **Controller support** | ✅ Available | Xbox, PlayStation, Switch Pro, and other SDL3-compatible gamepads |
| **TTF font rendering** | ✅ Available | Smooth anti-aliased overlay fonts via ImGui |
| **Post-processing effects** | ✅ Available | Bloom, film grain, SSAO |
| **HD backgrounds** | 🚧 In progress | Upscaled camera views (2×–8×) with PNG/TGA support |
| **External music** | 🚧 In progress | OGG/MP3 track replacement via SoLoud |

All remaster features are configurable through `fitd_remaster.cfg` — see [`fitd_remaster.cfg.example`](fitd_remaster.cfg.example) for the full reference, and [`REMASTER.md`](REMASTER.md) for detailed documentation.

---

## Project Layout

```
FITD/
├── Fitd/               # Executable entry point (WinMain / main)
├── FitdLib/            # Core engine static library (~140 source files)
│   ├── shaders/        # bgfx shader programs (.sc)
│   └── embedded/       # Embedded game data (PAK files, textures, etc.)
├── ThirdParty/         # Git submodules
│   ├── bgfx.cmake      # Cross-platform rendering (bgfx + bimg + bx)
│   ├── SDL/             # SDL3 — windowing, input, audio
│   ├── soloud.cmake     # SoLoud audio library
│   ├── imgui/           # Dear ImGui (debug UI, TTF font overlay)
│   └── zlib/            # Compression (HQR/PAK archives)
├── build/              # Generated build directories
├── .github/workflows/  # CI (CMake multi-platform)
├── CMakeLists.txt      # Root CMake project
├── BUILDING.md         # Detailed build instructions
├── ARCHITECTURE.md     # Codebase architecture guide
├── REMASTER.md         # Remaster feature documentation
└── TTF_FONT_README.md  # TTF font feature documentation
```

For a deeper dive into the code modules and data flow, see **[ARCHITECTURE.md](ARCHITECTURE.md)**.

---

## Configuration

Copy `fitd_remaster.cfg.example` to `fitd_remaster.cfg` alongside the game data and edit to taste. Key sections:

- **Controller** — deadzone, sensitivity, Y-axis inversion
- **Graphics** — HD backgrounds, texture filtering
- **Post-processing** — bloom, film grain, SSAO
- **Music** — external OGG/MP3 music tracks
- **Font** — TTF overlay font path and size

---

## Contributing

Contributions are welcome! Please see **[CONTRIBUTING.md](CONTRIBUTING.md)** for guidelines on setting up a development environment, coding standards, and the pull request workflow.

---

## License

This project is licensed under the **GNU General Public License v2** — see the [LICENSE](LICENSE) file for details.

The original game data files are **not** included and must be obtained separately (e.g., from Steam or GOG).

