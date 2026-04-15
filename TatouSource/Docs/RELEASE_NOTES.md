# Alone In The Dark: Re-Haunted — Release Notes

## ⚠️ Version 2.0 - Pre-Release (Release Candidate)

**Status:** This is a pre-release build. The following items are still in progress before the final 2.0 release:
- **Texture Atlas Updates**: Continuing to update and finalize all texture atlases (many currently incomplete)
- **HD Mask Refinements**: HD masks have been updated for correct geometry; further refinements needed for complete accuracy
- **Additional Polish**: Final quality assurance and optimization passes

Final release expected upon completion of these remaining items.

---

## Version 2.0 - Enhanced Graphics, Animation Overhaul & Quality Improvements

### Overview
Version 2.0 represents a **major transformation** for Alone in the Dark: Re-Haunted! This release delivers comprehensive improvements to textured models, completely overhauled animation systems, revolutionary UI rendering enhancements, and extensive quality-of-life improvements. With all 29+ GitHub issues addressed, this version modernizes the visual presentation while maintaining the authentic, immersive gameplay experience of the original AITD1.

### New Features & Improvements

#### Graphics & Visual Enhancements
- **Textured 3D Models**: Complete replacement of core 3D models with high-quality textured versions for improved visual fidelity
- **HD Background Support**: Integrated HD replacement textures for UI screens with proper fallback rendering:
  - Intro screens (TITRE, LIVRE) with seamless fade-in/fade-out
  - Character selection backgrounds with proper portrait overlay handling
  - Book/letter/notebook reading interfaces
  - Seamless blending of HD backgrounds with UI elements
  - Multi-language "Please Wait..." loading screen support
