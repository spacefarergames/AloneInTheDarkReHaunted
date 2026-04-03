# Alone In The Dark: Re-Haunted — Remaster Features

This document covers all the enhancements added by the Re-Haunted fork on top of the original FITD engine.

---

## Controller Support

### Overview
Re-Haunted supports modern game controllers through SDL3's gamepad API. This includes Xbox controllers, PlayStation controllers, Nintendo Switch Pro controllers, and any other controller supported by SDL3.

### Features
- **Analog Stick Movement**: Use the left analog stick for smooth character movement
- **D-Pad Support**: Traditional D-pad input is fully supported
- **Button Mapping**: Intuitive button layout, fully rebindable via the in-game Controls menu
- **Hot-Plugging**: Controllers can be connected/disconnected during gameplay
- **Configurable Settings**: Customize deadzone, sensitivity, and axis inversion

### Default Button Mapping

#### Movement
- **Left Analog Stick**: Character movement (analog)
- **D-Pad**: Character movement (digital)

#### Actions  
- **A Button** (Xbox) / **Cross** (PlayStation): Action/Confirm/Click
- **B Button** (Xbox) / **Circle** (PlayStation): Cancel/Escape
- **Start Button**: Enter/Menu select
- **LB / RB** (Xbox) / **L1 / R1** (PlayStation): Quick turn left / right

### Configuration

Controller settings in `aitd_remaster.cfg`:

```ini
controller.enable = true          # Enable/disable controller support
controller.deadzone = 0.15        # Stick deadzone (0.0–1.0)
controller.sensitivity = 1.0      # Stick sensitivity (0.5–2.0)
controller.invertY = false        # Invert Y-axis
controller.analogMovement = true  # Enable analog stick movement
```

### Troubleshooting

- **Controller not detected?** — Ensure your controller is connected before starting the game, or reconnect it (hot-plug is supported). Check that SDL3 supports your controller type.
- **Stick drift?** — Increase `controller.deadzone` to 0.20–0.25.
- **Movement too sensitive?** — Reduce `controller.sensitivity` to 0.7–0.9.

### Technical Details

- SDL3 gamepad subsystem with configurable deadzone and sensitivity
- Digital threshold for analog-to-digital conversion: 0.3
- Frame-based input polling
- Three input modes: Analog (smooth), Digital (8-direction), Hybrid (both simultaneously)

---

## Fullscreen Mode

Toggle fullscreen at any time using:

- **F11** key
- **Alt + Enter**
- **Double-click** the game window
- **Display** option in the in-game system menu (Escape → Display: Windowed/Fullscreen)

The fullscreen setting is persisted in `aitd_remaster.cfg` as `graphics.fullscreen`. When the game starts, it reads this setting and automatically enters fullscreen if enabled. The game window is also raised to the foreground at startup to ensure the console window does not cover it.

```ini
graphics.fullscreen = true   # Start in fullscreen mode
```

---

## HD Graphics

### HD Backgrounds
- Load upscaled camera backgrounds at 2×, 4×, or 8× resolution (PNG/TGA format via bimg/stb_image)
- Animated HD backgrounds supported (e.g., flickering lights, moving elements)
- Automatic fallback to original PAK images when HD versions are unavailable
- Toggle between original and HD graphics via the **Detail** option in the system menu

### HD Depth Masks
- Hand-edited masks for correct 3D object occlusion with HD backgrounds
- Load from `masks_hd/` directory (enabled by default)
- Optional mask dumping for artists: set `masks.dump = true` to export generated masks as PNG

### Textured 3D Models
- High-quality textured replacements for core 3D models via texture atlas system
- Per-floor animation caching prevents incorrect model loading between game areas

### Configuration

```ini
graphics.hdBackgrounds = true     # Enable HD backgrounds
graphics.backgroundScale = 2     # Upscale factor (2, 4, or 8)
graphics.filtering = true        # Enable texture filtering
graphics.useArtwork = true       # Use replacement artwork assets

masks.dump = false                # Dump generated masks to PNG for editing
masks.load = true                 # Load hand-edited HD masks
```

---

## Post-Processing Effects

All post-processing effects are configurable and can be toggled independently.

### Bloom
Multi-pass bloom with configurable threshold, intensity, and number of passes.

```ini
postprocessing.bloom = true
postprocessing.bloomThreshold = 0.45
postprocessing.bloomIntensity = 0.55
postprocessing.bloomPasses = 2
```

### Film Grain
Subtle film grain overlay for a cinematic look.

```ini
postprocessing.filmGrain = true
postprocessing.filmGrainIntensity = 0.025
```

### Screen-Space Ambient Occlusion (SSAO)
Adds depth-aware shadow contact to corners and crevices.

```ini
postprocessing.ssao = true
postprocessing.ssaoRadius = 400.0
postprocessing.ssaoIntensity = 0.8
```

### Vignette
Darkens the edges of the screen for a horror/cinematic effect.

