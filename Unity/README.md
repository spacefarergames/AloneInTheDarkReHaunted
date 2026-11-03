# FITD inside Unity

The native plugin keeps portable C++ gameplay. Unity owns graphics resources and draw commands, audio output, keyboard/mouse/controller access, and packaged content. The default plugin creates no native window or graphics device. Native shader names select the converted Unity shaders in `Runtime/Resources/FitdShaders`.

## Windows build

With Visual Studio 2026 C++ tools, CMake, and Unity installed:

```powershell
./Unity/build.ps1 -GameData D:/FITD/Shipping -UnityEditor 'F:/Unity Editors/6000.4.5f1/Editor/Unity.exe'
```

This builds the x64 gameplay library, stages the runtime/editor scripts into `Unity/Project`, and imports game content into target-specific AssetBundles. `-ProjectPath` selects another project. Close Unity before replacing a loaded native library. `-SkipBuild` stages an existing library. Without `-GameData`, content is not rebuilt.

Open the project and select **FITD > Create Player Scene**, save the scene, and press Play. Include that scene in the player build. The build validator requires a matching native plugin and bundle catalog.

## Linux and macOS

Run `bash Unity/build_native.sh /absolute/path/to/UnityProject` on the target OS with CMake, Python 3, and a C++17 compiler installed. macOS builds both arm64 and x86_64 and requires Xcode command-line tools. The script stages `libFitdUnity.so` or `libFitdUnity.dylib`; Unity configures the platform-specific plugin importer.

Select the target in Unity, then use **FITD > Content > Import and Build Bundles for Active Platform**. Install that platform's Unity build support first. Command-line equivalent:

```sh
Unity -batchmode -nographics -quit -projectPath /path/to/project -buildTarget Linux64 \
  -executeMethod Fitd.Unity.FitdBundleBuilder.BuildCommandLine \
  -fitdData /path/to/game -fitdGame aitd1 -fitdTarget StandaloneLinux64
```

For macOS use `-buildTarget OSXUniversal -fitdTarget StandaloneOSX`; Windows uses `Win64` and `StandaloneWindows64`. Bundle outputs are in `Assets/StreamingAssets/Fitd/<target>`. Each platform needs its own native library and bundles. Android, iOS, consoles, and WebGL are not currently supported player targets.

The game presentation stretches to cover the complete window or screen, with input coordinates mapped through that same viewport. The main camera clears solid black. The Windows build defaults to borderless fullscreen at the native display resolution.

## Content and saves

The importer reads HDA archives and loose replacements, imports Unity textures/audio/fonts, and packs original engine inputs into bounded byte chunks. A catalog records paths, offsets, lengths, and SHA-256 hashes. Native decoders read the chunks on demand through a 128 MiB RAM cache. The player does not extract bundled inputs to disk. Content ships in bundles; players do not require original loose data beside the executable or a special working directory.

Saves/config changes go into a separate `FitdSaves/<game>` directory. The default Unity plugin excludes compiled-in original game data.

C++ still decodes engine textures and mixes PCM; Unity uploads/draws textures and pulls mixed PCM directly from `OnAudioFilterRead`. There is no one-second streaming clip or audio read-ahead queue. The player requests 256-sample DSP blocks when supported; the actual configured buffer size is logged. The callback is allocation-free apart from native pin-handle bookkeeping and never waits for native audio initialization/shutdown. Unity owns the audio device. Typed bundle assets are also available through `FitdBundleStore.Load<T>` for Unity systems.

## Input and lifecycle

WASD/arrows move, Shift runs, Space acts, Enter confirms, Escape opens/cancels menus, and Tab opens inventory. F1 toggles remaster options. Native custom keyboard bindings use the same USB scancode format. Short press edges are forwarded even when the key/button is released within one input update. Stick values bypass Unity's deadzone processor so the native configurable deadzone is applied once. The player requests at least 120 Hz for polled devices and restores the previous polling frequency when disabled. The setup installs Unity's Input System; enable **Input System Package** or **Both** in Player Settings > Active Input Handling for controller forwarding and its configurable native bindings. Legacy Unity input supports keyboard/mouse. Device polling runs in `MonoBehaviour.Update` after the Input System dynamic update; the Editor render fallback never reads the separate Editor input buffer. Native bindings initialize even though Unity owns the devices. `FitdPlayer.SubmitInput` supports scripted or touch controls when `automaticInput` is disabled.

In a built player, native engine Quit/error closes Unity, and Unity shutdown stops and joins the native thread. Disabling the standalone host also stops the engine. In the Editor, native Quit ends Play mode; closing the Editor stops the engine. Leaving Play mode normally pauses the native session; reentering resumes across domain reloads. One player owns the process-wide engine. After a native Quit/error, restart the Editor for a fresh session. Application shutdown joins the native game thread.

## Editor performance

Unity builds omit atmospheric dust and dirt particles, including initialization, updates, and draw calls.

The command renderer pools meshes and materials, caches unchanged render state/submeshes, uploads packed vertex/index buffers in bulk, and sends only uniforms used by each shader. A pinned packet buffer avoids marshalling the large asset-upload buffer on every frame. Native gameplay waits for each command packet to be consumed before sampling the next frame, avoiding an extra frame of queued input. The Unity plugin disables the standalone engine frame limiter; `FitdPlayer.targetFrameRate` controls its requested rate. In Play mode an Editor update hook also pumps packets and repaints existing Game views when another tab has focus.