- **Animation System Overhaul**: Major rewrite of animation handling system including:
  - Optimized animation data structures for better performance and reduced memory footprint
  - Support for hybrid animation techniques (combining traditional and optimized approaches)
  - Improved 2D animation playback for consistency
  - Per-floor animation caching to prevent incorrect model loading between different areas
  - Fixed Jack animations (#612b702)
  - Proper animation cleanup between floors to avoid taking the wrong animation
  - Early hybrid animation support
- **Body & ZV System Refactoring**: Comprehensive modernization of body and ZV (zone volume) rendering systems:
  - Complete rewrite of body storage and organization
  - Switch to proper body system for ZV code integration
  - Improved collision detection and rendering
  - Better model organization and lookup efficiency
- **UI Text Rendering**: Implemented text rendering directly on top of game world for seamless in-game UI integration
- **Enhanced UI Layer**: Improved UI layer rendering system for better menu and overlay presentation with proper transparency handling
- **Transparent Menus & UI**: Revolutionary transparent UI elements that blend seamlessly with the game world, reducing visual obstruction and improving immersion

#### Gameplay & Content
- **In-Game Maps**: Added interactive Mansion Map and Underground Map for improved navigation and exploration (#36)
  - Access maps from the pause menu
  - Real-time position tracking
  - Full room/area labeling for easier navigation
- **Collision Volume Display**: New developer feature to visualize collision volumes for actors without explicit body definitions (aids in debugging and development) (#43)
- **HQR System Modernization**: Complete rework of HQR (FITD archive format) handling:
  - Redesigned body/animation storage structures
  - Improved data access patterns and performance
  - Better template instantiation
- **Model Organization**: Major cleanup and renaming of model components for better code organization
- **Environment Fixes**:
  - Fixed Rocking Horse intersecting inside pillar in attic (#32)
  - Fixed wood texture in attic (#25)
  - Fixed dirt path in intro appearing as wood (#21)
  - Improved background rendering quality and upscaling (#29, #30)
- **Enhanced Loading Experience**: Improved "Please Wait..." loading screen with multi-language support (English, French, Italian, Spanish, German)

#### Bug Fixes & Stability
- **Animation System Fixes**:
  - Fixed multiple animation-related regressions from earlier versions
  - Animation playback consistency across all game areas
  - Proper animation cleanup between floors
  - Jack animation rendering corrections
- **Door Functionality**: Corrected non-functional door issues in AITD1 (regression fix)
- **UI Layer Regressions**: Fixed several UI layer rendering issues affecting menu appearance (#26)
- **Camera System**: Fixed potential out-of-bounds access and camera index issues
- **Memory Safety**: 
  - Improved boundary checking throughout the codebase
  - Resolved major SafeDelete issues (#37)
  - Better resource deallocation
- **Rendering Quality**:
  - Fixed stencil overdraw issues (#24)
  - Fixed framebuffer preview rendering in pause/save menus (#30)
  - Improved transparency support for UI overlays (#26)
- **Cross-Platform Support**: Resolved Linux compilation issues and ensured cross-platform compatibility
- **Steam Overlay Integration**: Enhanced compatibility with Steam overlay features (#40)

#### Performance Optimizations
- Optimized animation data structures for reduced memory footprint
- Improved rendering pipeline efficiency with better layer management
- Enhanced animation loading and caching mechanisms
- More efficient body storage and lookup
- Better template instantiation for faster compilation

### Technical Details

#### Modified Core Systems
- **Animation System** (`anim.cpp`, `anim.h`): Complete rewrite with optimized structures, hybrid support, and per-floor caching
- **Body System** (`body.cpp`, `body.h`): Refactored for better model organization and rendering with new storage structures
- **ZV System**: Updated to use proper body integration with improved collision handling
- **UI Layer** (`osystemAL.cpp`): Enhanced rendering with proper text layer support and HD background blending
- **HQR Archives** (`hqr.cpp`): Modernized data structures and access patterns for improved performance
- **Life Script System** (`life.cpp`): Updated to work with new animation and body systems
- **HD Background Renderer** (`hdBackgroundRenderer.cpp`): Integrated texture rendering pipeline for enhanced graphics
- **Voice-Over System**: Full support for AITD1 CD voice-over playback during reading sequences

#### Build & Compatibility
- Fixed CMake configuration for proper Linux support
- Improved cross-platform compilation and template instantiation
- Enhanced third-party library integration (SDL3, SoLoud, bgfx, etc.)
- Better project organization and filter structure

### Known Issues Addressed in This Release
- Door activation in AITD1 now functions correctly
- Animation playback is consistent across all game areas and floors
- UI text renders properly over game world without visual artifacts
- Collision detection functions properly with new body system
- Character selection screen displays correct portrait overlays with HD backgrounds
- Loading screen appears immediately when character is selected
- Camera system no longer causes out-of-bounds access
- Linux compilation errors fully resolved
- Rocking Horse collision fixed in attic (#32)
- Wood texture in attic corrected (#25)
- Stencil overdraw issues resolved (#24)
- UI framebuffer preview rendering improved (#30)
- SafeDelete resource handling stabilized (#37)
- Near-Camera Floating Polygon Clipping fixed (#43)
- Texture Wrap / Shading support added (#42)
- NUM_MAX_SPHERES_VERTICES limit resolved (#41)
- Mask dumping and fixup completed (#44)
- Ghost/orb particles now use transparent material (#39)
- Sound system fully functional (#38)
- Mansion Map and Underground Map added (#36)
- Interactive object hint overlay implemented (#35)
- Fullscreen toggle in system menu added (#34)
- Shadow noise feature restored and improved (#27)
- 3D objects now properly render when in UI mode (#28)
- Environment upscaling and rendering refinements completed
- Dirt path in intro no longer appears as wood (#21)
- Background rendering quality improved (#29)
- Transparency support added to UI overlays (BigCadre/Frames) (#26)
- Heiress voice pitch corrected on playback (#31)
- Demonic ashtray smoke handled correctly (#20)
- Particles and opaque shapes rendering fixed (#19)
- Dark room lighting properly applied (#18)

### Upgrade Notes
- **This is a MAJOR upgrade** - Version 2.0 represents the most comprehensive update to Re-Haunted since launch, with 29+ improvements across all systems
- Players should expect dramatically improved visual quality with the new textured models, transparent menus, and HD backgrounds
- New in-game maps (Mansion & Underground) provide a fresh gameplay experience and significantly improve navigation
- Loading times may vary as animation caching optimizations take effect on first playthrough
- Saved games from version 1.x are fully compatible with version 2.0
- Some screen layouts have been optimized for better HD background integration and transparency support
- Animation playback performance has been significantly improved due to per-floor caching
- Prepare for an enhanced, modernized AITD1 experience!

---

## Voice-Over Playback for AITD1 CD Edition

### Overview
Added full support for CD voice-over (VOC) playback during book, letter, and notebook reading in AITD1. When the original AITD1 CD-ROM is present (or VOC files are packed into the audio archive), spoken dialogue is now played back automatically as the player reads text, with each page of text accompanied by its corresponding voice-over audio.

### Features

#### Per-Page Voice-Over Concatenation
- VOC audio files are organized per text line using the **BBSSLL** naming convention:
  - **BB** = book number (00–19)
  - **SS** = page number within the book
  - **LL** = line number within the page
- When a page is displayed, all per-line VOC files for that page are loaded, decoded from Creative Voice File format to PCM, concatenated into a single audio stream, and played back seamlessly via SoLoud.

#### Multi-Source VOC File Loading
VOC files are searched in priority order:
1. **Audio archive** (`audio.hda`) — for packaged distributions
2. **CD-ROM drive** — auto-detected ALONECD volume (`\INDARK\` directory)
3. **Local filesystem** — working directory fallback

#### Life Script Integration
- The `LM_READ` life script macro now reads a third parameter (AITD1 only): the VOC book number, which is passed through `readBook()` → `AITD1_ReadBook()` → `Lire()`.
- The `LM_READ_ON_PICTURE` macro similarly passes its VOC index to `Lire()`.
- All three book types (letter, book, notebook) support voice-over playback.

#### Page-Turn Sound Effect
- The page-turn sound (`SAMPLE_PAGE`) now plays when flipping pages forward or backward during normal book reading (not just intro/demo modes), provided voice-over is active for the current text.

#### Character Selection Screen Voice-Over
- The heiress (Emily Hartwood) and detective (Edward Carnby) character intro readings on the selection screen now play the correct voice-over audio.
- **Fixed:** The original implementation incorrectly used CVar text resource indices as VOC book numbers. The heiress CVar value (20) exceeded the valid book range (0–19), producing a "No VOC lines found for book 20" error.
- **Resolution:** Analysis of the 1,439 VOC files on the AITD1 CD confirmed the correct mapping:
  - **Carnby (detective)** → VOC Book 0
  - **Emily (heiress)** → VOC Book 1

#### Automatic Cleanup
- Voice-over playback is stopped automatically when the player exits the reading screen (via Escape or mouse click).

### Technical Details

#### New Functions
| Function | File | Purpose |
|---|---|---|
| `osystem_playVocPageLines()` | `osystemAL.cpp` | Load, decode, concatenate, and play all line VOCs for a given page |
| `loadVocFileData()` | `osystemAL.cpp` | Search archive → CD → filesystem for raw VOC file data |

#### Modified Functions
| Function | File | Change |
|---|---|---|
| `Lire()` | `main.cpp` | Added `vocIndex` parameter (default `-1`); added `vocLinesOnPage` counter; triggers per-page VOC playback and page-turn SFX |
| `readBook()` | `main.cpp` | Passes `vocIndex` through to game-specific ReadBook handlers |
| `AITD1_ReadBook()` | `AITD1.cpp` | Passes `vocIndex` to `Lire()` for all book types |
| `ChoosePerso()` | `AITD1.cpp` | Uses correct VOC book numbers (0, 1) instead of CVar text indices |
| `LM_READ` handler | `life.cpp` | Reads VOC book number as 3rd s16 parameter (AITD1 only) |
| `LM_READ_ON_PICTURE` handler | `life.cpp` | Reads and passes VOC book number to `Lire()` |

#### Modified Headers
| Header | Change |
|---|---|
| `main.h` | `Lire()` declaration updated with `vocIndex = -1` default parameter |
| `osystem.h` | Added `osystem_playVocPageLines()` declaration |

### VOC File Format Reference
The AITD1 CD contains 1,439 VOC files across 20 books (numbered 0–19), stored in the `\INDARK\` directory. Each file follows the Creative Voice File format (26-byte header, block-based with types 0/1/2/9, 8-bit unsigned PCM). Files are decoded to WAV in memory using the existing `vocDecodeToWav()` decoder before playback.