```ini
postprocessing.vignette = false
postprocessing.vignetteIntensity = 0.35
postprocessing.vignetteRadius = 0.75
```

### Screen-Space Global Illumination (SSGI)
Approximates indirect lighting by sampling nearby surfaces in screen space. Renders at half resolution for performance.

```ini
postprocessing.ssgi = false
postprocessing.ssgiRadius = 300.0
postprocessing.ssgiIntensity = 0.6
postprocessing.ssgiNumSamples = 16    # 8–32
```

### Light Probes
Provides ambient lighting using Spherical Harmonics at strategic world positions. Loaded per-floor and per-camera.

```ini
postprocessing.lightProbes = false
postprocessing.lightProbeIntensity = 0.5
```

---

## TTF Font Rendering

Smooth TrueType font overlay on top of the original bitmap fonts, rendered via ImGui with anti-aliasing.

```ini
font.enableTTF = true
font.path = "BLKCHCRY.TTF"    # Gothic font (place in game directory)
font.size = 16                 # Font size in pixels (14–18 recommended)
font.hideOriginal = true       # Hide original bitmap font when TTF is active
```

Recommended fonts: **BLKCHCRY.TTF** (Black Chancery), **MORPHEUS.TTF**, or any gothic/horror TTF. See [TTF_FONT_README.md](TTF_FONT_README.md) for details.

---

## Transparent Menus

The system menu can use a blurred, semi-transparent overlay instead of opaque frames:

```ini
graphics.blurredMenu = true
graphics.menuBlurAmount = 5.0   # Blur strength (higher = darker)
```

---

## In-Game Maps

Interactive maps accessible from the system menu (press **Tab** / **Select** during the pause menu):

- **Mansion Map** — All floors of Derceto Manor
- **Underground Map** — Caverns and underground areas
- Real-time position tracking
- Full room and area labelling

---

## Interactive Hints

When enabled, interactable objects in the game world are highlighted:

```ini
gameplay.hints = true
```

Toggle via the **Hints** option in the system menu.

---

## Atmospheric Particles

Dust mote particles float in the attic (floor 7) for atmospheric effect. The system uses up to 150 particles with individual physics, size, transparency, and lifetime.

---

## Voice-Over Playback (AITD1 CD Edition)

Full support for CD voice-over during book, letter, and notebook reading in AITD1:

- Per-page VOC audio concatenation and playback via SoLoud
- VOC files are searched in priority order: HDA archive → CD-ROM drive (ALONECD volume) → local filesystem
- Automatic cleanup when exiting reading screens
- Page-turn sound effects during voiced reading

---

## External Music (In Progress)

Play custom music tracks instead of the original ADLIB music:

```ini
music.external = false
music.folder = "music"     # Folder containing MUSIC_XX.ogg or MUSIC_XX.mp3
```

---

## Key Bindings

All keyboard and gamepad bindings are rebindable via the in-game **Controls** menu, or by editing `aitd_remaster.cfg`:

```ini
# Keyboard (SDL scancodes)
controls.key.up = 82
controls.key.down = 81
controls.key.left = 80
controls.key.right = 79
controls.key.action = 44          # Space
controls.key.confirm = 40         # Enter
controls.key.cancel = 41          # Escape
controls.key.quickturnleft = 20   # Q
controls.key.quickturnright = 8   # E

# Gamepad (SDL_GamepadButton values)
controls.pad.up = 11
controls.pad.down = 12
controls.pad.left = 13
controls.pad.right = 14
controls.pad.action = 0           # A / Cross
controls.pad.confirm = 6          # Start
controls.pad.cancel = 1           # B / Circle
controls.pad.quickturnleft = 9    # LB / L1
controls.pad.quickturnright = 10  # RB / R1
```

---

## Crash Recovery

On Windows, an exception handler catches unhandled crashes and writes detailed logs to `crash_log.txt`. The game attempts to continue after non-fatal exceptions.

---

## Auto-Update Checker

At startup, a non-blocking background thread checks GitHub for newer releases. If a new version is available, a notification is printed to the console.

---

## Development

### Building
Remaster features are automatically included in every build — no special flags needed:

```bash
cmake --build . --config Release
```

### Configuration System
- `configRemaster.h` — `RemasterConfig` struct definition
- `configRemaster.cpp` — Config file parser and writer (`loadRemasterConfig()` / `saveRemasterConfig()`)
- `aitd_remaster.cfg` — User configuration file (created automatically on first save)

### Adding New Controller Mappings
Edit `controlsMenu.cpp` to add new actions, or use the in-game Controls menu for runtime rebinding.

---

## Credits

**Original Game**: Alone in the Dark (Infogrames, 1992)  
**Original FITD Engine**: yaz0r  
**Re-Haunted Remaster**: Spacefarer Retro Remasters LLC  
**Controller / Input**: SDL3 Gamepad API  
**Audio**: SoLoud  
**Rendering**: bgfx  
**UI**: Dear ImGui

---

## License

This is a fan project. All original game assets remain property of their respective copyright holders.