`FitdPlayer` exposes rendered frame counts, render CPU time, and draw counts. The gameplay smoke test logs both Editor update frequency and native work/wait time, so background Editor throttling can be distinguished from rendering cost. Batch-mode timing is not a substitute for profiling a visible Game view on the target machine. In automated Windows checks, packet transfer fell from about 322 ms to 0.53 ms and render CPU time fell from 78.6 ms to roughly 7�8 ms. The Editor gameplay test reached about 49 FPS. With the 2.5.0.0110 bundles and low-latency frame synchronization, the standalone Windows benchmark reached 59.71 FPS in the first room, with 1.63 ms render CPU time and 4.04 ms native work per frame. The audio callback used 256-sample blocks at 48 kHz (21.33 ms of configured buffering) and peaked at 0.63 ms in that run; physical device latency was not measured. Performance depends on the target machine and scene; this is not a guarantee for every system.

Local desktop bundles load directly from disk. The Unity main thread services bounded read requests from the native decoders.

TTF files load through the same bundled-content path resolver as gameplay data, including case-insensitive lookup and Unicode paths. Font cache reuse checks the current atlas, configured path, and size. Font atlas changes requested during a frame are deferred until the next frame boundary.

## Verification tools

For input tests, copy `FitdInputSmoke.cs` into the test project's `Assets` (it contains a runtime MonoBehaviour) and the other smoke helpers into `Assets/Editor`. Add `-fitdInput` to the gameplay smoke command. The test injects virtual Unity keyboard/gamepad devices and verifies the actions consumed by C++ gameplay. `FitdFacingSmoke` checks clockwise front-face visibility and the native front-CCW override on the GPU. `FitdMaskSmoke` checks transparent/opaque mask coverage and actual native mask bindings; `FitdPresentationSmoke` checks the full-screen viewport and black camera clear.

`Unity/Tests` contains Unity batch checks for shader compilation, typed bundles and native bundle pages, gameplay rendering, and Play/domain reload resume. `FitdDepthSmoke` and its `FitdDepthProbe.shader` verify Unity depth clears and occlusion ordering; copy both into the test project's `Assets/Editor`. The standalone `Unity/Native` CMake project tests command packet serialization without a graphics device. `Unity/tools/convert_shaders.py` regenerates the checked-in Unity shader translations after engine shader changes.

Validation on Windows with Unity 6000.4.5f1 includes all 24 shader programs, typed bundle loading and native bundle pages, the complete 7,319-asset/seven-bundle game, first-room gameplay, and Play/domain reload resume. The Linux x86_64 plugin also cross-compiles successfully; Linux player execution and macOS builds still need validation on those platforms.

The legacy `FITD_UNITY_COMMANDS=OFF` Windows-only readback host remains optional for comparison. It is not the default deployment path.

## Release 2.5.0.0110 content

The Windows bundles use the assets from `F:/Spacefarer Bakery Retro Remasters Releases/Tatou 1 Shipping/2.5.0.0110`. Its 38 original files are extracted from `Tatou.exe` by `tools/extract_release_data.py` without executing it. Candidate lengths from reference data or generated array sizes are checked against the release executable's compiled size constants; all payload bytes come from that executable. `release-provenance.json` in the extraction output records source and payload SHA-256 hashes. The newer release's archives and loose replacements remain authoritative.

`build.ps1 -GameData <release folder>` detects this layout and prepares originals automatically. `-BaseData <folder>` selects already-extracted originals. For direct Unity bundle commands, use `-fitdBaseData <folder>` alongside `-fitdData <release folder>`. Existing save/config overrides are retained in the separate writable save directory.

Test-only `FitdLifecycleSmoke` verifies both shutdown directions in a player. `FitdAudioSmoke` verifies live DSP callbacks and reports buffered audio time and callback cost. Copy the lifecycle helper into the validation project's `Assets`, not the shipping project.

## Unity settings overlay

F1 or Home opens the animated Unity UI Toolkit overlay. The game smoothly shrinks to a scene preview and pauses; closing restores the full display. Escape returns without discarding live changes; **Save & return** persists them. **Reload saved** and **Defaults** also update the preview. Categories use vector icons, keyboard navigation, and controller button mappings.

The overlay exposes native settings and bindings, font and music paths, plus Unity master volume, interface scale, and reduced motion. Fullscreen applies to standalone players; its control is disabled in the Editor. Diagnostic settings that take effect on the next launch say so. Native graphics backend, native MSAA/validation, external upscaler generation are omitted because those controls do not apply to the Unity host.

Settings are queued and applied at a native game-thread frame boundary. Save/reload/default acknowledgements are published with the resulting snapshot, so UI callbacks never mutate gameplay configuration concurrently. Settings metadata and native accessors share `Settings/settings.json`; regenerate both with `python Unity/tools/generate_settings.py` after editing the schema. UI resources live in `Package/Runtime/Resources/FitdUI` and are included in the player.

`Tests/FitdSettingsSmoke.cs` verifies UI changes reach native configuration, saved settings reload, defaults restore, bindings apply, animations reverse smoothly, and closing restores fullscreen presentation. Copy the test into a validation project's `Assets/Editor`, then run `-batchmode -force-d3d11 -executeMethod FitdSettingsSmoke.Run`.


The Game mode tab defaults to AITD1 and offers JACK when that platform's JACK catalog is installed. The startup overlay selects the game before its native session is created. In a running game, save a mode change with **Save & return** to apply it at the next launch. **Skip settings at startup** bypasses the startup screen. F1 still opens settings. Render scale changes the native render target live and saves with the other host settings.

To build the JACK catalog, run `python Unity/tools/extract_jack_data.py build/jack-bundle-source` (optionally pass `--release` with the remaster asset directory), then build bundles with `-fitdGame jack -fitdData /absolute/path/to/build/jack-bundle-source`. JACK uses a separate import directory, bundle directory and save directory.
