# Building FITD

This document covers how to build FITD on every supported platform.

---

## Prerequisites (All Platforms)

| Requirement | Minimum Version | Notes |
|-------------|----------------|-------|
| **Git** | 2.x | Must support `--recurse-submodules` |
| **CMake** | 3.9+ | Included with Visual Studio on Windows |
| **C++17 compiler** | See per-platform sections | MSVC, GCC, or Clang |

Clone the repository **with submodules** — several third-party libraries (bgfx, SDL3, SoLoud, ImGui, zlib) are pulled in as Git submodules:

```bash
git clone --recurse-submodules https://github.com/<your-org>/FITD.git
cd FITD
```

If you already cloned without `--recurse-submodules`, run:

```bash
git submodule update --init --recursive
```

---

## Windows

### Option A — Visual Studio 2022 (recommended)

1. Install **Visual Studio 2022** with the *Desktop development with C++* workload and the **CMake tools for Windows** component.
2. Run the helper batch file:

   ```cmd
   build\vs2022.bat
   ```

   This locates the VS2022 installation via `vswhere`, configures the environment, and generates a Visual Studio 17 (2022) solution in `build/vs2022/`.

3. Open `build\vs2022\FITD.sln`.
4. Set **Fitd** as the startup project.
5. Set the **Working Directory** (Project Properties → Debugging → Working Directory) to the folder containing your game data (e.g. your AITD1 Steam install directory).
6. Select a build configuration (**Debug** or **Release**) and press **F5**.

> The output executable is named `Tatou.exe`.

### Option B — Visual Studio 2026 Preview

A `build\vs2026.bat` script is also provided. It works identically but targets Visual Studio 18 (2026).

### Option C — CMake command-line (any generator)

```cmd
mkdir build\custom && cd build\custom
cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release ..\..
cmake --build . --config Release --target Fitd
```

You may substitute any CMake generator (`"NMake Makefiles"`, `"MinGW Makefiles"`, etc.).

---

## Linux

### 1. Install dependencies

On Debian / Ubuntu:

```bash
sudo apt update
sudo apt install \
  build-essential git cmake ninja-build pkg-config \
  libx11-dev libopengl-dev libglx-dev mesa-common-dev \
  libasound2-dev libpulse-dev libaudio-dev libjack-dev libsndio-dev \
  libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev \
  libxss-dev libxtst-dev libxkbcommon-dev libdrm-dev libgbm-dev \
  libgl1-mesa-dev libgles2-mesa-dev libegl1-mesa-dev \
  libdbus-1-dev libibus-1.0-dev libudev-dev \
  libpipewire-0.3-dev libwayland-dev libdecor-0-dev liburing-dev
```

On Fedora:

```bash
sudo dnf groupinstall "Development Tools"
sudo dnf install cmake ninja-build mesa-libGL-devel mesa-libEGL-devel \
  libX11-devel libXrandr-devel libXcursor-devel libXi-devel \
  pulseaudio-libs-devel alsa-lib-devel dbus-devel \
  wayland-devel libdecor-devel libxkbcommon-devel systemd-devel
```

### 2. Configure and build

```bash
mkdir -p build/linux && cd build/linux
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ../..
cmake --build . --target Fitd
```

### 3. Run

```bash
cd /path/to/game-data
/path/to/FITD/build/linux/Fitd/Tatou
```

---

## macOS (experimental)

> macOS support compiles but is less tested than Windows and Linux.

### 1. Install tools

```bash
xcode-select --install          # Apple Clang
brew install cmake ninja        # via Homebrew
```

### 2. Build

```bash
mkdir -p build/macos && cd build/macos
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ../..
cmake --build . --target Fitd
```

The CMake configuration automatically includes the Objective-C++ patch file (`bgfxPatch.mm`) on Darwin.

---

## Build Configurations

| Configuration | Console Window | Optimisation | Debug Symbols | Notes |
|---------------|---------------|--------------|---------------|-------|
| **Debug** | Shown | Off | Full | Default for development |
| **Release** | Hidden (Win) | Full | None | For distribution |
| **RelWithDebInfo** | Hidden (Win) | Full | Full | Profiling builds |
| **MinSizeRel** | Hidden (Win) | Size | None | Minimal binary size |

---

## Address Sanitizer

To enable ASan (and UBSan / LeakSan on non-MSVC), uncomment the `USE_SANITIZER` line in the root `CMakeLists.txt`:

```cmake
set(USE_SANITIZER ON)
```

Or pass it on the command line:

```bash
cmake -DUSE_SANITIZER=ON ...
```

---

## Continuous Integration

The project includes a GitHub Actions workflow (`.github/workflows/cmake-multi-platform.yml`) that builds on:

- **Windows** (latest MSVC) — Debug + Release
- **Ubuntu** (GCC and Clang) — Debug + Release

---

## Troubleshooting

| Problem | Solution |
|---------|----------|
| **Submodule directories are empty** | Run `git submodule update --init --recursive` |
| **`vs2022.bat` can't find Visual Studio** | Ensure VS2022 is installed with the C++ workload; `vswhere.exe` must be at `%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\` |
| **Missing OpenGL headers on Linux** | Install `libopengl-dev libglx-dev mesa-common-dev` |
| **Runtime: game data not found** | Set the working directory to the folder containing the game's original data files |
| **Runtime: controller not detected** | Ensure `controller.enable = true` in `fitd_remaster.cfg` and that SDL3 supports your gamepad |
