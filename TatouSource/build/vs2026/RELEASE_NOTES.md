# Release Notes — Alone In The Dark Re-Haunted

## Audio: VOC Decoder & LIFE Script Sound Playback

### New: Creative Voice File (.VOC) Decoder
- Added `vocDecodeToWav()` — a robust, standalone decoder for the Creative Voice File format used by all AITD-era sound effects.
- Handles VOC block types 0 (terminator), 1 (sound data), 2 (continuation), and 9 (new-format sound data), covering every VOC file shipped with the original games.
- Converts VOC data to standard WAV in memory for seamless playback through SoLoud.

### New: VOC-Aware Sample Playback
- `osystem_playSample()` now detects VOC-format data (via the `Creative Voice File\x1a` magic header) and routes it through the new `vocDecodeToWav()` decoder automatically.
- All existing LIFE script sample opcodes (`LM_SAMPLE`, `LM_SAMPLE_THEN`, `LM_SAMPLE_THEN_REPEAT`, `LM_ANIM_SAMPLE`) benefit from this without any changes to their handlers.
- Replaces the previous `ITD_AudioSource` inline VOC parser, which only supported type 1 blocks and asserted on anything else. The legacy path is retained as a fallback for non-VOC sample data.

### New: Looping Sample Support (LM_REP_SAMPLE)
- Implemented the `LM_REP_SAMPLE` LIFE script opcode, which was previously a stub.
- Added `osystem_playLoopingSample()` — a VOC-aware playback function that sets the SoLoud looping flag.
- Added `playSoundLooping()` wrapper in `tatou.cpp` mirroring the existing `playSound()` pattern.
- Correctly reads the sample number via `evalVar()` (AITD1/Time Gate) or direct `s16` (AITD2/3), and skips the frequency parameter.

### New: Voice-Over Support for LM_READ_ON_PICTURE
- For AITD1 CD, `LM_READ_ON_PICTURE` now reads an additional `vocIndex` from the LIFE script data.
- Calls `osystem_playVocByIndex()` before displaying the text and `osystem_stopVO()` after the player dismisses it, matching original CD behavior.

### Fixed: Single-Channel SFX (Matching Original DOS Behavior)
- **Bug:** The original single-handle tracking (`g_lastSfxHandle`) was replaced with a multi-sound `ActiveSfx` vector to allow overlapping sounds. However, LIFE script opcodes like `LM_ANIM_SAMPLE` fire `playSound()` every game tick while the actor is on a matching animation frame. The original DOS game had a single SFX channel where each new sound replaced the previous one, and LIFE scripts rely on this replacement behavior. With multi-sound, each tick created a new `Wav` instance — sounds accumulated indefinitely, becoming corrupted and never stopping.
- **Fix:** Both `osystem_playSample()` and `osystem_playLoopingSample()` now stop ALL active sounds before playing a new one, restoring the original DOS single-channel SFX behavior. The `ActiveSfx` vector is retained for clean memory management (tracking handles and source pointers for proper cleanup).
- `osystem_stopSample()` (for `LM_STOP_SAMPLE`) stops all active sounds.
- `ITD_AudioSource` copies sample data into an owned buffer (`m_ownedBuf`) instead of holding a raw pointer into HQR cache memory, preventing use-after-free if the cache evicts the entry during playback.
- Added explicit `setLooping(false)` calls in all `osystem_playSample()` code paths (VOC, Time Gate WAV, and legacy ITD) as a defensive measure.
### Files Changed
| File | Change |
|------|--------|
| `FitdLib/vocDecoder.cpp` | New — VOC-to-WAV decoder implementation |
| `FitdLib/vocDecoder.h` | New — `vocDecodeToWav()` declaration |
| `FitdLib/osystemAL.cpp` | VOC-aware `playSample`, new `playLoopingSample`, `ActiveSfx` vector system |
| `FitdLib/osystem.h` | Added `osystem_playLoopingSample()` declaration |
| `FitdLib/tatou.cpp` | Added `playSoundLooping()` wrapper |
| `FitdLib/tatou.h` | Added `playSoundLooping()` declaration |
| `FitdLib/life.cpp` | `LM_REP_SAMPLE` implementation, `LM_READ_ON_PICTURE` vocIndex support |