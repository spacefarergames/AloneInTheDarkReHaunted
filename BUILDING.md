# Building Alone In The Dark Re-Haunted for Linux

## Prerequisites

Install the required build dependencies:

```bash
# Ubuntu/Debian
sudo apt update
sudo apt install -y \
    build-essential \
    cmake \
    pkg-config \
    libx11-dev \
    libxext-dev \
    libxrandr-dev \
    libxinerama-dev \
    libxcursor-dev \
    libxi-dev \
    libgl-dev \
    libglu1-mesa-dev \
    libasound2-dev \
    libpulse-dev \
    libwayland-dev \
    libxkbcommon-dev \
    libpipewire-0.3-dev

# Fedora
sudo dnf install -y \
    gcc-c++ \
    cmake \
    pkgconfig \
    libX11-devel \
    libXext-devel \
    libXrandr-devel \
    libXinerama-devel \
    libXcursor-devel \
    libXi-devel \
    mesa-libGL-devel \
    mesa-libGLU-devel \
    alsa-lib-devel \
    pulseaudio-libs-devel \
    wayland-devel \
    libxkbcommon-devel \
    pipewire-devel

# Arch Linux
sudo pacman -S --needed \
    base-devel \
    cmake \
    pkgconf \
    libx11 \
    libxext \
    libxrandr \
    libxinerama \
    libxcursor \
    libxi \
    mesa \
    glu \
    alsa-lib \
    libpulse \
    wayland \
    libxkbcommon \
    pipewire
```

## Building

### Option 1: Using the build script

```bash
chmod +x build_linux.sh
./build_linux.sh
```

### Option 2: Manual CMake build

```bash
# Create build directory
mkdir -p build/linux
cd build/linux

# Configure
cmake ../.. -DCMAKE_BUILD_TYPE=Release

# Build (use -j for parallel compilation)
cmake --build . --config Release -j$(nproc)
```

The executable will be at: `build/linux/Fitd/Tatou`

## Running

### Option 1: Using the launch script

Copy `launch_tatou.sh` to the same directory as the `Tatou` binary:

```bash
cp launch_tatou.sh build/linux/Fitd/
cd build/linux/Fitd
chmod +x launch_tatou.sh
./launch_tatou.sh
```

### Option 2: Direct execution

```bash
cd build/linux/Fitd
./Tatou
```

**Important:** Run from the directory containing the game data files (PAK files, atlases, etc.).

## Game Data

The game requires the original Alone In The Dark data files. Place these in the working directory:
- `*.PAK` files (ENGLISH.PAK, ITD_RESS.PAK, etc.)
- `atlases/` directory (HD texture atlases)
- `backgrounds_hd/` directory (HD backgrounds)
- `audio.hda` (audio archive)

## Troubleshooting

### PipeWire warnings
```
can't load config client.conf: No such file or directory
```
This is harmless - audio will still work via PulseAudio/ALSA fallback.

### Missing libGL
```bash
sudo apt install libgl1-mesa-dev
```

### X11 errors
Ensure you have a display server running (X11 or Wayland with XWayland).

### Permission denied
```bash
chmod +x build/linux/Fitd/Tatou
```

## Cross-compiling from Windows (WSL)

You can build the Linux version from Windows using WSL:

```powershell
# In PowerShell
wsl -d Ubuntu -- bash -c "cd /mnt/d/FITD && mkdir -p build/linux && cd build/linux && cmake ../.. -DCMAKE_BUILD_TYPE=Release && cmake --build . -j4"
```

## Build Output

| File | Description |
|------|-------------|
| `build/linux/Fitd/Tatou` | Main game executable |
| `build/linux/FitdLib/libFitdLib.a` | Static game library |


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

### Option B — Visual Studio 2026

A `build\vs2026.bat` script is also provided. It works identically but targets Visual Studio 18 (2026):

```cmd
build\vs2026.bat
start build\vs2026\FITD.sln
```

Follow steps 3–6 from Option A above.

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
| **Runtime: controller not detected** | Ensure `controller.enable = true` in `aitd_remaster.cfg` and that SDL3 supports your gamepad |
| **Runtime: fullscreen not persisting** | Ensure `graphics.fullscreen = true` is in your `aitd_remaster.cfg`; the setting is saved automatically when you close the system menu |
| **Runtime: console window covers the game** | The game window is automatically raised to the foreground at startup; press **F11** or **Alt+Enter** to go fullscreen |
